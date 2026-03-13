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
	"log"
	"strconv"
	"time"

	"github.com/honeycombio/beeline-go"
	"github.com/jmsunseri/bobby-assistant/service/assistant/query"
	"github.com/jmsunseri/bobby-assistant/service/assistant/util"
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
		"both":     "both imperial and metric units (metric first, always followed by imperial in parentheses)",
	}
	units = unitMap[units]
	if units != "" {
		sentence += "Give measurements in " + units + ". Always specify the unit for temperature measurements. Convert units to the user's preference when nececessary. "
	}
	sentence += "Format numbers with commas and/or periods as appropriate for the user's language. "
	var language = util.GetLanguageName(query.PreferredLanguageFromContext(ctx))
	if language != "" {
		sentence += "Respond in " + language + ". "
	} else {
		sentence += "Respond in the language the user is using, unless they specify otherwise."
	}
	return sentence
}

func generateWidgetSentence(ctx context.Context) string {
	if !query.SupportsAnyWidgets(ctx) {
		return ""
	}
	sentence := "You can embed some widgets in your responses by using a special syntax. The following widgets are available:\n"
	if query.SupportsWidget(ctx, "timer") {
		sentence += "<!TIMER targetTime=[time in ISO 8601 format] name=[name of the timer]!>: embeds a timer widget counting down to the given time. If the timer doesn't have a name, the `name` field can be omitted\n" +
			"If a user asks to see a timer, and the timer exists, you should *always* include that timer as a widget at the beginning of your response. Before including a timer widget, you *must* call get_timers first to verify when the timer is set for. Use the TIMER widget *only* when showing the user how long is left on their timer, not when setting one. \n\n"
	}
	if query.SupportsWidget(ctx, "number") {
		sentence += "<!NUMERIC-ANSWER number=[number] unit=[unit]!>: If the primary response to a question is a single number, optionally with a unit (e.g. 'pounds', 'm/s', 'people') or without (e.g. the answer to some arithmetic), you *should* use this widget at the start of your response to highlight the answer. If there is no further clarification after the widget, **do not** provide any text output. **Never** include words (like 'million') in the number - you can put them in the unit (e.g. number '340.1', unit 'million people'). If no unit is necessary, leave it blank. For this widget only, format the number for human readability.\n" +
			"If using a NUMERIC-ANSWER widget, *always* put it at the *start* of the response. **NEVER**, UNDER ANY CIRCUMSTANCES, put a number widget after any text.\n"
	}
	return sentence
}

func (ps *PromptSession) generateSystemPrompt(ctx context.Context) string {
	ctx, span := beeline.StartSpan(ctx, "generate_system_prompt")
	defer span.Send()

	locationString := ""
	location := query.LocationFromContext(ctx)
	if location != nil {
		locationString = "The user is at latitude " + strconv.FormatFloat(location.Lat, 'f', 4, 64) + ", longitude " + strconv.FormatFloat(location.Lon, 'f', 4, 64) + ". "
	} else {
		locationString = "The user has not granted permission to access their location. "
	}

	return "You are a helpful assistant in the style of phone voice assistants. " +
		"Your name is Bobby, and you are running on a Pebble smartwatch. " +
		"The text you receive is transcribed from voice input. " +
		"Your knowledge cutoff is January 2025. " +
		"You may call multiple functions before responding to the user, if necessary. " +
		"If the user asks to set an alarm, assume they always want to set it for a time in the future. " +
		"As a creative, intelligent, helpful, friendly assistant, you should always try to answer the user's question. You can and should provide creative suggestions and factual responses as appropriate. Always try your best to answer the user's question. " +
		"**Never** claim to have taken an action (e.g. set a timer, alarm, or reminder) unless you have actually used a tool to do so. " +
		"Alarms and reminders are not interchangable - *never* use alarms when a user asks for reminders, and never user reminders when the user asks for an alarm or timer. If a user asks to set a timer, always set a timer (using 'set_timer'), not a reminder. If the user asks about a specific timer, respond only about that one. " +
		"If asked to perform language translation (e.g. 'what is X in french?' or 'how do you say X in german?'), *don't* look anything up - just respond immediately. You know how to do translations between any language pair. " +
		"Your responses will be displayed on a very small screen, so be brief. Do not use markdown in your responses.\n" +
		generateWidgetSentence(ctx) +
		generateLanguageSentence(ctx) +
		locationString +
		ps.generateTimeSentence(ctx)
}