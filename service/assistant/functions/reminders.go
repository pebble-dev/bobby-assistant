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
	"google.golang.org/genai"
	"log"
	"time"

	"github.com/pebble-dev/bobby-assistant/service/assistant/query"
)

type SetReminderInput struct {
	// The time to schedule the reminder for in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'.
	Time string `json:"time"`
	// The delay from now to when the reminder should be scheduled, in minutes.
	Delay int `json:"delay_mins"`
	// What to remind the user to do.
	What string `json:"what" jsonschema:"required"`
}

func init() {
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "set_reminder",
			Description: "Set a reminder for the user to perform a task at a time.  Either time or delay must be provided, but not both. If the user specifies a time but not a day, assume they meant the next time that time will happen.",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: false,
				Properties: map[string]*genai.Schema{
					"time": {
						Type:        genai.TypeString,
						Description: "The time to schedule the reminder for in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'. Always assume the user's timezone unless otherwise specified.",
						Nullable:    true,
					},
					"delay_mins": {
						Type:        genai.TypeInteger,
						Description: "The delay from now to when the reminder should be scheduled, in minutes.",
						Nullable:    true,
						Format:      "int32",
					},
					"what": {
						Type:        genai.TypeString,
						Description: "What to remind the user to do.",
						Nullable:    false,
					},
				},
				Required: []string{"what"},
			},
		},
		Cb:        setReminder,
		Thought:   reminderThought,
		InputType: SetReminderInput{},
	})
}

func setReminder(ctx context.Context, quotaTracker *quota.Tracker, args interface{}, requestChan chan<- map[string]interface{}, responseChan <-chan map[string]interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "set_reminder")
	defer span.Send()
	if !query.SupportsAction(ctx, "set_reminder") {
		return Error{Error: "You need to update the app on your watch to set reminders."}
	}
	arg := args.(*SetReminderInput)
	if arg.Time == "" && arg.Delay == 0 {
		return Error{Error: "Either time or delay must be provided."}
	}
	if arg.Time != "" && arg.Delay != 0 {
		return Error{Error: "Only one of time or delay may be provided."}
	}
	if arg.Delay != 0 {
		arg.Time = time.Now().UTC().Add(time.Duration(arg.Delay) * time.Minute).Format(time.RFC3339)
	}

	req := map[string]interface{}{
		"time":   arg.Time,
		"what":   arg.What,
		"action": "set_reminder",
	}
	// In actuality the only thing a reminder does is create a timeline pin... which we could do from here.
	// We already have a timeline token, after all. Send it to the watch for now anyway.
	log.Println("Asking watch to set reminder...")
	requestChan <- req
	log.Println("Waiting for confirmation...")
	resp := <-responseChan
	return resp
}

func reminderThought(args interface{}) string {
	return "Setting a reminder"
}
