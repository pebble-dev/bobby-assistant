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

package functions

import (
	"context"
	"github.com/honeycombio/beeline-go"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
	"log"
	"maps"

	"github.com/pebble-dev/bobby-assistant/service/assistant/query"
	"google.golang.org/genai"
)

type AlarmInput struct {
	// If setting an alarm, the time to schedule the alarm for in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'. Required for alarms. Must always be in the future.
	Time string `json:"time"`
	// If setting a timer, the number of seconds to set the timer for. Required for timers.
	Duration int `json:"duration_seconds"`
	// True if this is a timer, false if it's an alarm.
	IsTimer bool `json:"is_timer" jsonschema:"required"`
	// An optional name for the alarm or timer.
	Name string `json:"name"`
}

type DeleteAlarmInput struct {
	// The time of the alarm or timer to delete in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'.
	Time string `json:"time"`
	// True if deleting a timer, false if deleting an alarm.
	IsTimer bool `json:"is_timer"`
}

type GetAlarmInput struct {
	// True if retrieving timers, false if returning alarms.
	IsTimer bool `json:"is_timer" jsonschema:"required"`
}

func init() {
	params := genai.Schema{
		Type:     genai.TypeObject,
		Nullable: false,
		Properties: map[string]*genai.Schema{
			"time": {
				Type:        genai.TypeString,
				Description: "If setting an alarm, the time to schedule the alarm for in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'. Required for alarms. Must always be in the future.",
				Nullable:    true,
			},
			"duration_seconds": {
				Type:        genai.TypeInteger,
				Description: "If setting a timer, the number of seconds to set the timer for. Required for timers.",
				Nullable:    true,
				Format:      "int32",
			},
			"is_timer": {
				Type:        genai.TypeBoolean,
				Description: "True if this is a timer, false if it's an alarm.",
				Nullable:    false,
			},
		},
		Required: []string{"is_timer"},
	}
	// This registration is for old watch apps that don't support named alarms. The anticapability prevents it
	// from being seen by newer apps.
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "set_alarm",
			Description: "Get or set an alarm or a timer for a given time.",
			Parameters:  &params,
		},
		Cb:             alarmImpl,
		Thought:        alarmThought,
		InputType:      AlarmInput{},
		AntiCapability: "named_alarms",
	})

	paramsWithNames := params
	paramsWithNames.Properties = maps.Clone(params.Properties)
	paramsWithNames.Properties["name"] = &genai.Schema{
		Type:        genai.TypeString,
		Description: "Only if explicitly specified by the user, the name of the alarm or timer. Use title case. If the user didn't ask to name the timer, just leave it empty.",
		Nullable:    true,
	}
	// This registration is for new watch apps that support named alarms. The capability prevents the option for
	// naming being presented to the model for older apps.
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "set_alarm",
			Description: "Get or set an alarm or a timer for a given time.",
			Parameters:  &paramsWithNames,
		},
		Cb:         alarmImpl,
		Thought:    alarmThought,
		InputType:  AlarmInput{},
		Capability: "named_alarms",
	})

	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "get_alarms",
			Description: "Get any existing alarms or timers. **There is no get_timers, call this with is_timer=true instead.**",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: false,
				Properties: map[string]*genai.Schema{
					"is_timer": {
						Type:        genai.TypeBoolean,
						Description: "True if retrieving timers, false if returning alarms.",
						Nullable:    false,
					},
				},
				Required: []string{"is_timer"},
			},
		},
		Aliases:   []string{"get_alarms", "get_timer", "get_timers"},
		Cb:        getAlarmImpl,
		Thought:   getAlarmThought,
		InputType: GetAlarmInput{},
	})

	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "delete_alarm",
			Description: "Delete a specific alarm or timer by its expiration time. When deleting a timer, you must call get_alarm first to get the expiration time (calculating it from the chat history will only be approximate)",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: false,
				Properties: map[string]*genai.Schema{
					"time": {
						Type:        genai.TypeString,
						Description: "The time of the alarm or timer to delete in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'.",
						Nullable:    false,
					},
					"is_timer": {
						Type:        genai.TypeBoolean,
						Description: "True if deleting a timer, false if deleting an alarm.",
						Nullable:    false,
					},
				},
			},
		},
		Cb:        deleteAlarmImpl,
		Thought:   deleteAlarmThought,
		InputType: DeleteAlarmInput{},
	})
}

func alarmImpl(ctx context.Context, quotaTracker *quota.Tracker, args interface{}, requests chan<- map[string]interface{}, responses <-chan map[string]interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "set_alarm")
	defer span.Send()
	if !query.SupportsAction(ctx, "set_alarm") {
		return Error{Error: "You need to update the app on your watch to set alarms or timers."}
	}
	input := args.(*AlarmInput)
	log.Println("Asking watch to set an alarm...")
	requests <- map[string]interface{}{
		"time":     input.Time,
		"duration": input.Duration,
		"isTimer":  input.IsTimer,
		"name":     input.Name,
		"action":   "set_alarm",
		"cancel":   false,
	}
	log.Println("Waiting for confirmation...")
	resp := <-responses
	return resp
}

func alarmThought(i interface{}) string {
	args := i.(*AlarmInput)
	if args.IsTimer {
		return "Setting a timer"
	} else if args.Time == "" {
		return "Contemplating time"
	} else {
		return "Setting an alarm"
	}
}

func deleteAlarmImpl(ctx context.Context, quotaTracker *quota.Tracker, args interface{}, requests chan<- map[string]interface{}, responses <-chan map[string]interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "set_alarm")
	defer span.Send()
	if !query.SupportsAction(ctx, "set_alarm") {
		return Error{Error: "You need to update the app on your watch to delete alarms or timers."}
	}
	input := args.(*DeleteAlarmInput)
	log.Printf("Asking watch to delete an alarm set for %s...\n", input.Time)
	requests <- map[string]interface{}{
		"time":    input.Time,
		"isTimer": input.IsTimer,
		"action":  "set_alarm",
		"cancel":  true,
	}
	log.Println("Waiting for confirmation...")
	resp := <-responses
	return resp
}

func deleteAlarmThought(i interface{}) string {
	args := i.(*DeleteAlarmInput)
	if args.IsTimer {
		return "Deleting a timer"
	} else {
		return "Deleting an alarm"
	}
}

func getAlarmImpl(ctx context.Context, quotaTracker *quota.Tracker, args interface{}, requests chan<- map[string]interface{}, responses <-chan map[string]interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "get_alarm")
	defer span.Send()
	if !query.SupportsAction(ctx, "get_alarm") {
		return Error{Error: "You need to update the app on your watch to get alarms or timers."}
	}
	input := args.(*GetAlarmInput)
	log.Println("Asking watch to get alarms...")
	requests <- map[string]interface{}{
		"isTimer": input.IsTimer,
		"action":  "get_alarm",
	}
	log.Println("Waiting for response...")
	resp := <-responses
	log.Println("Got response:", resp)
	return resp
}

func getAlarmThought(i interface{}) string {
	args := i.(*GetAlarmInput)
	if args.IsTimer {
		return "Checking your timers"
	}
	return "Checking your alarms"
}
