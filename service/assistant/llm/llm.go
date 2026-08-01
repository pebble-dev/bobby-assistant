// Copyright 2026 Rebble Alliance, LLC
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

package llm

import (
	"context"
	"fmt"
	"iter"

	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
)

type Role string

const (
	RoleUser      Role = "user"
	RoleAssistant Role = "assistant"
	RoleFunction  Role = "function"
)

type Type string

const (
	TypeString  Type = "string"
	TypeNumber  Type = "number"
	TypeInteger Type = "integer"
	TypeBoolean Type = "boolean"
	TypeArray   Type = "array"
	TypeObject  Type = "object"
)

type Schema struct {
	Type        Type               `json:"type,omitempty"`
	Format      string             `json:"format,omitempty"`
	Description string             `json:"description,omitempty"`
	Enum        []string           `json:"enum,omitempty"`
	Items       *Schema            `json:"items,omitempty"`
	Properties  map[string]*Schema `json:"properties,omitempty"`
	Required    []string           `json:"required,omitempty"`
	Nullable    *bool              `json:"nullable,omitempty"`
}

type FunctionDeclaration struct {
	Name        string  `json:"name,omitempty"`
	Description string  `json:"description,omitempty"`
	Parameters  *Schema `json:"parameters,omitempty"`
}

type FunctionCall struct {
	Name string         `json:"name,omitempty"`
	Args map[string]any `json:"args,omitempty"`
}

type FunctionResponse struct {
	Name     string         `json:"name,omitempty"`
	Response map[string]any `json:"response,omitempty"`
}

type Part struct {
	Text             string            `json:"text,omitempty"`
	FunctionCall     *FunctionCall     `json:"functionCall,omitempty"`
	FunctionResponse *FunctionResponse `json:"functionResponse,omitempty"`
}

type Content struct {
	Role  Role    `json:"role,omitempty"`
	Parts []*Part `json:"parts,omitempty"`
}

// Model selects a capability tier
type Model string

const (
	// ModelDefault is the primary assistant model.
	ModelDefault Model = "default"
	// ModelLite is a fast, cheap model for auxiliary tasks like the verifier.
	ModelLite Model = "lite"
)

type Usage struct {
	InputTokens       int
	CachedInputTokens int
	OutputTokens      int
}

type Request struct {
	Model        Model
	SystemPrompt string
	Messages     []*Content
	Temperature  *float32
	// Tools the model may call
	Tools []*FunctionDeclaration
	// ResponseSchema forces the model to reply with JSON matching the schema.
	ResponseSchema *Schema
	// ThinkingBudget limits reasoning tokens; 0 disables thinking entirely.
	// Nil uses the provider's default behaviour.
	ThinkingBudget *int32
}

type Chunk struct {
	Text         string
	FunctionCall *FunctionCall
	Usage        *Usage
}

type Response struct {
	Text  string
	Usage *Usage
}

type Service interface {
	// StreamChat streams a chat response. A non-nil error ends the stream.
	StreamChat(ctx context.Context, req *Request) iter.Seq2[*Chunk, error]
	// Complete returns a full response in one shot.
	Complete(ctx context.Context, req *Request) (*Response, error)
}

// New returns a Service for the provider named in the config, defaulting to Gemini right now
func New(ctx context.Context) (Service, error) {
	provider := config.GetConfig().LLMProvider
	switch provider {
	case "", "gemini":
		return newGeminiService(ctx)
	default:
		return nil, fmt.Errorf("unknown LLM provider %q", provider)
	}
}
