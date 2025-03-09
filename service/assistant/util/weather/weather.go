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

package weather

import (
	"context"
	"encoding/json"
	"fmt"
	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
	"net/http"
)

func mapUnit(unit string) (string, error) {
	switch unit {
	case "imperial":
		return "e", nil
	case "metric":
		return "m", nil
	case "uk hybrid":
		return "h", nil
	}
	return "", fmt.Errorf("unit must be one of 'imperial', 'metric', or 'uk hybrid'; not %q", unit)
}

func GetDailyForecast(ctx context.Context, lat, lon float64, units string) (*Forecast, error) {
	units, err := mapUnit(units)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequestWithContext(ctx, "GET", fmt.Sprintf("https://api.weather.com/v3/wx/forecast/daily/7day?geocode=%f,%f&units=%s&language=en_US&format=json&apiKey=%s", lat, lon, units, config.GetConfig().IBMKey), nil)
	fmt.Println(fmt.Sprintf("https://api.weather.com/v3/wx/forecast/daily/7day?geocode=%f,%f&units=%s&language=en_US&format=json&apiKey=%s", lat, lon, units, config.GetConfig().IBMKey))
	if err != nil {
		return nil, fmt.Errorf("error creating request: %w", err)
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("error making request: %w", err)
	}
	defer resp.Body.Close()
	var forecast Forecast
	if err := json.NewDecoder(resp.Body).Decode(&forecast); err != nil {
		return nil, fmt.Errorf("error decoding response: %w", err)
	}
	return &forecast, nil
}

func GetCurrentConditions(ctx context.Context, lat, lon float64, units string) (*CurrentConditions, error) {
	units, err := mapUnit(units)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequestWithContext(ctx, "GET", fmt.Sprintf("https://api.weather.com/v3/wx/observations/current?geocode=%f,%f&units=%s&language=en_US&format=json&apiKey=%s", lat, lon, units, config.GetConfig().IBMKey), nil)
	if err != nil {
		return nil, fmt.Errorf("error creating request: %w", err)
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("error making request: %w", err)
	}
	defer resp.Body.Close()
	var conditions CurrentConditions
	if err := json.NewDecoder(resp.Body).Decode(&conditions); err != nil {
		return nil, fmt.Errorf("error decoding response: %w", err)
	}
	return &conditions, nil
}

func GetHourlyForecast(ctx context.Context, lat, lon float64, units string) (*HourlyForecast, error) {
	units, err := mapUnit(units)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequestWithContext(ctx, "GET", fmt.Sprintf("https://api.weather.com/v3/wx/forecast/hourly/2day?geocode=%f,%f&units=%s&language=en_US&format=json&apiKey=%s", lat, lon, units, config.GetConfig().IBMKey), nil)
	if err != nil {
		return nil, fmt.Errorf("error creating request: %w", err)
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("error making request: %w", err)
	}
	defer resp.Body.Close()
	var hourly HourlyForecast
	if err := json.NewDecoder(resp.Body).Decode(&hourly); err != nil {
		return nil, fmt.Errorf("error decoding response: %w", err)
	}
	return &hourly, nil
}

type Forecast struct {
	CalendarDayTemperatureMax []int             `json:"calendarDayTemperatureMax"`
	CalendarDayTemperatureMin []int             `json:"calendarDayTemperatureMin"`
	DayOfWeek                 []string          `json:"dayOfWeek"`
	MoonPhaseCode             []string          `json:"moonPhaseCode"`
	MoonPhase                 []string          `json:"moonPhase"`
	MoonPhaseDay              []int             `json:"moonPhaseDay"`
	Narrative                 []string          `json:"narrative"`
	SunriseTimeLocal          []string          `json:"sunriseTimeLocal"`
	SunsetTimeLocal           []string          `json:"sunsetTimeLocal"`
	MoonriseTimeLocal         []string          `json:"moonriseTimeLocal"`
	MoonsetTimeLocal          []string          `json:"moonsetTimeLocal"`
	Qpf                       []float32         `json:"qpf"`
	QpfSnow                   []float32         `json:"qpfSnow"`
	DayParts                  []ForecastDayPart `json:"daypart"`
}

type ForecastDayPart struct {
	CloudCover            []*int     `json:"cloudCover"`
	DayOrNight            []*string  `json:"dayOrNight"`
	DaypartName           []*string  `json:"daypartName"`
	IconCode              []*int     `json:"iconCode"`
	IconCodeExtend        []*int     `json:"iconCodeExtend"`
	Narrative             []*string  `json:"narrative"`
	PrecipChance          []*int     `json:"precipChance"`
	PrecipType            []*string  `json:"precipType"`
	Qpf                   []*float64 `json:"qpf"`
	QpfSnow               []*float64 `json:"qpfSnow"`
	QualifierCode         []*string  `json:"qualifierCode"`
	QualifierPhrase       []*string  `json:"qualifierPhrase"`
	RelativeHumidity      []*int     `json:"relativeHumidity"`
	SnowRange             []*string  `json:"snowRange"`
	Temperature           []*int     `json:"temperature"`
	TemperatureHeatIndex  []*int     `json:"temperatureHeatIndex"`
	TemperatureWindChill  []*int     `json:"temperatureWindChill"`
	ThunderCategory       []*string  `json:"thunderCategory"`
	ThunderIndex          []*int     `json:"thunderIndex"`
	UvDescription         []*string  `json:"uvDescription"`
	UvIndex               []*int     `json:"uvIndex"`
	WindDirection         []*int     `json:"windDirection"`
	WindDirectionCardinal []*string  `json:"windDirectionCardinal"`
	WindPhrase            []*string  `json:"windPhrase"`
	WindSpeed             []*int     `json:"windSpeed"`
	WxPhraseLong          []*string  `json:"wxPhraseLong"`
	WxPhraseShort         []*string  `json:"wxPhraseShort"`
}

type CurrentConditions struct {
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
	IconCode              int     `json:"iconCode"`
}

type HourlyForecast struct {
	WxPhraseLong   []string `json:"wxPhraseLong"`
	Temperature    []int    `json:"temperature"`
	PrecipChance   []int    `json:"precipChance"`
	PrecipType     []string `json:"precipType"`
	ValidTimeLocal []string `json:"validTimeLocal"`
	UVIndex        []int    `json:"uvIndex"`
}
