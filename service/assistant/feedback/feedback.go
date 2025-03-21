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

package feedback

import (
	"context"
	"encoding/json"
	"fmt"
	"github.com/google/uuid"
	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
	"github.com/pebble-dev/bobby-assistant/service/assistant/persistence"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/storage"
	"github.com/redis/go-redis/v9"
	"log"
	"net/http"
	"time"
)

type feedbackRequest struct {
	feedbackMetadata
	Text string `json:"text"`
}

type reportRequest struct {
	feedbackMetadata
	ThreadUUID string `json:"thread_uuid"`
}

type ReportedThread struct {
	OriginalThreadID string                          `json:"original_thread_id"`
	ReportTime       time.Time                       `json:"report_time"`
	ThreadContent    []persistence.SerializedMessage `json:"thread_content"`
}

func HandleFeedback(rw http.ResponseWriter, r *http.Request) {
	ctx := r.Context()

	var req feedbackRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		log.Printf("Error decoding feedback request: %v", err)
		http.Error(rw, err.Error(), http.StatusBadRequest)
		return
	}

	if err := sendToDiscord(ctx, "Feedback Received", req.Text, req.feedbackMetadata); err != nil {
		log.Printf("Error sending feedback to Discord: %v", err)
		http.Error(rw, err.Error(), http.StatusInternalServerError)
		return
	}
}

func HandleReport(rw http.ResponseWriter, r *http.Request) {
	ctx := r.Context()

	var req reportRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		log.Printf("Error decoding report request: %v", err)
		http.Error(rw, err.Error(), http.StatusBadRequest)
		return
	}

	rd := storage.GetRedis()
	messages, err := persistence.LoadThread(ctx, rd, req.ThreadUUID)
	if err != nil {
		log.Printf("Error loading thread: %v", err)
		http.Error(rw, err.Error(), http.StatusGone)
		return
	}

	report := ReportedThread{
		OriginalThreadID: req.ThreadUUID,
		ReportTime:       time.Now(),
		ThreadContent:    messages,
	}

	reportId, err := storeReport(ctx, rd, report)
	if err != nil {
		http.Error(rw, err.Error(), http.StatusInternalServerError)
		return
	}

	dm := fmt.Sprintf("A user has reported a bad thread: %s/reported-thread/%s", config.GetConfig().BaseURL, reportId)
	if err := sendToDiscord(ctx, "Report Received", dm, req.feedbackMetadata); err != nil {
		log.Printf("Error sending report to Discord: %v", err)
		http.Error(rw, err.Error(), http.StatusInternalServerError)
		return
	}
}

func storeReport(ctx context.Context, rd *redis.Client, report ReportedThread) (string, error) {
	reportId := uuid.New()

	j, err := json.Marshal(report)
	if err != nil {
		log.Printf("Error marshalling report: %v", err)
		return "", fmt.Errorf("Error marshalling report: %w", err)
	}
	rd.Set(ctx, "reported-thread:"+reportId.String(), j, 0)
	return reportId.String(), nil
}

func loadReport(ctx context.Context, rd *redis.Client, report string) (ReportedThread, error) {
	j, err := rd.Get(ctx, "reported-thread:"+report).Result()
	if err != nil {
		return ReportedThread{}, err
	}
	var rt ReportedThread
	if err := json.Unmarshal([]byte(j), &rt); err != nil {
		return ReportedThread{}, err
	}
	return rt, nil
}
