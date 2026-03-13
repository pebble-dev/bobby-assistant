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
	"github.com/gotd/td/telegram/auth/qrlogin"
	"github.com/gotd/td/tg"
	"github.com/google/uuid"
)

const (
	// loginTimeout is the maximum time to wait for a login to complete
	loginTimeout = 5 * time.Minute
	// pollInterval is how often to poll for login token acceptance
	pollInterval = 1 * time.Second
)

// LoginManager handles the deep link login flow for Telegram.
type LoginManager struct {
	client  *TelegramClient
	storage *SessionStorage
	appID   int
	appHash string

	// Track active logins
	activeLogins   map[string]context.CancelFunc
	activeLoginsMu sync.Mutex
}

// NewLoginManager creates a new login manager.
func NewLoginManager(client *TelegramClient, storage *SessionStorage, appID int, appHash string) *LoginManager {
	return &LoginManager{
		client:       client,
		storage:      storage,
		appID:        appID,
		appHash:      appHash,
		activeLogins: make(map[string]context.CancelFunc),
	}
}

// StartLogin initiates a new deep link login flow.
// It returns a LoginSession that contains the deep link URL to open in Telegram.
func (l *LoginManager) StartLogin(ctx context.Context, userID int) (*LoginSession, error) {
	// Generate a unique session ID
	sessionID := uuid.New().String()

	// Create login session record
	now := time.Now()
	login := &LoginSession{
		SessionID: sessionID,
		UserID:    userID,
		State:     LoginStatePending,
		CreatedAt: now,
		ExpiresAt: now.Add(loginTimeout),
	}

	// Save initial state
	if err := l.storage.SaveLoginSession(ctx, login); err != nil {
		return nil, fmt.Errorf("failed to save login session: %w", err)
	}

	// Create session storage for this login attempt
	sessionStore := &loginSessionStorage{
		storage:   l.storage,
		userID:    userID,
		sessionID: sessionID,
	}

	// Channel to receive results from the login goroutine
	resultChan := make(chan loginResult, 1)

	// Create context for this login attempt
	loginCtx, cancel := context.WithTimeout(context.Background(), loginTimeout)

	// Track this login
	l.activeLoginsMu.Lock()
	l.activeLogins[sessionID] = cancel
	l.activeLoginsMu.Unlock()

	// Start the login flow in a background goroutine
	go l.runLoginFlow(loginCtx, sessionID, userID, sessionStore, resultChan)

	// Wait briefly for the initial token to be generated
	time.Sleep(200 * time.Millisecond)

	// Poll for the deep link (up to 3 seconds)
	for i := 0; i < 30; i++ {
		currentLogin, err := l.storage.GetLoginSession(ctx, sessionID)
		if err != nil {
			cancel()
			return nil, fmt.Errorf("failed to get login session: %w", err)
		}
		if currentLogin.DeepLink != "" {
			// Start background waiter for completion
			go l.waitForCompletion(userID, sessionID, resultChan, cancel)
			return currentLogin, nil
		}
		time.Sleep(100 * time.Millisecond)
	}

	// Failed to get deep link in time
	cancel()
	l.updateLoginState(sessionID, LoginStateFailed, "failed to generate login token")
	return nil, fmt.Errorf("failed to generate login token in time")
}

// loginResult is used to pass results from the background goroutine.
type loginResult struct {
	userID   int64
	username string
	success  bool
	err      error
}

// runLoginFlow runs the Telegram QR login flow.
func (l *LoginManager) runLoginFlow(ctx context.Context, sessionID string, userID int, sessionStore *loginSessionStorage, resultChan chan<- loginResult) {
	defer close(resultChan)

	// Create Telegram client for this login
	client := telegram.NewClient(l.appID, l.appHash, telegram.Options{
		SessionStorage: sessionStore,
	})

	err := client.Run(ctx, func(ctx context.Context) error {
		api := client.API()
		qr := qrlogin.NewQR(api, l.appID, l.appHash, qrlogin.Options{})

		for {
			// Export a new token
			token, err := qr.Export(ctx)
			if err != nil {
				resultChan <- loginResult{err: fmt.Errorf("failed to export token: %w", err)}
				return err
			}

			// Update the login session with the deep link
			deepLink := token.URL()
			if login, err := l.storage.GetLoginSession(ctx, sessionID); err == nil {
				login.DeepLink = deepLink
				login.State = LoginStateWaiting
				_ = l.storage.UpdateLoginSession(ctx, login)
			}

			// Wait for token expiration or context cancellation, polling for acceptance
			deadline := time.NewTimer(time.Until(token.Expires()))
			defer deadline.Stop()

			pollTicker := time.NewTicker(pollInterval)
			defer pollTicker.Stop()

			for {
				select {
				case <-pollTicker.C:
					// Try to import the token (check if user accepted)
					auth, err := qr.Import(ctx)
					if err == nil && auth != nil {
						// Login successful!
						// Extract user from auth result
						userID := auth.User.GetID()
						var username string
						if u, ok := auth.User.(*tg.User); ok {
							username = u.Username
						}

						resultChan <- loginResult{
							userID:   userID,
							username: username,
							success:  true,
						}
						return nil
					}

				case <-deadline.C:
					// Token expired, generate new one
					goto nextToken

				case <-ctx.Done():
					resultChan <- loginResult{err: ctx.Err()}
					return ctx.Err()
				}
			}

		nextToken:
		}
	})

	if err != nil {
		select {
		case resultChan <- loginResult{err: err}:
		default:
		}
	}
}

// waitForCompletion waits for the login to complete and updates the session.
func (l *LoginManager) waitForCompletion(userID int, sessionID string, resultChan <-chan loginResult, cancel context.CancelFunc) {
	defer cancel()

	// Clean up tracking
	defer func() {
		l.activeLoginsMu.Lock()
		delete(l.activeLogins, sessionID)
		l.activeLoginsMu.Unlock()
	}()

	select {
	case result := <-resultChan:
		ctx := context.Background()

		if result.err != nil {
			l.updateLoginState(sessionID, LoginStateFailed, result.err.Error())
			return
		}

		if !result.success {
			l.updateLoginState(sessionID, LoginStateFailed, "login failed")
			return
		}

		// Get or create session data
		session, err := l.storage.GetSession(ctx, userID)
		if err != nil && err != ErrSessionNotFound {
			l.updateLoginState(sessionID, LoginStateFailed, fmt.Sprintf("failed to get session: %v", err))
			return
		}

		now := time.Now()
		if session == nil {
			session = &SessionData{
				UserID:    userID,
				CreatedAt: now,
			}
		}

		session.TelegramUserID = result.userID
		session.TelegramUsername = result.username
		session.LastUsedAt = now

		// Save the session
		if err := l.storage.SaveSession(ctx, session); err != nil {
			l.updateLoginState(sessionID, LoginStateFailed, fmt.Sprintf("failed to save session: %v", err))
			return
		}

		// Mark login as complete
		l.updateLoginState(sessionID, LoginStateComplete, "")

	case <-time.After(loginTimeout):
		l.updateLoginState(sessionID, LoginStateExpired, "login timed out")
	}
}

// updateLoginState updates the state of a login session.
func (l *LoginManager) updateLoginState(sessionID string, state LoginState, errMsg string) {
	ctx := context.Background()

	login, err := l.storage.GetLoginSession(ctx, sessionID)
	if err != nil {
		return
	}

	login.State = state
	if errMsg != "" {
		login.Error = errMsg
	}

	_ = l.storage.UpdateLoginSession(ctx, login)
}

// PollLogin checks the status of an ongoing login session.
func (l *LoginManager) PollLogin(ctx context.Context, sessionID string) (*LoginSession, error) {
	login, err := l.storage.GetLoginSession(ctx, sessionID)
	if err != nil {
		return nil, err
	}

	// Check if expired
	if time.Now().After(login.ExpiresAt) {
		_ = l.storage.DeleteLoginSession(ctx, sessionID)
		return nil, ErrLoginExpired
	}

	return login, nil
}

// CancelLogin cancels an ongoing login attempt.
func (l *LoginManager) CancelLogin(sessionID string) {
	l.activeLoginsMu.Lock()
	if cancel, exists := l.activeLogins[sessionID]; exists {
		cancel()
		delete(l.activeLogins, sessionID)
	}
	l.activeLoginsMu.Unlock()
}

// loginSessionStorage implements telegram.SessionStorage for login sessions.
type loginSessionStorage struct {
	storage   *SessionStorage
	userID    int
	sessionID string
}

// LoadSession loads the session data (returns nil for new logins).
func (s *loginSessionStorage) LoadSession(ctx context.Context) ([]byte, error) {
	// For new logins, check if there's an existing session
	session, err := s.storage.GetSession(ctx, s.userID)
	if err == ErrSessionNotFound {
		return nil, nil // No session yet
	}
	if err != nil {
		return nil, err
	}
	return session.Session, nil
}

// StoreSession stores the session data during login.
func (s *loginSessionStorage) StoreSession(ctx context.Context, data []byte) error {
	// The session will be transferred to the main storage in waitForCompletion
	now := time.Now()

	session, err := s.storage.GetSession(ctx, s.userID)
	if err != nil && err != ErrSessionNotFound {
		return err
	}

	if session == nil {
		session = &SessionData{
			UserID:    s.userID,
			CreatedAt: now,
		}
	}

	session.Session = data
	session.LastUsedAt = now

	return s.storage.SaveSession(ctx, session)
}

// GetUserInfo retrieves the Telegram user info for a connected user.
func (l *LoginManager) GetUserInfo(ctx context.Context, userID int) (*tg.User, error) {
	var user *tg.User

	err := l.client.RunClient(ctx, userID, func(ctx context.Context, api *tg.Client) error {
		users, err := api.UsersGetUsers(ctx, []tg.InputUserClass{&tg.InputUserSelf{}})
		if err != nil {
			return err
		}

		if len(users) > 0 {
			if u, ok := users[0].(*tg.User); ok {
				user = u
			}
		}

		return nil
	})

	if err != nil {
		return nil, fmt.Errorf("failed to get user info: %w", err)
	}

	return user, nil
}