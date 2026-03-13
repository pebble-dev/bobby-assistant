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
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

const (
	// sessionKeyPrefix is the Redis key prefix for session data
	sessionKeyPrefix = "telegram_session:"
	// loginKeyPrefix is the Redis key prefix for login sessions
	loginKeyPrefix = "telegram_login:"
	// sessionTTL is how long sessions are valid (30 days)
	sessionTTL = 30 * 24 * time.Hour
	// loginTTL is how long login attempts are valid (5 minutes)
	loginTTL = 5 * time.Minute
)

var (
	// ErrSessionNotFound is returned when no session exists for a user
	ErrSessionNotFound = errors.New("session not found")
	// ErrLoginNotFound is returned when no login session exists
	ErrLoginNotFound = errors.New("login session not found")
	// ErrLoginExpired is returned when a login session has expired
	ErrLoginExpired = errors.New("login session expired")
	// ErrInvalidKey is returned when the encryption key is invalid
	ErrInvalidKey = errors.New("invalid encryption key")
)

// SessionStorage handles storing and retrieving Telegram sessions from Redis
// with AES-256-GCM encryption.
type SessionStorage struct {
	redis   *redis.Client
	key     []byte // 32-byte encryption key
	gcm     cipher.AEAD
	keyHash string
}

// NewSessionStorage creates a new session storage with the given Redis client
// and encryption key. The key must be exactly 32 bytes for AES-256.
func NewSessionStorage(redisClient *redis.Client, encryptionKey string) (*SessionStorage, error) {
	key := []byte(encryptionKey)
	if len(key) != 32 {
		return nil, fmt.Errorf("%w: key must be 32 bytes, got %d", ErrInvalidKey, len(key))
	}

	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, fmt.Errorf("failed to create cipher: %w", err)
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return nil, fmt.Errorf("failed to create GCM: %w", err)
	}

	return &SessionStorage{
		redis:   redisClient,
		key:     key,
		gcm:     gcm,
		keyHash: base64.StdEncoding.EncodeToString(key[:8]), // For logging/debugging
	}, nil
}

// userSessionKey returns the Redis key for a user's session.
func (s *SessionStorage) userSessionKey(userID int) string {
	return fmt.Sprintf("%s%d", sessionKeyPrefix, userID)
}

// loginSessionKey returns the Redis key for a login session.
func (s *SessionStorage) loginSessionKey(sessionID string) string {
	return fmt.Sprintf("%s%s", loginKeyPrefix, sessionID)
}

// encrypt encrypts data using AES-256-GCM and returns base64-encoded ciphertext.
func (s *SessionStorage) encrypt(plaintext []byte) (string, error) {
	nonce := make([]byte, s.gcm.NonceSize())
	if _, err := rand.Read(nonce); err != nil {
		return "", fmt.Errorf("failed to generate nonce: %w", err)
	}

	ciphertext := s.gcm.Seal(nil, nonce, plaintext, nil)
	// Prepend nonce to ciphertext
	combined := append(nonce, ciphertext...)
	return base64.StdEncoding.EncodeToString(combined), nil
}

// decrypt decrypts base64-encoded AES-256-GCM ciphertext.
func (s *SessionStorage) decrypt(ciphertext string) ([]byte, error) {
	data, err := base64.StdEncoding.DecodeString(ciphertext)
	if err != nil {
		return nil, fmt.Errorf("failed to decode base64: %w", err)
	}

	if len(data) < s.gcm.NonceSize() {
		return nil, errors.New("ciphertext too short")
	}

	nonce := data[:s.gcm.NonceSize()]
	ciphertextBytes := data[s.gcm.NonceSize():]

	plaintext, err := s.gcm.Open(nil, nonce, ciphertextBytes, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to decrypt: %w", err)
	}

	return plaintext, nil
}

// SaveSession stores a user's Telegram session in Redis.
func (s *SessionStorage) SaveSession(ctx context.Context, session *SessionData) error {
	session.LastUsedAt = time.Now()
	session.CreatedAt = time.Now()

	data, err := json.Marshal(session)
	if err != nil {
		return fmt.Errorf("failed to marshal session: %w", err)
	}

	encrypted, err := s.encrypt(data)
	if err != nil {
		return fmt.Errorf("failed to encrypt session: %w", err)
	}

	key := s.userSessionKey(session.UserID)
	return s.redis.Set(ctx, key, encrypted, sessionTTL).Err()
}

// GetSession retrieves a user's Telegram session from Redis.
func (s *SessionStorage) GetSession(ctx context.Context, userID int) (*SessionData, error) {
	key := s.userSessionKey(userID)
	encrypted, err := s.redis.Get(ctx, key).Result()
	if err == redis.Nil {
		return nil, ErrSessionNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("failed to get session: %w", err)
	}

	data, err := s.decrypt(encrypted)
	if err != nil {
		return nil, fmt.Errorf("failed to decrypt session: %w", err)
	}

	var session SessionData
	if err := json.Unmarshal(data, &session); err != nil {
		return nil, fmt.Errorf("failed to unmarshal session: %w", err)
	}

	return &session, nil
}

// HasSession checks if a user has an active session.
func (s *SessionStorage) HasSession(ctx context.Context, userID int) (bool, error) {
	key := s.userSessionKey(userID)
	exists, err := s.redis.Exists(ctx, key).Result()
	if err != nil {
		return false, fmt.Errorf("failed to check session existence: %w", err)
	}
	return exists > 0, nil
}

// DeleteSession removes a user's session from Redis.
func (s *SessionStorage) DeleteSession(ctx context.Context, userID int) error {
	key := s.userSessionKey(userID)
	return s.redis.Del(ctx, key).Err()
}

// UpdateLastUsed updates the last_used_at timestamp for a session.
func (s *SessionStorage) UpdateLastUsed(ctx context.Context, userID int) error {
	session, err := s.GetSession(ctx, userID)
	if err != nil {
		return err
	}

	session.LastUsedAt = time.Now()
	return s.SaveSession(ctx, session)
}

// SaveLoginSession stores an in-progress login session.
func (s *SessionStorage) SaveLoginSession(ctx context.Context, login *LoginSession) error {
	data, err := json.Marshal(login)
	if err != nil {
		return fmt.Errorf("failed to marshal login session: %w", err)
	}

	key := s.loginSessionKey(login.SessionID)
	return s.redis.Set(ctx, key, data, loginTTL).Err()
}

// GetLoginSession retrieves an in-progress login session.
func (s *SessionStorage) GetLoginSession(ctx context.Context, sessionID string) (*LoginSession, error) {
	key := s.loginSessionKey(sessionID)
	data, err := s.redis.Get(ctx, key).Result()
	if err == redis.Nil {
		return nil, ErrLoginNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("failed to get login session: %w", err)
	}

	var login LoginSession
	if err := json.Unmarshal([]byte(data), &login); err != nil {
		return nil, fmt.Errorf("failed to unmarshal login session: %w", err)
	}

	// Check if expired
	if time.Now().After(login.ExpiresAt) {
		// Clean up expired session
		_ = s.redis.Del(ctx, key)
		return nil, ErrLoginExpired
	}

	return &login, nil
}

// UpdateLoginSession updates an existing login session.
func (s *SessionStorage) UpdateLoginSession(ctx context.Context, login *LoginSession) error {
	return s.SaveLoginSession(ctx, login)
}

// DeleteLoginSession removes a login session.
func (s *SessionStorage) DeleteLoginSession(ctx context.Context, sessionID string) error {
	key := s.loginSessionKey(sessionID)
	return s.redis.Del(ctx, key).Err()
}

// UpdateBotUsername updates the bot username for an existing session.
func (s *SessionStorage) UpdateBotUsername(ctx context.Context, userID int, botUsername string) error {
	session, err := s.GetSession(ctx, userID)
	if err != nil {
		return err
	}

	session.BotUsername = botUsername
	return s.SaveSession(ctx, session)
}