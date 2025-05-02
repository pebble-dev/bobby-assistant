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
	"strings"
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
	t := true
	f := false
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "get_weather",
			Description: "Given a location, return the current or future weather, and sunrise/sunset times. Do not specify a location if you want the user's local weather.",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: &f,
				Properties: map[string]*genai.Schema{
					"location": {
						Type:        genai.TypeString,
						Description: "The city, state, and country, e.g. 'Redwood City, CA, USA'. Omit for the user's current location.",
						Nullable:    &t,
					},
					"unit": {
						Type:        genai.TypeString,
						Description: "The user's unit preference",
						Nullable:    &f,
						Enum:        []string{"imperial", "metric", "uk hybrid"},
					},
					"kind": {
						Type:        genai.TypeString,
						Description: "The kind of weather to return: current weather, the next 7 days, or the next 24 hours.",
						Nullable:    &f,
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

func weatherThought(i any) string {
	args := i.(*WeatherInput)
	weatherType := "weather"
	switch args.Kind {
	case "forecast daily":
		weatherType = "daily forecast"
	case "forecast hourly":
		weatherType = "hourly forecast"
	case "current":
		weatherType = "weather"
	}
	if args.Location == "" || args.Location == "here" {
		return fmt.Sprintf("Checking the %s nearby...", weatherType)
	}
	placeName, _, _ := strings.Cut(args.Location, ",")
	return fmt.Sprintf("Checking the %s in %s...", weatherType, placeName)
}

func getWeather(ctx context.Context, quotaTracker *quota.Tracker, args any) any {
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

func processDailyForecast(ctx context.Context, lat, lon float64, units string) any {
	forecast, err := weather.GetDailyForecast(ctx, lat, lon, units)
	if err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not get forecast: " + err.Error()}
	}
	response := map[string]any{}
	for i, day := range forecast.DayOfWeek {
		if i == 0 {
			day += " (Today)"
		}
		dayParts := forecast.DayParts[0]
		uvIndex := dayParts.UvIndex[i*2]
		if uvIndex == nil {
			uvIndex = dayParts.UvIndex[i*2+1]
		}
		windSpeed := dayParts.WindSpeed[i*2]
		if windSpeed == nil {
			windSpeed = dayParts.WindSpeed[i*2+1]
		}
		windDirection := dayParts.WindDirectionCardinal[i*2]
		if windDirection == nil {
			windDirection = dayParts.WindDirectionCardinal[i*2+1]
		}
		response[day] = map[string]any{
			"high":      forecast.CalendarDayTemperatureMax[i],
			"low":       forecast.CalendarDayTemperatureMin[i],
			"narrative": forecast.Narrative[i],
			"sunrise":   forecast.SunriseTimeLocal[i],
			"sunset":    forecast.SunsetTimeLocal[i],
			//"moonrise":   forecast.MoonriseTimeLocal[i],
			//"moonset":    forecast.MoonsetTimeLocal[i],
			//"moon_phase": forecast.MoonPhase[i],
			"qpf":                     forecast.Qpf[i],
			"qpf_snow":                forecast.QpfSnow[i],
			"uv_index":                uvIndex,
			"wind_speed":              windSpeed,
			"wind_direction_cardinal": windDirection,
		}
	}
	response["temperatureUnit"] = forecast.TemperatureUnit
	response["windSpeedUnit"] = forecast.WindSpeedUnit
	response["attribution"] = "The Weather Channel"
	return response
}

func processHourlyForecast(ctx context.Context, lat, lon float64, units string) any {
	hourly, err := weather.GetHourlyForecast(ctx, lat, lon, units)
	if err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not get forecast: " + err.Error()}
	}
	var response []map[string]any
	for i, t := range hourly.ValidTimeLocal {
		if i >= 24 {
			break
		}

		entry := map[string]any{
			"time":                    t[11:16],
			"temperature":             hourly.Temperature[i],
			"uv_index":                hourly.UVIndex[i],
			"description":             hourly.WxPhraseLong[i],
			"wind_speed":              hourly.WindSpeed[i],
			"wind_direction_cardinal": hourly.WindDirectionCardinal[i],
		}
		if hourly.PrecipChance[i] > 20 {
			entry["precip_chance"] = fmt.Sprintf("%d%%", hourly.PrecipChance[i])
			entry["precip_type"] = hourly.PrecipType[i]
		}

		response = append(response, entry)
	}
	// the thing that is returned must not be an array.
	return map[string]any{
		"response":        response,
		"temperatureUnit": hourly.TemperatureUnit,
		"attribution":     "The Weather Channel",
	}
}

func processCurrentWeather(ctx context.Context, lat, lon float64, units string) any {
	observations, err := weather.GetCurrentConditions(ctx, lat, lon, units)
	if err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not get current conditions: " + err.Error()}
	}
	return *observations
}
