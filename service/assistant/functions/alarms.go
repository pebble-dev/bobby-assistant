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
	// The time to schedule the alarm for in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'. Must always be in the future.
	Time string `json:"time"`
	// An optional name for the alarm.
	Name string `json:"name"`
}

type TimerInput struct {
	// If setting a timer, the number of seconds to set the timer for. Required for timers.
	Duration int `json:"duration_seconds"`
	// These aren't exposed, but sometimes the model decides to use them anyway.
	DurationMinutes int `json:"duration_minutes"`
	DurationHours   int `json:"duration_hours"`
	// An optional name for the alarm or timer.
	Name string `json:"name"`
}

type DeleteAlarmInput struct {
	// The time of the alarm to delete in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'.
	Time string `json:"time"`
}

type DeleteTimerInput struct {
	// The expiration time of the timer to delete in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'.
	Time string `json:"time"`
}

type Empty struct{}

func init() {
	params := genai.Schema{
		Type:     genai.TypeObject,
		Nullable: false,
		Properties: map[string]*genai.Schema{
			"time": {
				Type:        genai.TypeString,
				Description: "The time to schedule the alarm for in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'. Must always be in the future.",
				Nullable:    true,
			},
		},
	}
	// This registration is for old watch apps that don't support named alarms. The anticapability prevents it
	// from being seen by newer apps.
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "set_alarm",
			Description: "Set an alarm for a given time.",
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
		Description: "Only if explicitly specified by the user, the name of the alarm. Use title case. If the user didn't ask to name the alarm, just leave it empty.",
		Nullable:    true,
	}
	// This registration is for new watch apps that support named alarms. The capability prevents the option for
	// naming being presented to the model for older apps.
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "set_alarm",
			Description: "Set an alarm for a given time.",
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
			Description: "Get any existing alarms.",
		},
		Aliases:   []string{"get_alarm"},
		Cb:        getAlarmImpl,
		Thought:   getAlarmThought,
		InputType: Empty{},
	})

	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "delete_alarm",
			Description: "Delete a specific alarm by its expiration time.",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: false,
				Properties: map[string]*genai.Schema{
					"time": {
						Type:        genai.TypeString,
						Description: "The time of the alarm to delete in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'.",
						Nullable:    false,
					},
				},
			},
		},
		Cb:        deleteAlarmImpl,
		Thought:   deleteAlarmThought,
		InputType: DeleteAlarmInput{},
	})
	timerParams := genai.Schema{
		Type:     genai.TypeObject,
		Nullable: false,
		Properties: map[string]*genai.Schema{
			"duration_seconds": {
				Type:        genai.TypeInteger,
				Description: "The number of seconds to set the timer for.",
				Nullable:    true,
				Format:      "int32",
			},
		},
	}
	// This registration is for old watch apps that don't support named alarms. The anticapability prevents it
	// from being seen by newer apps.
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "set_timer",
			Description: "Set a timer for a given duration.",
			Parameters:  &timerParams,
		},
		Cb:             timerImpl,
		Thought:        timerThought,
		InputType:      TimerInput{},
		AntiCapability: "named_alarms",
	})
	timerParamsWithNames := timerParams
	timerParamsWithNames.Properties = maps.Clone(timerParams.Properties)
	timerParamsWithNames.Properties["name"] = &genai.Schema{
		Type:        genai.TypeString,
		Description: "Only if explicitly specified by the user, the name of the timer. Use title case. If the user didn't ask to name the timer, just leave it empty.",
		Nullable:    true,
	}
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "set_timer",
			Description: "Set a timer for a given time.",
			Parameters:  &timerParamsWithNames,
		},
		Cb:         timerImpl,
		Thought:    timerThought,
		InputType:  TimerInput{},
		Capability: "named_alarms",
	})

	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "get_timers",
			Description: "Get any existing timers.",
		},
		Aliases:   []string{"get_timer"},
		Cb:        getTimerImpl,
		Thought:   getTimerThought,
		InputType: Empty{},
	})

	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "delete_timer",
			Description: "Delete a specific timer by its expiration time.",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: false,
				Properties: map[string]*genai.Schema{
					"time": {
						Type:        genai.TypeString,
						Description: "The time of the alarm to delete in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'.",
						Nullable:    false,
					},
				},
			},
		},
		Cb:        deleteTimerImpl,
		Thought:   deleteTimerThought,
		InputType: DeleteTimerInput{},
	})
}

func alarmImpl(ctx context.Context, quotaTracker *quota.Tracker, args interface{}, requests chan<- map[string]interface{}, responses <-chan map[string]interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "set_alarm")
	defer span.Send()
	if !query.SupportsAction(ctx, "set_alarm") {
		return Error{Error: "You need to update the app on your watch to set alarms."}
	}
	input := args.(*AlarmInput)
	log.Println("Asking watch to set an alarm...")
	requests <- map[string]interface{}{
		"time":    input.Time,
		"isTimer": false,
		"name":    input.Name,
		"action":  "set_alarm",
		"cancel":  false,
	}
	log.Println("Waiting for confirmation...")
	resp := <-responses
	return resp
}

func timerImpl(ctx context.Context, quotaTracker *quota.Tracker, args interface{}, requests chan<- map[string]interface{}, responses <-chan map[string]interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "set_timer")
	defer span.Send()
	if !query.SupportsAction(ctx, "set_alarm") {
		return Error{Error: "You need to update the app on your watch to set timers."}
	}
	input := args.(*TimerInput)
	log.Println("Asking watch to set an alarm...")
	duration := input.Duration + input.DurationMinutes*60 + input.DurationHours*3600
	if duration == 0 {
		return Error{Error: "You need to pass the timer duration in seconds to duration_seconds (e.g. duration_seconds=300 for a 5 minute timer)."}
	}
	requests <- map[string]interface{}{
		"duration": duration,
		"isTimer":  true,
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
	if args.Time == "" {
		return "Contemplating time"
	} else {
		return "Setting an alarm"
	}
}

func timerThought(i interface{}) string {
	args := i.(*TimerInput)
	if args.Duration == 0 {
		return "Contemplating time"
	} else {
		return "Setting a timer"
	}
}

func deleteAlarmImpl(ctx context.Context, quotaTracker *quota.Tracker, args interface{}, requests chan<- map[string]interface{}, responses <-chan map[string]interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "delete_alarm")
	defer span.Send()
	if !query.SupportsAction(ctx, "set_alarm") {
		return Error{Error: "You need to update the app on your watch to delete alarms."}
	}
	input := args.(*DeleteAlarmInput)
	log.Printf("Asking watch to delete an alarm set for %s...\n", input.Time)
	requests <- map[string]interface{}{
		"time":    input.Time,
		"isTimer": false,
		"action":  "set_alarm",
		"cancel":  true,
	}
	log.Println("Waiting for confirmation...")
	resp := <-responses
	return resp
}

func deleteTimerImpl(ctx context.Context, quotaTracker *quota.Tracker, args interface{}, requests chan<- map[string]interface{}, responses <-chan map[string]interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "delete_timer")
	defer span.Send()
	if !query.SupportsAction(ctx, "set_alarm") {
		return Error{Error: "You need to update the app on your watch to delete timers."}
	}
	input := args.(*DeleteTimerInput)
	log.Printf("Asking watch to delete a timer set for %s...\n", input.Time)
	requests <- map[string]interface{}{
		"time":    input.Time,
		"isTimer": true,
		"action":  "set_alarm",
		"cancel":  true,
	}
	log.Println("Waiting for confirmation...")
	resp := <-responses
	return resp
}

func deleteAlarmThought(i interface{}) string {
	return "Deleting an alarm"
}

func deleteTimerThought(i interface{}) string {
	return "Deleting a timer"
}

func getAlarmImpl(ctx context.Context, quotaTracker *quota.Tracker, args interface{}, requests chan<- map[string]interface{}, responses <-chan map[string]interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "get_alarm")
	defer span.Send()
	if !query.SupportsAction(ctx, "get_alarm") {
		return Error{Error: "You need to update the app on your watch to get alarms."}
	}
	log.Println("Asking watch to get alarms...")
	requests <- map[string]interface{}{
		"isTimer": false,
		"action":  "get_alarm",
	}
	log.Println("Waiting for response...")
	resp := <-responses
	log.Println("Got response:", resp)
	return resp
}

func getTimerImpl(ctx context.Context, quotaTracker *quota.Tracker, args interface{}, requests chan<- map[string]interface{}, responses <-chan map[string]interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "get_timer")
	defer span.Send()
	if !query.SupportsAction(ctx, "get_alarm") {
		return Error{Error: "You need to update the app on your watch to get timers."}
	}
	log.Println("Asking watch to get alarms...")
	requests <- map[string]interface{}{
		"isTimer": true,
		"action":  "get_alarm",
	}
	log.Println("Waiting for response...")
	resp := <-responses
	log.Println("Got response:", resp)
	return resp
}

func getAlarmThought(i interface{}) string {
	return "Checking your alarms"
}

func getTimerThought(i interface{}) string {
	return "Checking your timers"
}
