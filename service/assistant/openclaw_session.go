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
	"fmt"
	"log"
	"regexp"
	"strings"
	"time"

	"github.com/honeycombio/beeline-go"
	"github.com/jmsunseri/bobby-assistant/service/assistant/auth/telegram"
	"github.com/jmsunseri/bobby-assistant/service/assistant/functions"
	"github.com/jmsunseri/bobby-assistant/service/assistant/quota"
	"github.com/jmsunseri/bobby-assistant/service/assistant/widgets"
	"nhooyr.io/websocket"
)

const (
	// MaxIterations is the maximum number of function call iterations before stopping
	MaxIterations = 10
	// LLMPromptPrefix is the prefix for prompts sent to OpenClaw's LLM
	LLMPromptPrefix = "/llm-task "
	// ResponseTimeout is the maximum time to wait for OpenClaw response
	ResponseTimeout = 120 * time.Second
)

// OpenClawSession manages a conversation with OpenClaw via Telegram.
type OpenClawSession struct {
	manager      *telegram.Manager
	userID       int
	botUsername  string
	systemPrompt string
	tools        []OpenAITool
	messages     []OpenAIMessage
	quotaTracker *quota.Tracker
	websocket    *websocket.Conn
	queryContext context.Context
}

// NewOpenClawSession creates a new OpenClaw session.
func NewOpenClawSession(
	ctx context.Context,
	manager *telegram.Manager,
	userID int,
	botUsername string,
	systemPrompt string,
	capabilities []string,
	qt *quota.Tracker,
	ws *websocket.Conn,
) *OpenClawSession {
	return &OpenClawSession{
		manager:      manager,
		userID:       userID,
		botUsername:  botUsername,
		systemPrompt: systemPrompt,
		tools:        GetAvailableTools(capabilities),
		messages:     nil,
		quotaTracker: qt,
		websocket:    ws,
		queryContext: ctx,
	}
}

// Run starts the conversation loop with OpenClaw.
func (s *OpenClawSession) Run(ctx context.Context, initialPrompt string) error {
	ctx, span := beeline.StartSpan(ctx, "openclaw_session")
	defer span.Send()

	// Add initial user message
	s.messages = append(s.messages, OpenAIMessage{
		Role:    "user",
		Content: initialPrompt,
	})

	iterations := 0
	for {
		if iterations >= MaxIterations {
			log.Printf("Max iterations reached in OpenClaw session")
			break
		}
		iterations++

		// Send to OpenClaw and get response
		response, toolCalls, content, err := s.sendAndReceive(ctx)
		if err != nil {
			return fmt.Errorf("failed to send/receive: %w", err)
		}

		// Stream text content to client
		if content != "" {
			if err := s.streamToClient(content); err != nil {
				return fmt.Errorf("failed to stream content: %w", err)
			}
		}

		// If no tool calls, we're done
		if len(toolCalls) == 0 {
			// Send done signal
			if err := s.websocket.Write(ctx, websocket.MessageText, []byte("d")); err != nil {
				log.Printf("write done signal failed: %v", err)
			}
			return nil
		}

		// Execute tool calls locally
		toolResults := s.executeToolCalls(ctx, toolCalls)

		// Add assistant message with tool calls
		s.messages = append(s.messages, response)

		// Add tool results as messages
		for id, result := range toolResults {
			s.messages = append(s.messages, OpenAIMessage{
				Role:       "tool",
				Content:    result,
				ToolCallID: id,
			})
		}

		// Continue loop - send tool results back to OpenClaw
	}

	// Should not reach here, but return nil if we do
	return nil
}

// sendAndReceive sends the current messages to OpenClaw and parses the response.
func (s *OpenClawSession) sendAndReceive(ctx context.Context) (OpenAIMessage, []OpenAIToolCall, string, error) {
	// Format the prompt with tools and system message
	prompt, err := s.formatPrompt()
	if err != nil {
		return OpenAIMessage{}, nil, "", fmt.Errorf("failed to format prompt: %w", err)
	}

	// Send to OpenClaw via Telegram and wait for response
	msg, err := s.manager.SendMessageAndWaitForResponse(ctx, s.userID, prompt, ResponseTimeout)
	if err != nil {
		return OpenAIMessage{}, nil, "", fmt.Errorf("failed to send/receive: %w", err)
	}

	// Parse the response
	responseContent := msg.Text
	toolCalls, content, err := ParseOpenAIToolCalls(responseContent)
	if err != nil {
		// If parsing fails, treat the whole message as text
		content = responseContent
	}

	// Build the assistant message
	assistantMsg := OpenAIMessage{
		Role: "assistant",
	}
	if content != "" {
		assistantMsg.Content = content
	}
	if len(toolCalls) > 0 {
		assistantMsg.ToolCalls = toolCalls
	}

	return assistantMsg, toolCalls, content, nil
}

// formatPrompt formats the current messages with system prompt and tools for OpenClaw.
func (s *OpenClawSession) formatPrompt() (string, error) {
	// Build the JSON structure
	request := map[string]interface{}{
		"system":  s.systemPrompt,
		"tools":   s.tools,
		"messages": s.messages,
	}

	jsonBytes, err := json.Marshal(request)
	if err != nil {
		return "", err
	}

	// Prepend the /llm-task prefix
	return LLMPromptPrefix + string(jsonBytes), nil
}

// executeToolCalls executes the tool calls locally and returns the results.
func (s *OpenClawSession) executeToolCalls(ctx context.Context, toolCalls []OpenAIToolCall) map[string]string {
	results := make(map[string]string)

	for _, tc := range toolCalls {
		result := s.executeToolCall(ctx, tc)
		results[tc.ID] = result
	}

	return results
}

// executeToolCall executes a single tool call.
func (s *OpenClawSession) executeToolCall(ctx context.Context, tc OpenAIToolCall) string {
	ctx, span := beeline.StartSpan(ctx, "tool_call_"+tc.Function.Name)
	defer span.Send()

	// Check if this is a regular function or an action (requires round-trip to watch)
	isAction := functions.IsAction(tc.Function.Name)

	var result string
	var err error

	if isAction {
		// Actions require round-trip to the watch
		result, err = functions.CallAction(ctx, s.quotaTracker, tc.Function.Name, tc.Function.Arguments, s.websocket)
	} else {
		// Regular functions can be called directly
		result, err = functions.CallFunction(ctx, s.quotaTracker, tc.Function.Name, tc.Function.Arguments)
	}

	if err != nil {
		log.Printf("tool call %s failed: %v", tc.Function.Name, err)
		result = fmt.Sprintf(`{"error": "%s"}`, err.Error())
	}

	return result
}

// streamToClient streams text content to the WebSocket client.
func (s *OpenClawSession) streamToClient(content string) error {
	// Process widgets in the content (same as Gemini implementation)
	widgetRegex := regexp.MustCompile(`(?s)\s*<!.+?[!/]>\s*`)
	widget := widgetRegex.FindAllString(content, -1)
	splitting := true
	leftTrimming := false

	streamContent := content
	if len(widget) > 0 {
		for _, w := range widget {
			processed, err := widgets.ProcessWidget(s.queryContext, s.quotaTracker, w)
			replacement := ""
			if err != nil {
				log.Printf("process widget failed: %v\n", err)
				replacement = "(widget processing failed)"
			} else {
				jsoned, err := json.Marshal(processed)
				if err != nil {
					log.Printf("marshal widget failed: %v\n", err)
					replacement = "(widget processing failed)"
				} else {
					splitting = false
					replacement = "<<!!WIDGET:" + string(jsoned) + "!!>>"
				}
			}
			streamContent = strings.Replace(streamContent, w, replacement, 1)
			if strings.HasSuffix(streamContent, "!!>>") {
				leftTrimming = true
			}
		}
	}

	// If the last thing was a widget, strip leading whitespace from the next text
	if leftTrimming {
		streamContent = strings.TrimLeft(streamContent, " \r\n\t")
	}

	// Stream the content word by word
	if strings.TrimSpace(streamContent) != "" {
		var words []string
		if splitting {
			words = strings.Split(streamContent, " ")
			leftTrimming = false
		} else {
			words = []string{streamContent}
		}

		for i, w := range words {
			if i != len(words)-1 {
				w += " "
			}
			if err := s.websocket.Write(s.queryContext, websocket.MessageText, []byte("c"+w)); err != nil {
				return err
			}
			time.Sleep(40 * time.Millisecond)
		}
	}

	return nil
}