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
	"google.golang.org/genai"
	"log"
	"net/url"
	"strings"
)

type POIQuery struct {
	Location string
	Query    string
}

type POI struct {
	Name         string
	Address      string
	Categories   []string
	OpeningHours map[string]string
}

type POIResponse struct {
	Results []POI
}

func init() {
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "poi",
			Description: "Look up points of interest near the user's location (or another named location).",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: false,
				Properties: map[string]*genai.Schema{
					"query": {
						Type:        genai.TypeString,
						Description: "The search query to use to find points of interest. Could be a name (e.g. \"McDonald's\"), a category (e.g. \"restaurant\" or \"pizza\"), or another search term.",
						Nullable:    false,
					},
					"location": {
						Type:        genai.TypeString,
						Description: "The name of the location to search near. If not provided, the user's current location will be used. Assume that no location should be provided unless explicitly requested: not providing one results in more accurate answers.",
						Nullable:    true,
					},
				},
				Required: []string{"query"},
			},
		},
		Fn:        searchPoi,
		Thought:   searchPoiThought,
		InputType: POIQuery{},
	})
}

func searchPoiThought(args interface{}) string {
	poiQuery := args.(*POIQuery)
	if poiQuery.Location != "" {
		location, _, _ := strings.Cut(poiQuery.Location, ",")
		return fmt.Sprintf("Looking for %s near %s...", poiQuery.Query, location)
	}
	return fmt.Sprintf("Looking for %s nearby...", poiQuery.Query)
}

func searchPoi(ctx context.Context, quotaTracker *quota.Tracker, args interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "search_poi")
	defer span.Send()
	poiQuery := args.(*POIQuery)
	span.AddField("query", poiQuery.Query)
	qs := url.Values{}
	location := query.LocationFromContext(ctx)
	if poiQuery.Location != "" {
		coords, err := mapbox.GeocodeWithContext(ctx, poiQuery.Location)
		if err != nil {
			span.AddField("error", err)
			return Error{Error: "Error finding location: " + err.Error()}
		}
		location = &query.Location{
			Lon: coords.Lon,
			Lat: coords.Lat,
		}
	}
	qs.Set("q", poiQuery.Query)
	if location != nil {
		qs.Set("proximity", fmt.Sprintf("%f,%f", location.Lon, location.Lat))
	}

	log.Printf("Searching for POIs matching %q", poiQuery.Query)
	_ = quotaTracker.ChargeCredits(ctx, quota.PoiSearchCredits)
	results, err := mapbox.SearchBoxRequest(ctx, qs)
	if err != nil {
		span.AddField("error", err)
		log.Printf("Failed to search for POIs: %v", err)
		return Error{Error: err.Error()}
	}
	log.Printf("Found %d POIs", len(results.Features))

	var pois []POI
	for _, feature := range results.Features {
		poi := POI{
			Name:         feature.Properties.Name,
			Address:      feature.Properties.Address,
			Categories:   feature.Properties.POICategory,
			OpeningHours: make(map[string]string),
		}
		days := []string{"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}
		for _, period := range feature.Properties.Metadata.OpenHours.Periods {
			poi.OpeningHours[days[period.Open.Day]] = fmt.Sprintf("%s - %s", period.Open.Time, period.Close.Time)
		}
		for _, day := range days {
			if _, ok := poi.OpeningHours[day]; !ok {
				poi.OpeningHours[day] = "Closed"
			}
		}
		pois = append(pois, poi)
	}

	return &POIResponse{
		Results: pois,
	}
}
