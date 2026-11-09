// Copyright 2026 Rebble Foundation
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

// Package consent defines the consent text shown on the watch before Bobby can be used.
// The text lives here, not in the app, so the LLM provider can change without an app release.
package consent

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"strconv"

	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
)

// MaxPageBytes limits the title and text of each page so a page fits in one AppMessage.
const MaxPageBytes = 900

type Page struct {
	Title string `json:"title"`
	Text  string `json:"text"`
}

type Consent struct {
	// Version must only ever increase. Any increase makes every client re-consent before its next query.
	// Version 1 is the text bundled into app versions up to 1.5, which existing users have already agreed to.
	Version int `json:"version"`
	// Pages are shown in order and dismissed with the select button.
	Pages []Page `json:"pages"`
	// Location is the final page. It asks the user to allow or deny location access.
	Location Page `json:"location"`
}

var llmWarning = Page{
	Title: "Important",
	Text:  "Bobby uses an AI language model to respond to your requests. Like all other AI language models, Bobby may lie, do the wrong thing, or make offensive or inappropriate comments.",
}

// byProvider maps config.LLMProvider to the consent text for that provider.
// Add an entry, with a higher version, when adding a provider.
var byProvider = map[string]*Consent{
	"gemini": {
		Version: 1,
		Pages: []Page{
			llmWarning,
			{
				Title: "Privacy",
				Text:  "Bobby uses Google's Gemini API to process requests. To do this, all of your requests will be sent verbatim to Google. Google does not use these requests for training or for any other purpose. If you do not agree, you cannot use Bobby.",
			},
		},
		Location: Page{
			Title: "Location",
			Text:  "In order to provide contextually relevant information, Bobby would like to use your precise location. The name of the city you are in will be sent to Google's Gemini along with your requests. This decision can be changed later in app's config page.\n\nCan Bobby access your location?",
		},
	},
}

var current *Consent

func init() {
	provider := config.GetConfig().LLMProvider
	if provider == "" {
		provider = "gemini"
	}
	c, ok := byProvider[provider]
	if !ok {
		log.Fatalf("no consent text defined for LLM provider %q", provider)
	}
	if err := validate(c); err != nil {
		log.Fatalf("invalid consent text for LLM provider %q: %v", provider, err)
	}
	current = c
}

func validate(c *Consent) error {
	if c.Version < 1 {
		return fmt.Errorf("version must be at least 1, got %d", c.Version)
	}
	for i, p := range append(append([]Page{}, c.Pages...), c.Location) {
		if p.Title == "" || p.Text == "" {
			return fmt.Errorf("page %d has an empty title or text", i)
		}
		if len(p.Title)+len(p.Text) > MaxPageBytes {
			return fmt.Errorf("page %d is %d bytes, more than the %d allowed", i, len(p.Title)+len(p.Text), MaxPageBytes)
		}
	}
	return nil
}

// Current returns the consent text for the configured LLM provider.
func Current() *Consent {
	return current
}

// Problem says why a query can't go ahead without (re-)consent.
type Problem int

const (
	// OK means the client has accepted the current consent version.
	OK Problem = iota
	// UpdateRequired means the client predates server-provided consent and can't show the current text.
	UpdateRequired
	// ConsentRequired means the client must show the current consent text before querying again.
	ConsentRequired
)

// Check compares the consentVersion query parameter a client sent with the current consent version.
func Check(versionParam string) Problem {
	required := Current().Version
	if versionParam == "" {
		// Older apps don't report a consent version. They have agreed to version 1, which was bundled into the app.
		if required > 1 {
			return UpdateRequired
		}
		return OK
	}
	accepted, err := strconv.Atoi(versionParam)
	if err != nil || accepted < required {
		return ConsentRequired
	}
	return OK
}

// HandleConsent serves the current consent text as JSON.
func HandleConsent(rw http.ResponseWriter, r *http.Request) {
	rw.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(rw).Encode(Current()); err != nil {
		log.Printf("Error encoding consent response: %v", err)
	}
}
