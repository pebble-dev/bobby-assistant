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
	"errors"
	"fmt"
	"github.com/honeycombio/beeline-go"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/mapbox"
	"log"
	"net/url"
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
		"uk":       "UK hybrid units (temperature in Celsius, wind speed in mph, etc.)",
		"both":     "both imperial and metric units",
	}
	units = unitMap[units]
	if units != "" {
		sentence += "Give measurements in " + units + ". Always specify the unit for temperature measurements."
	}
	var language = util.GetLanguageName(query.PreferredLanguageFromContext(ctx))
	if language != "" {
		sentence += "Respond in " + language + ". "
	}
	return sentence
}

func (ps *PromptSession) getPlaceFromLocation(ctx context.Context) (string, error) {
	// Use the Mapbox API to turn the user's longitude and latitude into a place name.
	// We don't want anything more specific than their town name, so we filter at that level ("place" in Mapbox terms).
	// We will return just a region or country if there isn't a nearby place.
	location := query.LocationFromContext(ctx)
	collection, err := mapbox.GeocodingRequest(ctx, fmt.Sprintf("%f,%f", location.Lon, location.Lat), url.Values{"types": {"place,region,country"}})
	if err != nil {
		return "", err
	}
	if len(collection.Features) == 0 {
		return "", errors.New("the user isn't anywhere")
	}
	return collection.Features[0].PlaceName, nil
}

func (ps *PromptSession) generateSystemPrompt(ctx context.Context) string {
	ctx, span := beeline.StartSpan(ctx, "generate_system_prompt")
	defer span.Send()
	locationString := ""
	location := query.LocationFromContext(ctx)
	if location != nil {
		if place, err := ps.getPlaceFromLocation(ctx); err == nil {
			locationString = "The user is in " + place + ". "
		} else {
			span.AddField("error", err)
			log.Printf("Failed to get user location: %v", err)
		}
	} else {
		locationString = "The user has not granted permission to access their location, but they could enable it on the settings page if needed. "
	}
	return "You are a helpful assistant in the style of phone voice assistants. " +
		"Your name is Bobby, and you are running on a Pebble smartwatch. " +
		"The text you receive is transcribed from voice input. " +
		"Your knowledge cutoff is September 2024. However, you can use the wikipedia function to access the current content of specific Wikipedia pages. " +
		"Always follow Wikipedia redirects immediately and silently. Never ask the user whether you should check wikipedia - if you would ask, assume that you should. Don't mention looking up articles or Wikipedia to the user. " +
		locationString +
		ps.generateTimeSentence(ctx) +
		"You may call multiple functions before responding to the user, if necessary. If executing a lua script fails, try hard to fix the script using the error message, and consider alternate approaches to solve the problem. " +
		"If the user asks to set an alarm, assume they always want to set it for a time in the future. " +
		"As a creative, intelligent, helpful, friendly assistant, you should always try to answer the user's question. You can and should provide creative suggestions and factual responses as appropriate. Always try your best to answer the user's question. " +
		"**Never** claim to have taken an action (e.g. set a timer, alarm, or reminder) unless you have actually used a tool to do so. " +
		"Even if in previous turns you have apparently taken an action (like setting an alarm) without using a tool, you must still use tools if asked to do so again. " +
		"Your responses will be displayed on a very small screen, so be brief. Do not use markdown in your responses. " +
		generateLanguageSentence(ctx)
}
