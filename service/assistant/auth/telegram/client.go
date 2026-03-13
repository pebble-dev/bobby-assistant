// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package telegram

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/gotd/td/telegram"
	"github.com/gotd/td/telegram/message"
	"github.com/gotd/td/tg"
	"github.com/redis/go-redis/v9"
)

// TelegramClient wraps the gotd/td Telegram client with our session storage.
// It manages multiple user connections and provides methods for sending messages.
type TelegramClient struct {
	appID   int
	appHash string
	storage *SessionStorage
	redis   *redis.Client

	// Active client contexts for users
	clients    map[int]*userClient
	clientsMux sync.RWMutex
}

// userClient represents an active Telegram client for a specific user.
type userClient struct {
	client *telegram.Client
	cancel context.CancelFunc
	done   chan struct{}
}

// NewTelegramClient creates a new Telegram client wrapper.
func NewTelegramClient(appID int, appHash string, storage *SessionStorage, redisClient *redis.Client) *TelegramClient {
	return &TelegramClient{
		appID:   appID,
		appHash: appHash,
		storage: storage,
		redis:   redisClient,
		clients: make(map[int]*userClient),
	}
}

// sessionStorage implements telegram.SessionStorage interface for a specific user.
type sessionStorage struct {
	storage *SessionStorage
	userID  int
}

// LoadSession loads the session from Redis.
func (s *sessionStorage) LoadSession(ctx context.Context) ([]byte, error) {
	session, err := s.storage.GetSession(ctx, s.userID)
	if err == ErrSessionNotFound {
		return nil, nil // No session yet
	}
	if err != nil {
		return nil, err
	}
	return session.Session, nil
}

// StoreSession stores the session in Redis.
func (s *sessionStorage) StoreSession(ctx context.Context, data []byte) error {
	// Get existing session to preserve metadata
	existing, err := s.storage.GetSession(ctx, s.userID)
	if err != nil && err != ErrSessionNotFound {
		return err
	}

	now := time.Now()
	session := &SessionData{
		UserID:     s.userID,
		Session:    data,
		LastUsedAt: now,
	}

	if existing != nil {
		session.CreatedAt = existing.CreatedAt
		session.TelegramUserID = existing.TelegramUserID
		session.TelegramUsername = existing.TelegramUsername
		session.BotUsername = existing.BotUsername
	} else {
		session.CreatedAt = now
	}

	return s.storage.SaveSession(ctx, session)
}

// getOrCreateClient returns an existing client or creates a new one for the user.
// The client is started in a background goroutine and can be used for operations.
func (c *TelegramClient) getOrCreateClient(userID int) (*userClient, error) {
	c.clientsMux.RLock()
	uc, exists := c.clients[userID]
	c.clientsMux.RUnlock()

	if exists {
		return uc, nil
	}

	c.clientsMux.Lock()
	defer c.clientsMux.Unlock()

	// Double-check after acquiring write lock
	if uc, exists = c.clients[userID]; exists {
		return uc, nil
	}

	// Create new client with user-specific session storage
	sessionStore := &sessionStorage{
		storage: c.storage,
		userID:  userID,
	}

	client := telegram.NewClient(c.appID, c.appHash, telegram.Options{
		SessionStorage: sessionStore,
	})

	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan struct{})

	uc = &userClient{
		client: client,
		cancel: cancel,
		done:   done,
	}

	// Start the client in a background goroutine
	go func() {
		defer close(done)
		_ = client.Run(ctx, func(ctx context.Context) error {
			// Keep the connection alive until context is cancelled
			<-ctx.Done()
			return ctx.Err()
		})
	}()

	c.clients[userID] = uc
	return uc, nil
}

// RunClient starts a Telegram client for a user and runs a callback.
// This is the primary way to perform operations that require an active connection.
func (c *TelegramClient) RunClient(ctx context.Context, userID int, f func(ctx context.Context, api *tg.Client) error) error {
	uc, err := c.getOrCreateClient(userID)
	if err != nil {
		return err
	}

	// Run the callback with the client's API
	return uc.client.Run(ctx, func(ctx context.Context) error {
		return f(ctx, uc.client.API())
	})
}

// SendMessage sends a message to a bot on behalf of the user.
func (c *TelegramClient) SendMessage(ctx context.Context, userID int, botUsername string, text string) error {
	return c.RunClient(ctx, userID, func(ctx context.Context, api *tg.Client) error {
		// Resolve the bot username to get the peer
		resolved, err := api.ContactsResolveUsername(ctx, botUsername)
		if err != nil {
			return fmt.Errorf("failed to resolve bot username: %w", err)
		}

		// Find the bot in the resolved peers
		var inputPeer tg.InputPeerClass
		for _, user := range resolved.GetUsers() {
			if u, ok := user.(*tg.User); ok && u.Username == botUsername {
				inputPeer = &tg.InputPeerUser{
					UserID:     u.ID,
					AccessHash: u.AccessHash,
				}
				break
			}
		}

		if inputPeer == nil {
			// Fallback: use the first user found
			for _, user := range resolved.GetUsers() {
				if u, ok := user.(*tg.User); ok {
					inputPeer = &tg.InputPeerUser{
						UserID:     u.ID,
						AccessHash: u.AccessHash,
					}
					break
				}
			}
		}

		if inputPeer == nil {
			return fmt.Errorf("bot not found: %s", botUsername)
		}

		// Send the message using the sender helper
		sender := message.NewSender(api)
		_, err = sender.To(inputPeer).Text(ctx, text)
		return err
	})
}

// GetMessageHistory retrieves recent messages from a chat.
func (c *TelegramClient) GetMessageHistory(ctx context.Context, userID int, botUsername string, limit int) ([]Message, error) {
	var messages []Message

	err := c.RunClient(ctx, userID, func(ctx context.Context, api *tg.Client) error {
		// Resolve the bot username
		resolved, err := api.ContactsResolveUsername(ctx, botUsername)
		if err != nil {
			return fmt.Errorf("failed to resolve bot username: %w", err)
		}

		// Find the bot in the resolved peers
		var inputPeer tg.InputPeerClass
		for _, user := range resolved.GetUsers() {
			if u, ok := user.(*tg.User); ok {
				inputPeer = &tg.InputPeerUser{
					UserID:     u.ID,
					AccessHash: u.AccessHash,
				}
				break
			}
		}

		if inputPeer == nil {
			return fmt.Errorf("bot not found: %s", botUsername)
		}

		// Get message history
		result, err := api.MessagesGetHistory(ctx, &tg.MessagesGetHistoryRequest{
			Peer:  inputPeer,
			Limit: limit,
		})
		if err != nil {
			return fmt.Errorf("failed to get message history: %w", err)
		}

		// Convert to our Message type
		switch v := result.(type) {
		case *tg.MessagesChannelMessages:
			for _, msg := range v.Messages {
				if m, ok := msg.(*tg.Message); ok {
					messages = append(messages, c.convertMessage(m))
				}
			}
		case *tg.MessagesMessages:
			for _, msg := range v.Messages {
				if m, ok := msg.(*tg.Message); ok {
					messages = append(messages, c.convertMessage(m))
				}
			}
		case *tg.MessagesMessagesSlice:
			for _, msg := range v.Messages {
				if m, ok := msg.(*tg.Message); ok {
					messages = append(messages, c.convertMessage(m))
				}
			}
		}

		return nil
	})

	return messages, err
}

// convertMessage converts a tg.Message to our Message type.
func (c *TelegramClient) convertMessage(m *tg.Message) Message {
	msg := Message{
		ID:        m.ID,
		Text:       m.Message,
		Timestamp:  time.Unix(int64(m.Date), 0),
	}

	// Extract FromUserID
	if m.FromID != nil {
		if fromUser, ok := m.FromID.(*tg.PeerUser); ok {
			msg.FromUserID = fromUser.UserID
		}
	}

	// Extract ChatID
	if m.PeerID != nil {
		if peerUser, ok := m.PeerID.(*tg.PeerUser); ok {
			msg.ChatID = peerUser.UserID
		}
	}

	return msg
}

// Close stops all Telegram clients and releases resources.
func (c *TelegramClient) Close() error {
	c.clientsMux.Lock()
	defer c.clientsMux.Unlock()

	var lastErr error
	for userID, uc := range c.clients {
		uc.cancel() // Cancel the context to stop the client
		<-uc.done   // Wait for the client to stop
		delete(c.clients, userID)
	}

	return lastErr
}

// IsConnected checks if the user's Telegram client is active.
func (c *TelegramClient) IsConnected(userID int) bool {
	c.clientsMux.RLock()
	_, exists := c.clients[userID]
	c.clientsMux.RUnlock()
	return exists
}

// WaitForResponse waits for a new message from a specific bot after sending a message.
// It polls the message history and returns when a new message is detected.
func (c *TelegramClient) WaitForResponse(ctx context.Context, userID int, botUsername string, lastMessageID int, timeout time.Duration) (*Message, error) {
	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()

	ticker := time.NewTicker(500 * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return nil, ctx.Err()
		case <-ticker.C:
			messages, err := c.GetMessageHistory(ctx, userID, botUsername, 5)
			if err != nil {
				continue // Keep trying on error
			}

			// Look for new messages from the bot
			for _, msg := range messages {
				if msg.ID > lastMessageID && msg.FromUserID != 0 {
					// This is a message from the bot (not from us)
					return &msg, nil
				}
			}
		}
	}
}

// SendMessageAndWaitForResponse sends a message and waits for a response.
// Returns the bot's response message.
func (c *TelegramClient) SendMessageAndWaitForResponse(ctx context.Context, userID int, botUsername string, text string, timeout time.Duration) (*Message, error) {
	// Get the current message history to find the last message ID
	messages, err := c.GetMessageHistory(ctx, userID, botUsername, 1)
	if err != nil {
		// Continue anyway, might be a new chat
	}

	var lastMessageID int
	if len(messages) > 0 {
		lastMessageID = messages[0].ID
	}

	// Send the message
	if err := c.SendMessage(ctx, userID, botUsername, text); err != nil {
		return nil, fmt.Errorf("failed to send message: %w", err)
	}

	// Wait for response
	return c.WaitForResponse(ctx, userID, botUsername, lastMessageID, timeout)
}

// StopUserClient stops the Telegram client for a specific user.
func (c *TelegramClient) StopUserClient(userID int) {
	c.clientsMux.Lock()
	defer c.clientsMux.Unlock()

	if uc, exists := c.clients[userID]; exists {
		uc.cancel()
		<-uc.done
		delete(c.clients, userID)
	}
}