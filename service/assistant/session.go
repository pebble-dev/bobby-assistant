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
	"encoding/json"
	"errors"
	"github.com/honeycombio/beeline-go"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
	"log"
	"net/http"
	"net/url"
	"strings"
	"time"

	"github.com/google/uuid"
	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
	"github.com/pebble-dev/bobby-assistant/service/assistant/functions"
	"github.com/pebble-dev/bobby-assistant/service/assistant/query"
	"github.com/redis/go-redis/v9"
	"google.golang.org/api/iterator"
	"google.golang.org/genai"
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
}

type QueryContext struct {
	values url.Values
}

func NewPromptSession(redisClient *redis.Client, rw http.ResponseWriter, r *http.Request) (*PromptSession, error) {
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
	}, nil
}

func (ps *PromptSession) Run(ctx context.Context) {
	ctx = query.ContextWith(ctx, ps.query)
	geminiClient, err := genai.NewClient(ctx, &genai.ClientConfig{
		APIKey:  config.GetConfig().GeminiKey,
		Backend: genai.BackendGeminiAPI,
	})
	if err != nil {
		log.Printf("error creating Gemini client: %v\n", err)
		_ = ps.conn.Close(websocket.StatusInternalError, "Error creating client.")
		return
	}

	var messages []*genai.Content
	messages = append(messages, &genai.Content{
		Parts: []*genai.Part{{Text: ps.prompt}},
		Role:  "user",
	})

	if ps.originalThreadId != "" {
		oldMessages, err := ps.restoreThread(ctx, ps.originalThreadId)
		if err != nil {
			log.Printf("error restoring thread: %v\n", err)
			_ = ps.conn.Close(websocket.StatusInternalError, "Error restoring thread.")
			return
		} else {
			messages = append(oldMessages, messages...)
		}
	}
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
		_ = ps.conn.Close(websocket.StatusPolicyViolation, "You need an active Rebble subscription to use Bobby.")
		return
	}
	qt := quota.NewTracker(ps.redis, user.UserId)
	used, remaining, err := qt.GetQuota(ctx)
	if err != nil {
		log.Printf("get quota failed: %v\n", err)
		_ = ps.conn.Close(websocket.StatusInternalError, "Quota lookup failed.")
		return
	}
	if remaining < 1 {
		log.Printf("quota exceeded for user %d\n", user.UserId)
		_ = ps.conn.Close(websocket.StatusPolicyViolation, "You have exceeded your quota for this month.")
		return
	}
	log.Printf("user %d has used %d / %d credits\n", user.UserId, used, remaining)
	totalInputTokens := 0
	totalOutputTokens := 0
	iterations := 0
	for {
		cont, err := func() (bool, error) {
			ctx, span := beeline.StartSpan(ctx, "chat_iteration")
			defer span.Send()
			iterations++
			var tools []*genai.Tool
			if iterations <= 10 {
				tools = []*genai.Tool{{FunctionDeclarations: functions.GetFunctionDefinitionsForCapabilities(query.SupportedActionsFromContext(ctx))}}
			}
			systemPrompt := ps.generateSystemPrompt(ctx)
			streamCtx, streamSpan := beeline.StartSpan(ctx, "chat_stream")
			temperature := float64(0.5)
			one := int64(1)
			s := geminiClient.Models.GenerateContentStream(streamCtx, "models/gemini-2.0-flash", messages, &genai.GenerateContentConfig{
				SystemInstruction: &genai.Content{Parts: []*genai.Part{{Text: systemPrompt}}},
				Temperature:       &temperature,
				CandidateCount:    &one,
				Tools:             tools,
			})
			var functionCall *genai.FunctionCall
			content := ""
			var usageData *genai.GenerateContentResponseUsageMetadata
		read_loop:
			for resp, err := range s {
				if errors.Is(err, iterator.Done) {
					break
				}
				if err != nil {
					streamSpan.AddField("error", err)
					log.Printf("recv from Google failed: %v\n", err)
					// This comes up when Google is over capacity, which does happen sometimes.
					// There's nothing we can really do here, though we could blame them instead of ourselves.
					_ = ps.conn.Close(websocket.StatusInternalError, "Bobby is unavailable right now. Please try again in a few moments.")
					streamSpan.Send()
					return false, err
				}
				usageData = resp.UsageMetadata
				if len(resp.Candidates) == 0 {
					continue
				}
				choice := resp.Candidates[0]
				ourContent := ""
				if choice.Content == nil {

				}
				for _, c := range choice.Content.Parts {
					if c.Text != "" {
						ourContent += c.Text
					}
					if c.FunctionCall != nil {
						fc := *c.FunctionCall
						functionCall = &fc
					}
				}
				if strings.TrimSpace(ourContent) != "" {
					words := strings.Split(ourContent, " ")
					for i, w := range words {
						if i != len(words)-1 {
							w += " "
						}
						if err := ps.conn.Write(streamCtx, websocket.MessageText, []byte("c"+w)); err != nil {
							streamSpan.AddField("error", err)
							log.Printf("write to websocket failed: %v\n", err)
							break read_loop
						}
						time.Sleep(time.Millisecond * 40)
					}
				}
				content += ourContent
			}
			streamSpan.Send()
			if usageData != nil {
				if usageData.PromptTokenCount != nil {
					_, err = qt.ChargeOutputQuota(ctx, int(*usageData.PromptTokenCount))
					if err != nil {
						log.Printf("charge output quota failed: %v\n", err)
					}
					totalInputTokens += int(*usageData.PromptTokenCount)
				}
				if usageData.CandidatesTokenCount != nil {
					_, err = qt.ChargeInputQuota(ctx, int(*usageData.CandidatesTokenCount))
					if err != nil {
						log.Printf("charge input quota failed: %v\n", err)
					}
					totalOutputTokens += int(*usageData.CandidatesTokenCount)
				}
			}
			if len(strings.TrimSpace(content)) > 0 {
				messages = append(messages, &genai.Content{
					Parts: []*genai.Part{{Text: content}},
					Role:  "model",
				})
			}
			if functionCall != nil {
				messages = append(messages, &genai.Content{
					Role: "model",
					Parts: []*genai.Part{
						{FunctionCall: functionCall},
					},
				})
				log.Printf("calling function %s\n", functionCall.Name)
				fnBytes, _ := json.Marshal(functionCall.Args)
				fnArgs := string(fnBytes)
				if err := ps.conn.Write(ctx, websocket.MessageText, []byte("f"+functions.SummariseFunction(functionCall.Name, fnArgs))); err != nil {
					log.Printf("write to websocket failed: %v\n", err)
					return false, err
				}
				var result string
				var err error
				if functions.IsAction(functionCall.Name) {
					result, err = functions.CallAction(ctx, qt, functionCall.Name, fnArgs, ps.conn)
				} else {
					result, err = functions.CallFunction(ctx, qt, functionCall.Name, fnArgs)
				}
				if err != nil {
					log.Printf("call function failed: %v\n", err)
					result = "failed to call function: " + err.Error()
				}
				var mapResult map[string]any
				_ = json.Unmarshal([]byte(result), &mapResult)
				messages = append(messages, &genai.Content{
					Role: "function",
					Parts: []*genai.Part{
						{FunctionResponse: &genai.FunctionResponse{
							Name:     functionCall.Name,
							Response: mapResult,
						}},
					},
				})
				return true, nil
			} else {
				if err := ps.conn.Write(ctx, websocket.MessageText, []byte("d")); err != nil {
					log.Printf("write to websocket failed: %v\n", err)
					return false, err
				}
			}
			return false, nil
		}()
		if err != nil {
			return
		}
		if !cont {
			log.Println("Stopping")
			break
		}
		log.Println("Going around again")
	}
	beeline.AddField(ctx, "total_input_tokens", totalInputTokens)
	beeline.AddField(ctx, "total_output_tokens", totalOutputTokens)
	beeline.AddField(ctx, "total_cost", totalInputTokens*quota.InputTokenCredits+totalOutputTokens*quota.OutputTokenCredits)
	if err := ps.storeThread(ctx, messages); err != nil {
		log.Printf("store thread failed: %v\n", err)
		_ = ps.conn.Close(websocket.StatusInternalError, "store thread failed")
		return
	}
	if err := ps.conn.Write(ctx, websocket.MessageText, []byte("t"+ps.threadId.String())); err != nil {
		log.Printf("store thread ID failed: %s\n", err)
	}
	log.Println("Request handled successfully.")
	_ = ps.conn.Close(websocket.StatusNormalClosure, "")
}

type SerializedMessage struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

func (ps *PromptSession) storeThread(ctx context.Context, messages []*genai.Content) error {
	ctx, span := beeline.StartSpan(ctx, "store_thread")
	defer span.Send()
	var toStore []SerializedMessage
	for _, m := range messages {
		if len(m.Parts) != 0 && (m.Role == "user" || m.Role == "model") && len(strings.TrimSpace(m.Parts[0].Text)) > 0 {
			toStore = append(toStore, SerializedMessage{
				Content: m.Parts[0].Text,
				Role:    m.Role,
			})
		}
	}
	j, err := json.Marshal(toStore)
	if err != nil {
		span.AddField("error", err)
		return err
	}
	ps.redis.Set(ctx, "thread:"+ps.threadId.String(), j, 10*time.Minute)
	return nil
}

func (ps *PromptSession) restoreThread(ctx context.Context, oldThreadId string) ([]*genai.Content, error) {
	ctx, span := beeline.StartSpan(ctx, "restore_thread")
	defer span.Send()
	j, err := ps.redis.Get(ctx, "thread:"+oldThreadId).Result()
	if err != nil {
		span.AddField("error", err)
		return nil, err
	}
	var messages []SerializedMessage
	if err := json.Unmarshal([]byte(j), &messages); err != nil {
		span.AddField("error", err)
		return nil, err
	}
	var result []*genai.Content
	for _, m := range messages {
		result = append(result, &genai.Content{
			Parts: []*genai.Part{{Text: m.Content}},
			Role:  m.Role,
		})
	}
	return result, nil
}
