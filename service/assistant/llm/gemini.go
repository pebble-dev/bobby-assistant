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
	"errors"
	"iter"
	"strings"

	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
	"google.golang.org/api/iterator"
	"google.golang.org/genai"
)

type geminiService struct {
	client *genai.Client
}

var geminiModels = map[Model]string{
	ModelDefault: "models/gemini-2.5-flash",
	ModelLite:    "models/gemini-2.5-flash-lite",
}

func newGeminiService(ctx context.Context) (Service, error) {
	client, err := genai.NewClient(ctx, &genai.ClientConfig{
		APIKey:  config.GetConfig().GeminiKey,
		Backend: genai.BackendGeminiAPI,
	})
	if err != nil {
		return nil, err
	}
	return &geminiService{client: client}, nil
}

func (g *geminiService) StreamChat(ctx context.Context, req *Request) iter.Seq2[*Chunk, error] {
	return func(yield func(*Chunk, error) bool) {
		s := g.client.Models.GenerateContentStream(ctx, geminiModels[req.Model], toGeminiContents(req.Messages), geminiConfig(req))
		for resp, err := range s {
			if errors.Is(err, iterator.Done) {
				return
			}
			if err != nil {
				yield(nil, err)
				return
			}
			chunk := &Chunk{Usage: fromGeminiUsage(resp.UsageMetadata)}
			if len(resp.Candidates) > 0 && resp.Candidates[0].Content != nil {
				for _, p := range resp.Candidates[0].Content.Parts {
					chunk.Text += p.Text
					if p.FunctionCall != nil {
						chunk.FunctionCall = &FunctionCall{Name: p.FunctionCall.Name, Args: p.FunctionCall.Args}
					}
				}
			}
			if !yield(chunk, nil) {
				return
			}
		}
	}
}

func (g *geminiService) Complete(ctx context.Context, req *Request) (*Response, error) {
	cfg := geminiConfig(req)
	if req.ResponseSchema != nil {
		cfg.ResponseMIMEType = "application/json"
		cfg.ResponseSchema = toGeminiSchema(req.ResponseSchema)
	}
	resp, err := g.client.Models.GenerateContent(ctx, geminiModels[req.Model], toGeminiContents(req.Messages), cfg)
	if err != nil {
		return nil, err
	}
	return &Response{Text: resp.Text(), Usage: fromGeminiUsage(resp.UsageMetadata)}, nil
}

func geminiConfig(req *Request) *genai.GenerateContentConfig {
	cfg := &genai.GenerateContentConfig{
		Temperature:    req.Temperature,
		CandidateCount: 1,
	}
	if req.SystemPrompt != "" {
		cfg.SystemInstruction = &genai.Content{Parts: []*genai.Part{{Text: req.SystemPrompt}}}
	}
	if len(req.Tools) > 0 {
		var decls []*genai.FunctionDeclaration
		for _, t := range req.Tools {
			decls = append(decls, &genai.FunctionDeclaration{
				Name:        t.Name,
				Description: t.Description,
				Parameters:  toGeminiSchema(t.Parameters),
			})
		}
		cfg.Tools = []*genai.Tool{{FunctionDeclarations: decls}}
	}
	if req.ThinkingBudget != nil {
		cfg.ThinkingConfig = &genai.ThinkingConfig{
			IncludeThoughts: false,
			ThinkingBudget:  req.ThinkingBudget,
		}
	}
	return cfg
}

func toGeminiContents(messages []*Content) []*genai.Content {
	var result []*genai.Content
	for _, m := range messages {
		var parts []*genai.Part
		for _, p := range m.Parts {
			gp := &genai.Part{Text: p.Text}
			if p.FunctionCall != nil {
				gp.FunctionCall = &genai.FunctionCall{Name: p.FunctionCall.Name, Args: p.FunctionCall.Args}
			}
			if p.FunctionResponse != nil {
				gp.FunctionResponse = &genai.FunctionResponse{Name: p.FunctionResponse.Name, Response: p.FunctionResponse.Response}
			}
			parts = append(parts, gp)
		}
		result = append(result, &genai.Content{Role: toGeminiRole(m.Role), Parts: parts})
	}
	return result
}

func toGeminiRole(r Role) string {
	if r == RoleAssistant {
		return "model"
	}
	return string(r)
}

func toGeminiSchema(s *Schema) *genai.Schema {
	if s == nil {
		return nil
	}
	out := &genai.Schema{
		Type:        genai.Type(strings.ToUpper(string(s.Type))),
		Format:      s.Format,
		Description: s.Description,
		Enum:        s.Enum,
		Items:       toGeminiSchema(s.Items),
		Required:    s.Required,
		Nullable:    s.Nullable,
	}
	if s.Properties != nil {
		out.Properties = make(map[string]*genai.Schema, len(s.Properties))
		for k, v := range s.Properties {
			out.Properties[k] = toGeminiSchema(v)
		}
	}
	return out
}

func fromGeminiUsage(u *genai.GenerateContentResponseUsageMetadata) *Usage {
	if u == nil {
		return nil
	}
	return &Usage{
		InputTokens:       int(u.PromptTokenCount),
		CachedInputTokens: int(u.CachedContentTokenCount),
		OutputTokens:      int(u.CandidatesTokenCount),
	}
}
