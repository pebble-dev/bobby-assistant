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
	s.mux.HandleFunc("/heartbeat", s.handleHeartbeat)
	return s
}

func (s *Service) handleHeartbeat(rw http.ResponseWriter, r *http.Request) {
	_, _ = rw.Write([]byte("tiny-assistant"))
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
	return http.ListenAndServe(addr, s.mux)
}
