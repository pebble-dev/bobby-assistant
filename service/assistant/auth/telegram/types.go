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

// Package telegram provides Telegram authentication and messaging functionality for Clawd.
// This package handles the deep link login flow and session management for connecting
// users to the OpenClaw bot via Telegram.
package telegram

import (
	"context"
	"time"
)

// SessionData represents a user's Telegram session stored in Redis.
// It contains the encrypted session bytes and metadata about the connection.
type SessionData struct {
	// UserID is the Pebble user ID (from timeline token verification)
	UserID int `json:"user_id"`
	// TelegramUserID is the Telegram user ID of the authenticated user
	TelegramUserID int64 `json:"telegram_user_id"`
	// TelegramUsername is the user's Telegram username (if available)
	TelegramUsername string `json:"telegram_username,omitempty"`
	// BotUsername is the OpenClaw bot username configured by the user
	BotUsername string `json:"bot_username"`
	// Session is the encrypted Telegram session data
	Session []byte `json:"session"`
	// CreatedAt is when the session was created
	CreatedAt time.Time `json:"created_at"`
	// LastUsedAt is when the session was last used
	LastUsedAt time.Time `json:"last_used_at"`
}

// LoginState represents the state of an ongoing login flow.
// This is used to track login attempts that haven't completed yet.
type LoginState string

const (
	// LoginStatePending means the login is waiting for user to tap the deep link
	LoginStatePending LoginState = "pending"
	// LoginStateWaiting means user tapped the link, waiting for Telegram authorization
	LoginStateWaiting LoginState = "waiting"
	// LoginStateComplete means the login succeeded
	LoginStateComplete LoginState = "complete"
	// LoginStateFailed means the login failed
	LoginStateFailed LoginState = "failed"
	// LoginStateExpired means the login timed out
	LoginStateExpired LoginState = "expired"
)

// LoginSession represents an in-progress login attempt.
// It tracks the state between starting a login and completing it.
type LoginSession struct {
	// SessionID is a unique identifier for this login attempt
	SessionID string `json:"session_id"`
	// UserID is the Pebble user ID attempting to log in
	UserID int `json:"user_id"`
	// DeepLink is the tg://login?token=... URL to open
	DeepLink string `json:"deep_link"`
	// State is the current state of the login
	State LoginState `json:"state"`
	// CreatedAt is when the login session was created
	CreatedAt time.Time `json:"created_at"`
	// ExpiresAt is when the login session expires
	ExpiresAt time.Time `json:"expires_at"`
	// Error contains an error message if State is LoginStateFailed
	Error string `json:"error,omitempty"`
}

// AuthStartResponse is returned when starting a new login flow.
type AuthStartResponse struct {
	// DeepLink is the tg://login?token=... URL to open in Telegram
	DeepLink string `json:"deep_link"`
	// SessionID is the unique identifier for this login attempt
	SessionID string `json:"session_id"`
	// ExpiresIn is the number of seconds until the login expires
	ExpiresIn int `json:"expires_in"`
}

// AuthStatusResponse is returned when checking login status.
type AuthStatusResponse struct {
	// State is the current state of the login
	State LoginState `json:"state"`
	// Error contains an error message if State is LoginStateFailed
	Error string `json:"error,omitempty"`
	// Connected is true if the user has an active Telegram session
	Connected bool `json:"connected,omitempty"`
	// BotUsername is the configured OpenClaw bot username (if connected)
	BotUsername string `json:"bot_username,omitempty"`
}

// Message represents a message to send or received from Telegram.
type Message struct {
	// ID is the Telegram message ID
	ID int `json:"id"`
	// Text is the message content
	Text string `json:"text"`
	// FromUserID is the sender's Telegram user ID
	FromUserID int64 `json:"from_user_id"`
	// ChatID is the chat ID (usually same as FromUserID for direct messages)
	ChatID int64 `json:"chat_id"`
	// Timestamp is when the message was sent
	Timestamp time.Time `json:"timestamp"`
}

// Client defines the interface for Telegram client operations.
type Client interface {
	// StartLogin initiates a new QR/deep link login flow.
	StartLogin(ctx context.Context, userID int) (*LoginSession, error)

	// PollLogin waits for the login to complete and returns the session.
	PollLogin(ctx context.Context, sessionID string) (*SessionData, error)

	// GetSession retrieves an existing session for a user.
	GetSession(ctx context.Context, userID int) (*SessionData, error)

	// HasSession checks if a user has an active session.
	HasSession(ctx context.Context, userID int) (bool, error)

	// DeleteSession removes a user's session.
	DeleteSession(ctx context.Context, userID int) error

	// SendMessage sends a message to a bot on behalf of the user.
	SendMessage(ctx context.Context, userID int, botUsername string, text string) error

	// StreamMessages streams incoming messages from a bot to the user.
	StreamMessages(ctx context.Context, userID int, botUsername string) (<-chan Message, error)

	// Close closes the client and releases resources.
	Close() error
}