package functions

import (
	"context"
	"fmt"
	"github.com/pebble-dev/bobby-assistant/service/assistant/llm"
	"strings"
	"time"

	"github.com/honeycombio/beeline-go"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
)

type TimeResponse struct {
	Time string `json:"time"`
}

type GetTimeInput struct {
	// The timezone, e.g. 'America/Los_Angeles'.
	Timezone string `json:"timezone" jsonschema:"required"`
	// The number of seconds to add to the current time.
	Offset float64 `json:"offset"`
}

func init() {
	f := false
	registerFunction(Registration{
		Definition: llm.FunctionDeclaration{
			Name:        "get_time_elsewhere",
			Description: "Get the current time in a given valid tzdb timezone. Not all cities have a tzdb entry - be sure to use one that exists. Call multiple times to find the time in multiple timezones.",
			Parameters: &llm.Schema{
				Type:     llm.TypeObject,
				Nullable: &f,
				Properties: map[string]*llm.Schema{
					"timezone": {
						Type:        llm.TypeString,
						Description: "The timezone, e.g. 'America/Los_Angeles'.",
						Nullable:    &f,
					},
					"offset": {
						Type:        llm.TypeNumber,
						Description: "The number of seconds to add to the current time, if checking a different time. Omit or set to zero for current time.",
						Format:      "double",
					},
				},
				Required: []string{"timezone"},
			},
		},
		Fn:        getTimeElsewhere,
		Thought:   getTimeThought,
		InputType: GetTimeInput{},
	})
}

func getTimeThought(args any) string {
	arg := args.(*GetTimeInput)
	if arg.Timezone != "" {
		s := strings.Split(arg.Timezone, "/")
		place := strings.Replace(s[len(s)-1], "_", " ", -1)
		return "Checking the time in " + place
	}
	return "Checking the time"
}

func getTimeElsewhere(ctx context.Context, quotaTracker *quota.Tracker, args any) any {
	ctx, span := beeline.StartSpan(ctx, "get_time_elsewhere")
	defer span.Send()
	arg := args.(*GetTimeInput)
	utc := time.Now().UTC().Add(time.Duration(arg.Offset) * time.Second)
	loc, err := time.LoadLocation(arg.Timezone)
	if err != nil {
		return Error{fmt.Sprintf("The timezone %q is not valid", arg.Timezone)}
	}
	utc.In(loc)
	return TimeResponse{utc.In(loc).Format(time.RFC1123)}
}
