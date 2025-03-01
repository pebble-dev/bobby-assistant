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
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"net/url"

	"github.com/honeycombio/beeline-go"
	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
	"github.com/pebble-dev/bobby-assistant/service/assistant/query"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/mapbox"
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
	unit, ok := map[string]string{"imperial": "e", "metric": "m", "uk hybrid": "h"}[arg.Unit]
	if !ok {
		span.AddField("error", "bad units: "+arg.Unit)
		return Error{"unit must be one of imperial, metric, or uk hybrid"}
	}
	var lat, lon float64
	location := query.LocationFromContext(ctx)
	if arg.Location != "" {
		vals := url.Values{}
		vals.Set("types", "place,district,region,country,locality,neighborhood")
		if location != nil {
			vals.Set("proximity", fmt.Sprintf("%f,%f", location.Lon, location.Lat))
		}
		collection, err := mapbox.GeocodingRequest(ctx, arg.Location, vals)
		if err != nil {
			span.AddField("error", "error finding location: "+err.Error())
			return Error{"Could not find location: " + err.Error()}
		}
		if len(collection.Features) == 0 {
			span.AddField("error", "no such location: "+err.Error())
			return Error{"Could not find location with name " + arg.Location}
		}
		lat, lon = collection.Features[0].Center[1], collection.Features[0].Center[0]
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
		return processCurrentWeather(ctx, lat, lon, unit)
	case "forecast daily":
		return processDailyForecast(ctx, lat, lon, unit)
	case "forecast hourly":
		return processHourlyForecast(ctx, lat, lon, unit)
	}
	return Error{"invalid kind"}
}

type weirdIbmForecast struct {
	CalendarDayTemperatureMax []int     `json:"calendarDayTemperatureMax"`
	CalendarDayTemperatureMin []int     `json:"calendarDayTemperatureMin"`
	DayOfWeek                 []string  `json:"dayOfWeek"`
	MoonPhaseCode             []string  `json:"moonPhaseCode"`
	MoonPhase                 []string  `json:"moonPhase"`
	MoonPhaseDay              []int     `json:"moonPhaseDay"`
	Narrative                 []string  `json:"narrative"`
	SunriseTimeLocal          []string  `json:"sunriseTimeLocal"`
	SunsetTimeLocal           []string  `json:"sunsetTimeLocal"`
	MoonriseTimeLocal         []string  `json:"moonriseTimeLocal"`
	MoonsetTimeLocal          []string  `json:"moonsetTimeLocal"`
	Qpf                       []float32 `json:"qpf"`
	QpfSnow                   []float32 `json:"qpfSnow"`
}

type weirdIbmCurrent struct {
	CloudCoverPhrase      string  `json:"cloudCoverPhrase"`
	CloudCover            int     `json:"cloudCover"`
	DayOfWeek             string  `json:"dayOfWeek"`
	DayOrNight            string  `json:"dayOrNight"`
	Precip1Hour           float32 `json:"precip1Hour"`
	Precip6Hour           float32 `json:"precip6Hour"`
	Precip12Hour          float32 `json:"precip12Hour"`
	RelativeHumidity      int     `json:"relativeHumidity"`
	SunriseTimeLocal      string  `json:"sunriseTimeLocal"`
	SunsetTimeLocal       string  `json:"sunsetTimeLocal"`
	Temperature           int     `json:"temperature"`
	TemperatureFeelsLike  int     `json:"temperatureFeelsLike"`
	TemperatureMax24Hour  int     `json:"temperatureMax24Hour"`
	TemperatureMin24Hour  int     `json:"temperatureMin24Hour"`
	TemperatureWindChill  int     `json:"temperatureWindChill"`
	UVIndex               int     `json:"uvIndex"`
	Visibility            float32 `json:"visibility"`
	WindDirectionCardinal string  `json:"windDirectionCardinal"`
	WindSpeed             int     `json:"windSpeed"`
	WindGust              int     `json:"windGust"`
	Description           string  `json:"wxPhraseLong"`
	//MustCite              string  `json:"source"`
}

type weirdIbmHourly struct {
	WxPhraseLong   []string `json:"wxPhraseLong"`
	Temperature    []int    `json:"temperature"`
	PrecipChance   []int    `json:"precipChance"`
	PrecipType     []string `json:"precipType"`
	ValidTimeLocal []string `json:"validTimeLocal"`
	UVIndex        []int    `json:"uvIndex"`
}

func processDailyForecast(ctx context.Context, lat, lon float64, units string) interface{} {
	req, err := http.NewRequestWithContext(ctx, "GET", fmt.Sprintf("https://api.weather.com/v3/wx/forecast/daily/7day?geocode=%f,%f&units=%s&language=en_US&format=json&apiKey=795ea3c9f93e42379ea3c9f93e623723", lat, lon, units), nil)
	if err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not create request: " + err.Error()}
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not make request: " + err.Error()}
	}
	defer resp.Body.Close()
	var forecast weirdIbmForecast
	if err := json.NewDecoder(resp.Body).Decode(&forecast); err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not decode response: " + err.Error()}
	}
	log.Println(forecast)
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
	req, err := http.NewRequestWithContext(ctx, "GET", fmt.Sprintf("https://api.weather.com/v3/wx/forecast/hourly/2day?geocode=%f,%f&units=%s&language=en_US&format=json&apiKey=%s", lat, lon, units, config.GetConfig().IBMKey), nil)
	if err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not create request: " + err.Error()}
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not make request: " + err.Error()}
	}
	defer resp.Body.Close()
	var hourly weirdIbmHourly
	if err := json.NewDecoder(resp.Body).Decode(&hourly); err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not decode response: " + err.Error()}
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
	return response
}

func processCurrentWeather(ctx context.Context, lat, lon float64, units string) interface{} {
	req, err := http.NewRequestWithContext(ctx, "GET", fmt.Sprintf("https://api.weather.com/v3/wx/observations/current?geocode=%f,%f&units=%s&language=en_US&format=json&apiKey=%s", lat, lon, units, config.GetConfig().IBMKey), nil)
	if err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not create request: " + err.Error()}
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not make request: " + err.Error()}
	}
	defer resp.Body.Close()
	var observations weirdIbmCurrent
	if err := json.NewDecoder(resp.Body).Decode(&observations); err != nil {
		beeline.AddField(ctx, "error", err)
		return Error{"Could not decode response: " + err.Error()}
	}
	//observations.MustCite = "The Weather Channel"
	return observations
}
