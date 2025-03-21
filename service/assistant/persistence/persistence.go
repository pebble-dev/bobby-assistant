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

package persistence

import (
	"context"
	"encoding/json"
	"github.com/redis/go-redis/v9"
	"google.golang.org/genai"
)

type SerializedMessage struct {
	Role             string                  `json:"role"`
	Content          string                  `json:"content"`
	FunctionCall     *genai.FunctionCall     `json:"functionCall,omitempty"`
	FunctionResponse *genai.FunctionResponse `json:"functionResponse,omitempty"`
}

func LoadThread(context context.Context, r *redis.Client, id string) ([]SerializedMessage, error) {
	j, err := r.Get(context, "thread:"+id).Result()
	if err != nil {
		return nil, err
	}
	var messages []SerializedMessage
	if err := json.Unmarshal([]byte(j), &messages); err != nil {
		return nil, err
	}
	return messages, nil
}
