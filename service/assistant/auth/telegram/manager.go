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

	"github.com/redis/go-redis/v9"
)

// Manager handles Telegram connections for multiple users.
// It provides a centralized way to manage authentication and messaging.
type Manager struct {
	client       *TelegramClient
	loginManager *LoginManager
	storage      *SessionStorage
	redis        *redis.Client

	// Track active message streams
	streams   map[int]context.CancelFunc
	streamsMu sync.Mutex
}

// NewManager creates a new Telegram connection manager.
func NewManager(appID int, appHash string, encryptionKey string, redisClient *redis.Client) (*Manager, error) {
	storage, err := NewSessionStorage(redisClient, encryptionKey)
	if err != nil {
		return nil, fmt.Errorf("failed to create session storage: %w", err)
	}

	client := NewTelegramClient(appID, appHash, storage, redisClient)
	loginManager := NewLoginManager(client, storage, appID, appHash)

	return &Manager{
		client:       client,
		loginManager: loginManager,
		storage:      storage,
		redis:        redisClient,
		streams:      make(map[int]context.CancelFunc),
	}, nil
}

// StartLogin initiates a new login flow for a user.
func (m *Manager) StartLogin(ctx context.Context, userID int) (*LoginSession, error) {
	return m.loginManager.StartLogin(ctx, userID)
}

// PollLogin checks the status of an ongoing login.
func (m *Manager) PollLogin(ctx context.Context, sessionID string) (*LoginSession, error) {
	return m.loginManager.PollLogin(ctx, sessionID)
}

// GetAuthStatus returns the authentication status for a user.
func (m *Manager) GetAuthStatus(ctx context.Context, userID int) (*AuthStatusResponse, error) {
	hasSession, err := m.storage.HasSession(ctx, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to check session: %w", err)
	}

	if !hasSession {
		return &AuthStatusResponse{
			State:     LoginStatePending,
			Connected: false,
		}, nil
	}

	session, err := m.storage.GetSession(ctx, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get session: %w", err)
	}

	return &AuthStatusResponse{
		State:       LoginStateComplete,
		Connected:   true,
		BotUsername: session.BotUsername,
	}, nil
}

// Logout disconnects a user's Telegram session.
func (m *Manager) Logout(ctx context.Context, userID int) error {
	// Stop any active streams
	m.StopStream(userID)

	// Stop the Telegram client
	m.client.StopUserClient(userID)

	// Delete the session from storage
	if err := m.storage.DeleteSession(ctx, userID); err != nil {
		return fmt.Errorf("failed to delete session: %w", err)
	}

	return nil
}

// SetBotUsername sets the OpenClaw bot username for a user.
func (m *Manager) SetBotUsername(ctx context.Context, userID int, botUsername string) error {
	hasSession, err := m.storage.HasSession(ctx, userID)
	if err != nil {
		return fmt.Errorf("failed to check session: %w", err)
	}

	if !hasSession {
		return ErrSessionNotFound
	}

	return m.storage.UpdateBotUsername(ctx, userID, botUsername)
}

// SendMessage sends a message to a bot on behalf of a user.
func (m *Manager) SendMessage(ctx context.Context, userID int, text string) error {
	// Get the user's session
	session, err := m.storage.GetSession(ctx, userID)
	if err != nil {
		return fmt.Errorf("failed to get session: %w", err)
	}

	if session.BotUsername == "" {
		return fmt.Errorf("bot username not configured")
	}

	return m.SendMessageToBot(ctx, userID, session.BotUsername, text)
}

// SendMessageToBot sends a message to a specific bot on behalf of a user.
func (m *Manager) SendMessageToBot(ctx context.Context, userID int, botUsername string, text string) error {
	// Update last used timestamp
	_ = m.storage.UpdateLastUsed(ctx, userID)

	// Send the message
	return m.client.SendMessage(ctx, userID, botUsername, text)
}

// SendMessageAndWaitForResponse sends a message and waits for the bot's response.
func (m *Manager) SendMessageAndWaitForResponse(ctx context.Context, userID int, text string, timeout time.Duration) (*Message, error) {
	session, err := m.storage.GetSession(ctx, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get session: %w", err)
	}

	if session.BotUsername == "" {
		return nil, fmt.Errorf("bot username not configured")
	}

	// Update last used timestamp
	_ = m.storage.UpdateLastUsed(ctx, userID)

	return m.client.SendMessageAndWaitForResponse(ctx, userID, session.BotUsername, text, timeout)
}

// StreamMessages starts streaming messages from the configured bot for a user.
// Returns a channel that receives messages.
func (m *Manager) StreamMessages(ctx context.Context, userID int) (<-chan Message, error) {
	// Get the user's session
	session, err := m.storage.GetSession(ctx, userID)
	if err != nil {
		return nil, fmt.Errorf("failed to get session: %w", err)
	}

	if session.BotUsername == "" {
		return nil, fmt.Errorf("bot username not configured")
	}

	// Create a channel for messages
	msgChan := make(chan Message, 100)

	// Create a cancellable context
	streamCtx, cancel := context.WithCancel(ctx)

	// Track this stream
	m.streamsMu.Lock()
	m.streams[userID] = cancel
	m.streamsMu.Unlock()

	// Start streaming in background
	go func() {
		defer close(msgChan)
		defer func() {
			m.streamsMu.Lock()
			delete(m.streams, userID)
			m.streamsMu.Unlock()
		}()

		// Get message history first (for context)
		messages, err := m.client.GetMessageHistory(streamCtx, userID, session.BotUsername, 10)
		if err == nil {
			for _, msg := range messages {
				select {
				case msgChan <- msg:
				case <-streamCtx.Done():
					return
				}
			}
		}

		// TODO: Implement real-time message updates
		// The gotd/td library supports updates via long polling or webhooks
		// For now, we just return the history

		<-streamCtx.Done()
	}()

	return msgChan, nil
}

// StopStream stops the message stream for a user.
func (m *Manager) StopStream(userID int) {
	m.streamsMu.Lock()
	cancel, exists := m.streams[userID]
	if exists {
		cancel()
		delete(m.streams, userID)
	}
	m.streamsMu.Unlock()
}

// IsConnected returns whether a user has an active Telegram connection.
func (m *Manager) IsConnected(userID int) bool {
	return m.client.IsConnected(userID)
}

// HasSession returns whether a user has a stored Telegram session.
func (m *Manager) HasSession(ctx context.Context, userID int) (bool, error) {
	return m.storage.HasSession(ctx, userID)
}

// GetSession returns the stored session for a user.
func (m *Manager) GetSession(ctx context.Context, userID int) (*SessionData, error) {
	return m.storage.GetSession(ctx, userID)
}

// Close shuts down the manager and all active connections.
func (m *Manager) Close() error {
	// Stop all streams
	m.streamsMu.Lock()
	for userID, cancel := range m.streams {
		cancel()
		delete(m.streams, userID)
	}
	m.streamsMu.Unlock()

	// Close the Telegram client
	return m.client.Close()
}