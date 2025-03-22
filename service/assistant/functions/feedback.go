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
	"github.com/pebble-dev/bobby-assistant/service/assistant/query"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
	"google.golang.org/genai"
	"log"
)

type FeedbackInput struct {
	Feedback      string `json:"feedback"`
	IncludeThread bool   `json:"include_thread"`
}

func init() {
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "send_feedback",
			Description: "Send feedback to the developers. Include the thread if you want to provide context for the feedback. Only call this if the user specifically asks to send feedback. Feedback text is optional but recommended if include_thread is true.",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: false,
				Properties: map[string]*genai.Schema{
					"feedback": {
						Type:        genai.TypeString,
						Description: "The feedback to send to the developers.",
						Nullable:    false,
					},
					"include_thread": {
						Type:        genai.TypeBoolean,
						Description: "Whether to include the thread as context in the feedback.",
						Nullable:    false,
					},
				},
				Required: []string{"include_thread"},
			},
		},
		Cb:         sendFeedbackImpl,
		Thought:    sendFeedbackThought,
		InputType:  FeedbackInput{},
		Capability: "send_feedback",
	})
}

func sendFeedbackImpl(ctx context.Context, quotaTracker *quota.Tracker, i interface{}, requestChan chan<- map[string]interface{}, responseChan <-chan map[string]interface{}) interface{} {
	args := i.(*FeedbackInput)
	if !args.IncludeThread && args.Feedback == "" {
		return Error{Error: "You need either set include_thread = true or include some feedback from the user."}
	}
	log.Printf("Asking phone to send feedback...")
	request := map[string]interface{}{
		"action":   "send_feedback",
		"feedback": args.Feedback,
	}
	if args.IncludeThread {
		request["thread_id"] = query.ThreadIdFromContext(ctx)
	}
	requestChan <- request
	log.Printf("Waiting for response...")
	response := <-responseChan
	log.Printf("Got response: %v", response)
	return response
}

func sendFeedbackThought(i interface{}) string {
	args := i.(*FeedbackInput)
	if args.IncludeThread && args.Feedback != "" {
		return "Sending conversation with feedback..."
	} else if args.IncludeThread {
		return "Sending conversation..."
	} else if args.Feedback != "" {
		return "Sending feedback..."
	} else {
		return "Doing nothing productive..."
	}
}
