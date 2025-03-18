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
	"github.com/honeycombio/beeline-go/wrappers/hnynethttp"
	"github.com/pebble-dev/bobby-assistant/service/assistant/feedback"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
	"log"
	"net/http"

	"github.com/redis/go-redis/v9"
)

type Service struct {
	mux   *http.ServeMux
	redis *redis.Client
}

func NewService(r *redis.Client) *Service {
	s := &Service{
		mux:   http.NewServeMux(),
		redis: r,
	}
	s.mux.HandleFunc("/query", s.handleQuery)
	s.mux.HandleFunc("/quota", s.handleQuota)
	s.mux.HandleFunc("/heartbeat", s.handleHeartbeat)
	s.mux.HandleFunc("/feedback", feedback.HandleFeedback)
	return s
}

func (s *Service) handleHeartbeat(rw http.ResponseWriter, r *http.Request) {
	_, _ = rw.Write([]byte("bobby"))
}

func (s *Service) handleQuota(rw http.ResponseWriter, r *http.Request) {
	ctx := r.Context()
	token := r.URL.Query().Get("token")
	if token == "" {
		log.Printf("No token provided.")
		http.Error(rw, "No token provided.", http.StatusNotFound)
		return
	}
	userInfo, err := quota.GetUserInfo(ctx, token)
	if err != nil {
		log.Printf("Error getting user info: %v", err)
		http.Error(rw, err.Error(), http.StatusNotFound)
		return
	}
	if !userInfo.HasSubscription {
		response, err := json.Marshal(map[string]interface{}{
			"used":            0,
			"remaining":       0,
			"hasSubscription": false,
		})
		if err != nil {
			log.Printf("Error marshalling quota response: %v", err)
			http.Error(rw, err.Error(), http.StatusInternalServerError)
			return
		}
		_, _ = rw.Write(response)
		return
	}
	qt := quota.NewTracker(s.redis, userInfo.UserId)
	used, remaining, err := qt.GetQuota(ctx)
	if err != nil {
		log.Printf("Error getting quota: %v", err)
		http.Error(rw, err.Error(), http.StatusInternalServerError)
		return
	}
	response, err := json.Marshal(map[string]any{
		"used":            used,
		"remaining":       remaining,
		"hasSubscription": true,
	})
	if err != nil {
		log.Printf("Error marshalling quota response: %v", err)
		http.Error(rw, err.Error(), http.StatusInternalServerError)
		return
	}
	_, _ = rw.Write(response)
}

func (s *Service) handleQuery(rw http.ResponseWriter, r *http.Request) {
	session, err := NewPromptSession(s.redis, rw, r)
	if err != nil {
		log.Printf("Creating session failed: %v", err)
		http.Error(rw, err.Error(), http.StatusInternalServerError)
		return
	}
	session.Run(r.Context())
}

func (s *Service) ListenAndServe(addr string) error {
	return http.ListenAndServe(addr, hnynethttp.WrapHandler(s.mux))
}
