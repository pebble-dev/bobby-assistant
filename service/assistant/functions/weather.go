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
	"fmt"
	"github.com/honeycombio/beeline-go"
	"github.com/pebble-dev/bobby-assistant/service/assistant/query"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/mapbox"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/weather"
	"google.golang.org/genai"
)

type WeatherInput struct {
	// The city, state, and country, e.g. 'Redwood City, CA, USA'. Omit for the user's current location.
	Location string `json:"location"`
	// The user's unit preference
	Unit string `json:"unit" jsonschema:"enum=imperial,enum=metric,enum=uk hybrid"`
	// The kind of weather to return: current weather, the next 7 days, or the next 24 hours.
	Kind string `json:"kind" jsonschema:"enum=current,enum=forecast daily,enum=forecast hourly"`
}

func init() {
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "get_weather",
			Description: "Given a location, return the current or future weather, and sunrise/sunset times. Do not specify a location if you want the user's local weather.",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: false,
				Properties: map[string]*genai.Schema{
					"location": {
						Type:        genai.TypeString,
						Description: "The city, state, and country, e.g. 'Redwood City, CA, USA'. Omit for the user's current location.",
						Nullable:    true,
					},
					"unit": {
						Type:        genai.TypeString,
						Description: "The user's unit preference",
						Nullable:    false,
						Enum:        []string{"imperial", "metric", "uk hybrid"},
					},
					"kind": {
						Type:        genai.TypeString,
						Description: "The kind of weather to return: current weather, the next 7 days, or the next 24 hours.",
						Nullable:    false,
						Enum:        []string{"current", "forecast daily", "forecast hourly"},
					},
				},
				Required: []string{"unit", "kind"},
			},
		},
		Fn:        getWeather,
		Thought:   weatherThought,
		InputType: WeatherInput{},
	})
}

func weatherThought(args interface{}) string {
	return "Checking the weather..."
}

func getWeather(ctx context.Context, quotaTracker *quota.Tracker, args interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "get_weather")
	defer span.Send()
	arg := args.(*WeatherInput)
	var lat, lon float64
	location := query.LocationFromContext(ctx)
	if arg.Location == "here" {
		arg.Location = ""
	}
	if arg.Location != "" {
		coords, err := mapbox.GeocodeWithContext(ctx, arg.Location)
		if err != nil {
			span.AddField("error", err)
			return Error{"Error finding location: " + err.Error()}
		}
		lat = coords.Lat
		lon = coords.Lon
	} else {
		if location == nil {
			span.AddField("error", "no location provided")
			return Error{"Could not find your location"}
		}
		lat, lon = location.Lat, location.Lon
	}

	_ = quotaTracker.ChargeCredits(ctx, quota.WeatherQueryCredits)
	switch arg.Kind {
	case "current":
		return processCurrentWeather(ctx, lat, lon, arg.Unit)
	case "forecast daily":
		return processDailyForecast(ctx, lat, lon, arg.Unit)
	case "forecast hourly":
		return processHourlyForecast(ctx, lat, lon, arg.Unit)
	}
	return Error{"invalid kind"}
}

func processDailyForecast(ctx context.Context, lat, lon float64, units string) interface{} {
	forecast, err := weather.GetDailyForecast(ctx, lat, lon, units)
	if err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not get forecast: " + err.Error()}
	}
	response := map[string]interface{}{}
	for i, day := range forecast.DayOfWeek {
		if i == 0 {
			day += " (Today)"
		}
		response[day] = map[string]interface{}{
			"high":      forecast.CalendarDayTemperatureMax[i],
			"low":       forecast.CalendarDayTemperatureMin[i],
			"narrative": forecast.Narrative[i],
			"sunrise":   forecast.SunriseTimeLocal[i],
			"sunset":    forecast.SunsetTimeLocal[i],
			//"moonrise":   forecast.MoonriseTimeLocal[i],
			//"moonset":    forecast.MoonsetTimeLocal[i],
			//"moon_phase": forecast.MoonPhase[i],
			"qpf":      forecast.Qpf[i],
			"qpf_snow": forecast.QpfSnow[i],
		}
	}
	//response["source"] = "The Weather Channel"
	return response
}

func processHourlyForecast(ctx context.Context, lat, lon float64, units string) interface{} {
	hourly, err := weather.GetHourlyForecast(ctx, lat, lon, units)
	if err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not get forecast: " + err.Error()}
	}
	var response []map[string]interface{}
	for i, t := range hourly.ValidTimeLocal {
		if i >= 24 {
			break
		}

		entry := map[string]interface{}{
			"time":        t[11:16],
			"temperature": hourly.Temperature[i],
			"uv_index":    hourly.UVIndex[i],
			"description": hourly.WxPhraseLong[i],
		}
		if hourly.PrecipChance[i] > 20 {
			entry["precip_chance"] = fmt.Sprintf("%d%%", hourly.PrecipChance[i])
			entry["precip_type"] = hourly.PrecipType[i]
		}

		response = append(response, entry)
	}
	// the thing that is returned must not be an array.
	return map[string]any{"response": response}
}

func processCurrentWeather(ctx context.Context, lat, lon float64, units string) interface{} {
	observations, err := weather.GetCurrentConditions(ctx, lat, lon, units)
	if err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not get current conditions: " + err.Error()}
	}
	return *observations
}
