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
	"bytes"
	"encoding/json"
	"golang.org/x/text/language"
	"golang.org/x/text/message"
	"io"
	"log"
	"net/http"
	"os"
	"runtime/debug"
	"time"

	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/storage"
)

type discordField struct {
	Name   string `json:"name"`
	Value  string `json:"value"`
	Inline bool   `json:"inline"`
}

type discordEmbed struct {
	Title       string         `json:"title"`
	Description string         `json:"description"`
	Color       int            `json:"color"`
	Fields      []discordField `json:"fields"`
	Timestamp   time.Time      `json:"timestamp"`
}

type discordMessage struct {
	Content   string         `json:"content"`
	Embeds    []discordEmbed `json:"embeds"`
	Username  string         `json:"username"`
	AvatarUrl string         `json:"avatar_url"`
}

type feedbackRequest struct {
	Text               string `json:"text"`
	AppVersion         string `json:"appVersion"`
	AlarmCount         int    `json:"alarmCount"`
	LocationEnabled    bool   `json:"locationEnabled"`
	LocationReady      bool   `json:"locationReady"`
	UnitPreference     string `json:"unitPreference"`
	LanguagePreference string `json:"languagePreference"`
	ReminderCount      int    `json:"reminderCount"`
	JsVersion          string `json:"jsVersion"`
	Timezone           int    `json:"timezone"`
	Platform           string `json:"platform"`
	AuthToken          string `json:"timelineToken"`
}

func HandleFeedback(rw http.ResponseWriter, r *http.Request) {
	ctx := r.Context()

	// Parse the request body
	var req feedbackRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		log.Printf("Error decoding feedback request: %v", err)
		http.Error(rw, err.Error(), http.StatusBadRequest)
		return
	}
	// Authenticate the request
	userInfo, err := quota.GetUserInfo(ctx, req.AuthToken)
	if err != nil {
		log.Printf("Error getting user info: %v", err)
		http.Error(rw, err.Error(), http.StatusUnauthorized)
		return
	}
	quotaUsed, _, err := quota.NewTracker(storage.GetRedis(), userInfo.UserId).GetQuota(ctx)
	if err != nil {
		quotaUsed = 0
	}

	serverName, err := os.Hostname()
	if err != nil {
		serverName = "unknown"
	}
	commitHash := "unknown"
	buildInfo, ok := debug.ReadBuildInfo()
	if ok {
		for _, v := range buildInfo.Settings {
			if v.Key == "vcs.revision" {
				commitHash = v.Value
				break
			}
		}
	}
	if len(commitHash) > 8 {
		commitHash = commitHash[:8]
	}

	p := message.NewPrinter(language.English)

	embed := discordMessage{
		Embeds: []discordEmbed{{
			Title:       "Feedback Received",
			Description: req.Text,
			Color:       0xffa9ff,
			Fields: []discordField{
				{
					Name:   "User",
					Value:  p.Sprintf("ID: %d\nSubscribed: %t\nTZ: %d", userInfo.UserId, userInfo.HasSubscription, req.Timezone),
					Inline: true,
				},
				{
					Name:   "App",
					Value:  p.Sprintf("Version: %s/%s\nPlatform: %s", req.AppVersion, req.JsVersion, req.Platform),
					Inline: true,
				},
				{
					Name:   "Settings",
					Value:  p.Sprintf("Units: %q\nLanguage: %q", req.UnitPreference, req.LanguagePreference),
					Inline: true,
				},
				{
					Name:   "Usage",
					Value:  p.Sprintf("Quota: %d\nAlarms: %d\nReminders: %d", quotaUsed, req.AlarmCount, req.ReminderCount),
					Inline: true,
				},
				{
					Name:   "Location",
					Value:  p.Sprintf("Enabled: %t\nReady: %t", req.LocationEnabled, req.LocationReady),
					Inline: true,
				},
				{
					Name:   "Server",
					Value:  p.Sprintf("Name: %s\nCommit: %s", serverName, commitHash),
					Inline: true,
				},
			},
		}},
		Username:  "Bobby Feedback",
		AvatarUrl: "https://assets2.rebble.io/144x144/67c3afe9d2acb30009a3c7cd",
	}

	marshalled, err := json.Marshal(embed)
	if err != nil {
		log.Printf("Error marshalling feedback: %v", err)
		http.Error(rw, err.Error(), http.StatusInternalServerError)
		return
	}
	reader := bytes.NewReader(marshalled)

	// Send the feedback to Discord
	url := config.GetConfig().DiscordFeedbackURL
	result, err := http.Post(url, "application/json", reader)
	if err != nil {
		log.Printf("Error sending feedback: %v", err)
		http.Error(rw, err.Error(), http.StatusInternalServerError)
		return
	}
	defer result.Body.Close()
	if result.StatusCode < 200 || result.StatusCode >= 300 {
		content, _ := io.ReadAll(result.Body)
		log.Printf("Error sending feedback: %s\n%s", result.Status, string(content))
		http.Error(rw, "Error sending feedback", http.StatusInternalServerError)
		return
	}
}
