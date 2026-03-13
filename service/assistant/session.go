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

package assistant

import (
	"context"
	"log"
	"net/http"
	"net/url"

	"github.com/google/uuid"
	"github.com/honeycombio/beeline-go"
	"github.com/jmsunseri/bobby-assistant/service/assistant/auth/telegram"
	"github.com/jmsunseri/bobby-assistant/service/assistant/quota"
	"github.com/jmsunseri/bobby-assistant/service/assistant/query"
	"github.com/redis/go-redis/v9"
	"nhooyr.io/websocket"
)

type PromptSession struct {
	conn             *websocket.Conn
	prompt           string
	userToken        string
	query            url.Values
	redis            *redis.Client
	threadId         uuid.UUID
	originalThreadId string
	telegramManager  *telegram.Manager
}

type QueryContext struct {
	values url.Values
}

func NewPromptSession(redisClient *redis.Client, telegramMgr *telegram.Manager, rw http.ResponseWriter, r *http.Request) (*PromptSession, error) {
	prompt := r.URL.Query().Get("prompt")
	userToken := r.URL.Query().Get("token")
	originalThreadId := r.URL.Query().Get("threadId")
	c, err := websocket.Accept(rw, r, &websocket.AcceptOptions{
		OriginPatterns:     []string{"null"},
		InsecureSkipVerify: true,
	})
	if err != nil {
		return nil, err
	}

	return &PromptSession{
		conn:             c,
		prompt:           prompt,
		userToken:        userToken,
		query:            r.URL.Query(),
		redis:            redisClient,
		threadId:         uuid.New(),
		originalThreadId: originalThreadId,
		telegramManager:  telegramMgr,
	}, nil
}

func (ps *PromptSession) Run(ctx context.Context) {
	ctx = query.ContextWith(ctx, ps.query)

	// Get user info for authentication
	user, err := quota.GetUserInfo(ctx, ps.userToken)
	if err != nil {
		log.Printf("get user info failed: %v\n", err)
		_ = ps.conn.Close(websocket.StatusInternalError, "get user info failed")
		return
	}
	beeline.AddField(ctx, "user_id", user.UserId)
	if !user.HasSubscription {
		beeline.AddField(ctx, "error", "no subscription")
		log.Printf("user %d has no subscription\n", user.UserId)
		_ = ps.conn.Close(websocket.StatusPolicyViolation, "You need an active Rebble subscription to use Clawd.")
		return
	}

	// Check if Telegram is configured
	if ps.telegramManager == nil {
		_ = ps.conn.Close(websocket.StatusInternalError, "Telegram not configured on server.")
		return
	}

	// Check if user has a Telegram session
	hasSession, err := ps.telegramManager.HasSession(ctx, user.UserId)
	if err != nil {
		log.Printf("error checking Telegram session: %v\n", err)
		_ = ps.conn.Close(websocket.StatusInternalError, "Failed to check Telegram session.")
		return
	}

	if !hasSession {
		_ = ps.conn.Close(websocket.StatusPolicyViolation, "No Telegram session found. Please connect your Telegram account in the settings.")
		return
	}

	// Run with Telegram/OpenClaw
	ps.runWithTelegram(ctx, user.UserId)
}

func (ps *PromptSession) runWithTelegram(ctx context.Context, userID int) {
	// Get bot username from URL parameter
	botUsername := ps.query.Get("bot")
	if botUsername == "" {
		// Try to get from stored session
		session, err := ps.telegramManager.GetSession(ctx, userID)
		if err != nil {
			log.Printf("error getting Telegram session: %v\n", err)
			_ = ps.conn.Close(websocket.StatusInternalError, "Failed to get Telegram session.")
			return
		}
		botUsername = session.BotUsername
	}

	if botUsername == "" {
		_ = ps.conn.Close(websocket.StatusPolicyViolation, "No bot username configured. Please set your OpenClaw bot username in the settings.")
		return
	}

	// Get capabilities from query context
	capabilities := query.SupportedActionsFromContext(ctx)

	// Generate system prompt
	systemPrompt := ps.generateSystemPrompt(ctx)

	// Create quota tracker
	user, err := quota.GetUserInfo(ctx, ps.userToken)
	if err != nil {
		log.Printf("get user info failed: %v\n", err)
		_ = ps.conn.Close(websocket.StatusInternalError, "get user info failed")
		return
	}
	qt := quota.NewTracker(ps.redis, user.UserId)

	// Create OpenClaw session and run
	openClawSession := NewOpenClawSession(
		ctx,
		ps.telegramManager,
		userID,
		botUsername,
		systemPrompt,
		capabilities,
		qt,
		ps.conn,
	)

	if err := openClawSession.Run(ctx, ps.prompt); err != nil {
		log.Printf("OpenClaw session failed: %v\n", err)
		_ = ps.conn.Close(websocket.StatusInternalError, err.Error())
		return
	}

	// Send thread ID for continuation
	if err := ps.conn.Write(ctx, websocket.MessageText, []byte("t"+ps.threadId.String())); err != nil {
		log.Printf("write thread ID failed: %v\n", err)
	}

	log.Println("OpenClaw request handled successfully.")
	_ = ps.conn.Close(websocket.StatusNormalClosure, "")
}