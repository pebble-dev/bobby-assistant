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

package api

import (
	"encoding/json"
	"log"
	"net/http"

	"github.com/jmsunseri/bobby-assistant/service/assistant/auth/telegram"
	"github.com/jmsunseri/bobby-assistant/service/assistant/quota"
)

// TelegramAuthHandler handles Telegram authentication endpoints.
type TelegramAuthHandler struct {
	manager *telegram.Manager
}

// NewTelegramAuthHandler creates a new Telegram auth handler.
func NewTelegramAuthHandler(manager *telegram.Manager) *TelegramAuthHandler {
	return &TelegramAuthHandler{
		manager: manager,
	}
}

// handleAuthStart starts a new Telegram login flow.
// POST /telegram/auth/start
// Request body: {"token": "pebble_timeline_token"}
// Response: {"deep_link": "tg://login?token=...", "session_id": "...", "expires_in": 300}
func (h *TelegramAuthHandler) handleAuthStart(rw http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(rw, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	ctx := r.Context()

	// Parse request body
	var req struct {
		Token string `json:"token"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(rw, "Invalid request body", http.StatusBadRequest)
		return
	}

	if req.Token == "" {
		http.Error(rw, "Token required", http.StatusBadRequest)
		return
	}

	// Verify the Pebble token and get user info
	userInfo, err := quota.GetUserInfo(ctx, req.Token)
	if err != nil {
		log.Printf("Error getting user info: %v", err)
		http.Error(rw, "Invalid token", http.StatusUnauthorized)
		return
	}

	// Start the login flow
	login, err := h.manager.StartLogin(ctx, userInfo.UserId)
	if err != nil {
		log.Printf("Error starting Telegram login: %v", err)
		http.Error(rw, "Failed to start login", http.StatusInternalServerError)
		return
	}

	// Return the login session info
	response := telegram.AuthStartResponse{
		DeepLink:  login.DeepLink,
		SessionID: login.SessionID,
		ExpiresIn: int(login.ExpiresAt.Sub(login.CreatedAt).Seconds()),
	}

	rw.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(rw).Encode(response); err != nil {
		log.Printf("Error encoding response: %v", err)
		http.Error(rw, "Failed to encode response", http.StatusInternalServerError)
		return
	}
}

// handleAuthStatus polls the status of a login attempt.
// GET /telegram/auth/status/{session_id}
// Query params: token (pebble timeline token)
// Response: {"state": "pending|waiting|complete|failed|expired", "error": "...", "connected": true, "bot_username": "..."}
func (h *TelegramAuthHandler) handleAuthStatus(rw http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(rw, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	ctx := r.Context()

	// Get session ID from path
	sessionID := r.PathValue("session_id")
	if sessionID == "" {
		// Try to extract from URL path manually if PathValue not available
		// URL pattern: /telegram/auth/status/{session_id}
		path := r.URL.Path
		prefix := "/telegram/auth/status/"
		if len(path) > len(prefix) {
			sessionID = path[len(prefix):]
		}
	}

	if sessionID == "" {
		http.Error(rw, "Session ID required", http.StatusBadRequest)
		return
	}

	token := r.URL.Query().Get("token")
	if token == "" {
		http.Error(rw, "Token required", http.StatusBadRequest)
		return
	}

	// Verify the Pebble token
	_, err := quota.GetUserInfo(ctx, token)
	if err != nil {
		http.Error(rw, "Invalid token", http.StatusUnauthorized)
		return
	}

	// Get login status
	login, err := h.manager.PollLogin(ctx, sessionID)
	if err != nil {
		if err == telegram.ErrLoginNotFound || err == telegram.ErrLoginExpired {
			http.Error(rw, err.Error(), http.StatusNotFound)
			return
		}
		log.Printf("Error polling login status: %v", err)
		http.Error(rw, "Failed to get login status", http.StatusInternalServerError)
		return
	}

	response := telegram.AuthStatusResponse{
		State: login.State,
		Error: login.Error,
	}

	rw.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(rw).Encode(response); err != nil {
		log.Printf("Error encoding response: %v", err)
		http.Error(rw, "Failed to encode response", http.StatusInternalServerError)
		return
	}
}

// handleAuthLogout disconnects a user's Telegram session.
// POST /telegram/auth/logout
// Request body: {"token": "pebble_timeline_token"}
// Response: {"success": true}
func (h *TelegramAuthHandler) handleAuthLogout(rw http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(rw, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	ctx := r.Context()

	// Parse request body
	var req struct {
		Token string `json:"token"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(rw, "Invalid request body", http.StatusBadRequest)
		return
	}

	if req.Token == "" {
		http.Error(rw, "Token required", http.StatusBadRequest)
		return
	}

	// Verify the Pebble token and get user info
	userInfo, err := quota.GetUserInfo(ctx, req.Token)
	if err != nil {
		http.Error(rw, "Invalid token", http.StatusUnauthorized)
		return
	}

	// Logout
	if err := h.manager.Logout(ctx, userInfo.UserId); err != nil {
		log.Printf("Error logging out: %v", err)
		http.Error(rw, "Failed to logout", http.StatusInternalServerError)
		return
	}

	response := map[string]bool{"success": true}
	rw.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(rw).Encode(response); err != nil {
		log.Printf("Error encoding response: %v", err)
		http.Error(rw, "Failed to encode response", http.StatusInternalServerError)
		return
	}
}

// handleAuthCheck checks if a user has an active Telegram session.
// GET /telegram/auth/check
// Query params: token (pebble timeline token)
// Response: {"connected": true, "bot_username": "..."}
func (h *TelegramAuthHandler) handleAuthCheck(rw http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(rw, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	ctx := r.Context()

	token := r.URL.Query().Get("token")
	if token == "" {
		http.Error(rw, "Token required", http.StatusBadRequest)
		return
	}

	// Verify the Pebble token and get user info
	userInfo, err := quota.GetUserInfo(ctx, token)
	if err != nil {
		http.Error(rw, "Invalid token", http.StatusUnauthorized)
		return
	}

	// Check auth status
	status, err := h.manager.GetAuthStatus(ctx, userInfo.UserId)
	if err != nil {
		log.Printf("Error checking auth status: %v", err)
		http.Error(rw, "Failed to check auth status", http.StatusInternalServerError)
		return
	}

	rw.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(rw).Encode(status); err != nil {
		log.Printf("Error encoding response: %v", err)
		http.Error(rw, "Failed to encode response", http.StatusInternalServerError)
		return
	}
}

// handleSetBotUsername sets the OpenClaw bot username for a user.
// POST /telegram/auth/bot
// Request body: {"token": "pebble_timeline_token", "bot_username": "@OpenClawBot"}
// Response: {"success": true}
func (h *TelegramAuthHandler) handleSetBotUsername(rw http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(rw, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	ctx := r.Context()

	// Parse request body
	var req struct {
		Token       string `json:"token"`
		BotUsername string `json:"bot_username"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(rw, "Invalid request body", http.StatusBadRequest)
		return
	}

	if req.Token == "" {
		http.Error(rw, "Token required", http.StatusBadRequest)
		return
	}

	if req.BotUsername == "" {
		http.Error(rw, "Bot username required", http.StatusBadRequest)
		return
	}

	// Verify the Pebble token and get user info
	userInfo, err := quota.GetUserInfo(ctx, req.Token)
	if err != nil {
		http.Error(rw, "Invalid token", http.StatusUnauthorized)
		return
	}

	// Set the bot username
	if err := h.manager.SetBotUsername(ctx, userInfo.UserId, req.BotUsername); err != nil {
		if err == telegram.ErrSessionNotFound {
			http.Error(rw, "No Telegram session found. Please connect first.", http.StatusBadRequest)
			return
		}
		log.Printf("Error setting bot username: %v", err)
		http.Error(rw, "Failed to set bot username", http.StatusInternalServerError)
		return
	}

	response := map[string]bool{"success": true}
	rw.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(rw).Encode(response); err != nil {
		log.Printf("Error encoding response: %v", err)
		http.Error(rw, "Failed to encode response", http.StatusInternalServerError)
		return
	}
}

// RegisterRoutes registers all Telegram auth routes on the given mux.
func (h *TelegramAuthHandler) RegisterRoutes(mux *http.ServeMux) {
	mux.HandleFunc("/telegram/auth/start", h.handleAuthStart)
	mux.HandleFunc("/telegram/auth/status/", h.handleAuthStatus)
	mux.HandleFunc("/telegram/auth/logout", h.handleAuthLogout)
	mux.HandleFunc("/telegram/auth/check", h.handleAuthCheck)
	mux.HandleFunc("/telegram/auth/bot", h.handleSetBotUsername)
}