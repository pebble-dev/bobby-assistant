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
	"github.com/google/uuid"
	"github.com/honeycombio/beeline-go"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util"
	"github.com/redis/go-redis/v9"
	"google.golang.org/genai"
	"time"
)

type SerializedMessage struct {
	Role             string                  `json:"role"`
	Content          string                  `json:"content"`
	FunctionCall     *genai.FunctionCall     `json:"functionCall,omitempty"`
	FunctionResponse *genai.FunctionResponse `json:"functionResponse,omitempty"`
}

type StoredContext struct {
	PoiQuery  *util.POIQuery `json:"poiQuery"`
	POIs      []util.POI     `json:"pois"`
	LastRoute map[string]any `json:"lastRoute"`
}

type ThreadContext struct {
	ThreadId       uuid.UUID           `json:"threadId"`
	Messages       []SerializedMessage `json:"messages"`
	ContextStorage StoredContext       `json:"contextStorage"`
}

func NewContext() *ThreadContext {
	return &ThreadContext{}
}

func LoadThread(ctx context.Context, r *redis.Client, id string) (*ThreadContext, error) {
	ctx, span := beeline.StartSpan(ctx, "load_thread")
	defer span.Send()
	j, err := r.Get(ctx, "thread:"+id).Result()
	if err != nil {
		return nil, err
	}
	var threadContext ThreadContext
	if err := json.Unmarshal([]byte(j), &threadContext); err != nil {
		return nil, err
	}
	return &threadContext, nil
}

func StoreThread(ctx context.Context, r *redis.Client, thread *ThreadContext) error {
	ctx, span := beeline.StartSpan(ctx, "store_thread")
	defer span.Send()
	j, err := json.Marshal(thread)
	if err != nil {
		span.AddField("error", err)
		return err
	}
	r.Set(ctx, "thread:"+thread.ThreadId.String(), j, 10*time.Minute)
	return nil
}
