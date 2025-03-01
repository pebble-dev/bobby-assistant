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
	"context"
	"github.com/honeycombio/beeline-go"
	"log"
	"strconv"
	"time"

	"github.com/pebble-dev/bobby-assistant/service/assistant/query"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util"
)

func (ps *PromptSession) generateTimeSentence(ctx context.Context) string {
	tzOffset := ps.query.Get("tzOffset")
	tzOffsetInt, err := strconv.Atoi(tzOffset)
	if err != nil {
		log.Printf("Failed to parse tzOffset: %v", err)
		return ""
	}
	// tzOffset is in minutes, but Go wants seconds.
	now := time.Now().UTC().In(time.FixedZone("local", tzOffsetInt*60))
	return "The user's local time is " + now.Format("Mon, 2 Jan 2006 15:04:05-07:00") + ". "
}

func generateLanguageSentence(ctx context.Context) string {
	sentence := ""
	var units = query.PreferredUnitsFromContext(ctx)
	unitMap := map[string]string{
		"imperial": "imperial units",
		"metric":   "metric units",
		"uk":       "UK hybrid units",
		"both":     "both imperial and metric units",
	}
	units = unitMap[units]
	if units != "" {
		sentence += "Give measurements in " + units + ". "
	}
	var language = util.GetLanguageName(query.PreferredLanguageFromContext(ctx))
	if language != "" {
		sentence += "Respond in " + language + ". "
	}
	return sentence
}

func (ps *PromptSession) generateSystemPrompt(ctx context.Context) string {
	ctx, span := beeline.StartSpan(ctx, "generate_system_prompt")
	defer span.Send()
	return "You are a helpful assistant in the style of phone voice assistants. " +
		"Your name is Bobby, and you are running on a Pebble smartwatch. " +
		"The text you receive is transcribed from voice input. " +
		"Your knowledge cutoff is September 2024. " +
		ps.generateTimeSentence(ctx) +
		"You may call multiple functions before responding to the user, if necessary. If executing a lua script fails, try hard to fix the script using the error message, and consider alternate approaches to solve the problem. " +
		"If the user asks to set an alarm, assume they always want to set it for a time in the future. " +
		"As a creative, intelligent, helpful, friendly assistant, you should always try to answer the user's question. You can and should provide creative suggestions and factual responses as appropriate. Always try your best to answer the user's question. " +
		"**Never** claim to have taken an action (e.g. set a timer, alarm, or reminder) unless you have actually used a tool to do so. " +
		"Your responses will be displayed on a very small screen, so be brief. Do not use markdown in your responses. " +
		generateLanguageSentence(ctx)
}
