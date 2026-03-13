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
	"encoding/json"

	"github.com/jmsunseri/bobby-assistant/service/assistant/functions"
	"google.golang.org/genai"
)

// OpenAI tool format structures
type OpenAITool struct {
	Type     string               `json:"type"`
	Function OpenAIFunctionDef    `json:"function"`
}

type OpenAIFunctionDef struct {
	Name        string                 `json:"name"`
	Description string                 `json:"description,omitempty"`
	Parameters  map[string]interface{} `json:"parameters,omitempty"`
}

type OpenAIToolCall struct {
	ID       string          `json:"id"`
	Type     string          `json:"type"`
	Function OpenAIFuncCall  `json:"function"`
}

type OpenAIFuncCall struct {
	Name      string `json:"name"`
	Arguments string `json:"arguments"`
}

type OpenAIMessage struct {
	Role      string         `json:"role"`
	Content   string         `json:"content,omitempty"`
	ToolCalls []OpenAIToolCall `json:"tool_calls,omitempty"`
	ToolCallID string        `json:"tool_call_id,omitempty"` // For tool response messages
}

type OpenAIResponse struct {
	Content    string          `json:"content"`
	ToolCalls  []OpenAIToolCall `json:"tool_calls"`
	Finish     string          `json:"finish_reason"`
}

// ConvertFunctionDeclarationsToOpenAI converts Gemini function declarations to OpenAI tool format.
func ConvertFunctionDeclarationsToOpenAI(declarations []*genai.FunctionDeclaration) []OpenAITool {
	tools := make([]OpenAITool, 0, len(declarations))
	for _, decl := range declarations {
		tool := OpenAITool{
			Type: "function",
			Function: OpenAIFunctionDef{
				Name:        decl.Name,
				Description: decl.Description,
				Parameters:  convertSchemaToOpenAI(decl.Parameters),
			},
		}
		tools = append(tools, tool)
	}
	return tools
}

// convertSchemaToOpenAI converts a Gemini schema to OpenAI's JSON schema format.
func convertSchemaToOpenAI(schema *genai.Schema) map[string]interface{} {
	if schema == nil {
		return nil
	}

	result := make(map[string]interface{})

	// Map type
	result["type"] = convertTypeToOpenAI(schema.Type)

	// Description
	if schema.Description != "" {
		result["description"] = schema.Description
	}

	// Nullable
	if schema.Nullable != nil && *schema.Nullable {
		result["nullable"] = true
	}

	// Enum
	if len(schema.Enum) > 0 {
		result["enum"] = schema.Enum
	}

	// Properties
	if len(schema.Properties) > 0 {
		props := make(map[string]interface{})
		for name, prop := range schema.Properties {
			props[name] = convertSchemaToOpenAI(prop)
		}
		result["properties"] = props
	}

	// Required
	if len(schema.Required) > 0 {
		result["required"] = schema.Required
	}

	// Items (for arrays)
	if schema.Items != nil {
		result["items"] = convertSchemaToOpenAI(schema.Items)
	}

	return result
}

// convertTypeToOpenAI maps Gemini types to OpenAI types.
func convertTypeToOpenAI(t genai.Type) string {
	switch t {
	case genai.TypeString:
		return "string"
	case genai.TypeNumber:
		return "number"
	case genai.TypeInteger:
		return "integer"
	case genai.TypeBoolean:
		return "boolean"
	case genai.TypeObject:
		return "object"
	case genai.TypeArray:
		return "array"
	default:
		return "string"
	}
}

// FormatOpenAIPrompt formats a prompt with system message and tools for OpenAI-compatible APIs.
func FormatOpenAIPrompt(systemPrompt string, tools []OpenAITool, messages []OpenAIMessage) ([]byte, error) {
	request := map[string]interface{}{
		"model": "openclaw",
		"messages": append([]OpenAIMessage{
			{
				Role:    "system",
				Content: systemPrompt,
			},
		}, messages...),
	}

	if len(tools) > 0 {
		request["tools"] = tools
	}

	return json.Marshal(request)
}

// ParseOpenAIToolCalls extracts tool calls from an OpenAI-format response.
func ParseOpenAIToolCalls(responseJSON string) ([]OpenAIToolCall, string, error) {
	var response struct {
		Choices []struct {
			Message struct {
				Content   string          `json:"content"`
				ToolCalls []OpenAIToolCall `json:"tool_calls"`
			} `json:"message"`
			FinishReason string `json:"finish_reason"`
		} `json:"choices"`
	}

	if err := json.Unmarshal([]byte(responseJSON), &response); err != nil {
		// Try parsing as a simple text response
		return nil, responseJSON, nil
	}

	if len(response.Choices) == 0 {
		return nil, "", nil
	}

	return response.Choices[0].Message.ToolCalls, response.Choices[0].Message.Content, nil
}

// FormatToolResultForOpenAI formats a function result as a tool response message.
func FormatToolResultForOpenAI(toolCallID string, name string, result string) OpenAIMessage {
	return OpenAIMessage{
		Role:       "tool",
		Content:    result,
		ToolCallID: toolCallID,
	}
}

// FormatToolCallsAsJSON serializes tool calls to JSON.
func FormatToolCallsAsJSON(toolCalls []OpenAIToolCall) (string, error) {
	if len(toolCalls) == 0 {
		return "", nil
	}
	bytes, err := json.Marshal(toolCalls)
	if err != nil {
		return "", err
	}
	return string(bytes), nil
}

// GetAvailableTools returns all available function declarations as OpenAI tools.
func GetAvailableTools(capabilities []string) []OpenAITool {
	declarations := functions.GetFunctionDefinitionsForCapabilities(capabilities)
	return ConvertFunctionDeclarationsToOpenAI(declarations)
}

// ConvertGeminiMessagesToOpenAI converts Gemini message format to OpenAI message format.
func ConvertGeminiMessagesToOpenAI(geminiMessages []*genai.Content) []OpenAIMessage {
	messages := make([]OpenAIMessage, 0, len(geminiMessages))

	for _, msg := range geminiMessages {
		openAIMsg := OpenAIMessage{
			Role: convertRoleToOpenAI(msg.Role),
		}

		var textContent string
		var toolCalls []OpenAIToolCall

		for _, part := range msg.Parts {
			if part.Text != "" {
				textContent += part.Text
			}
			if part.FunctionCall != nil {
				argsJSON, _ := json.Marshal(part.FunctionCall.Args)
				toolCalls = append(toolCalls, OpenAIToolCall{
					ID:   generateToolCallID(),
					Type: "function",
					Function: OpenAIFuncCall{
						Name:      part.FunctionCall.Name,
						Arguments: string(argsJSON),
					},
				})
			}
			if part.FunctionResponse != nil {
				// Function responses should be separate tool messages
				resultJSON, _ := json.Marshal(part.FunctionResponse.Response)
				messages = append(messages, OpenAIMessage{
					Role:       "tool",
					Content:    string(resultJSON),
					ToolCallID: part.FunctionResponse.Name, // Use name as ID for simplicity
				})
			}
		}

		if len(toolCalls) > 0 {
			openAIMsg.ToolCalls = toolCalls
		}
		if textContent != "" || len(toolCalls) == 0 {
			openAIMsg.Content = textContent
		}

		messages = append(messages, openAIMsg)
	}

	return messages
}

func convertRoleToOpenAI(role string) string {
	switch role {
	case "user":
		return "user"
	case "model":
		return "assistant"
	case "function":
		return "tool"
	default:
		return role
	}
}

func generateToolCallID() string {
	// Generate a simple unique ID for tool calls
	const charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	b := make([]byte, 24)
	for i := range b {
		b[i] = charset[i%len(charset)]
	}
	return "call_" + string(b)
}