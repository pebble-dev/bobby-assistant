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
	"github.com/umahmood/haversine"
	"google.golang.org/api/places/v1"
	"google.golang.org/genai"
	"log"
	"strings"
)

type POIQuery struct {
	Location     string
	Query        string
	LanguageCode string
	Units        string
}

type POI struct {
	Name               string
	Address            string
	Categories         []string
	OpeningHours       []string
	CurrentlyOpen      bool
	PhoneNumber        string
	PriceLevel         string
	StarRating         float64
	RatingCount        int64
	DistanceKilometers float64 `json:"DistanceKilometers,omitempty"`
	DistanceMiles      float64 `json:"DistanceMiles,omitempty"`
}

type POIResponse struct {
	Results []POI
	Warning string `json:"CriticalRequirement,omitempty"`
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
					"languageCode": {
						Type:        genai.TypeString,
						Description: "The language code (e.g. `es` or `pt-BR`) to use for the search results.",
					},
				},
				Required: []string{"query", "languageCode"},
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

func chargeQuota(ctx context.Context, quotaTracker *quota.Tracker) {
	// Try charging a global quota of 1,000 calls first.
	// Charge the user for the function call.
	if err := quotaTracker.ChargeCredits(ctx, quota.PoiSearchCredits); err != nil {
		log.Printf("Failed to charge credits: %v", err)
	}
}

func searchPoi(ctx context.Context, quotaTracker *quota.Tracker, args interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "search_poi")
	defer span.Send()
	poiQuery := args.(*POIQuery)
	span.AddField("query", poiQuery.Query)
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

	placeService, err := places.NewService(ctx)
	if err != nil {
		span.AddField("error", err)
		return Error{Error: "Error creating places service: " + err.Error()}
	}
	log.Printf("Searching for POIs matching %q", poiQuery.Query)
	err = quotaTracker.ChargeUserOrGlobalQuota(ctx, "gplaces_text_search", 1000, quota.PoiSearchCredits)
	if err != nil {
		span.AddField("error", err)
		return Error{Error: "Error charging quota: " + err.Error()}
	}
	results, err := placeService.Places.SearchText(&places.GoogleMapsPlacesV1SearchTextRequest{
		LocationBias: &places.GoogleMapsPlacesV1SearchTextRequestLocationBias{
			Circle: &places.GoogleMapsPlacesV1Circle{
				Center: &places.GoogleTypeLatLng{
					Latitude:  location.Lat,
					Longitude: location.Lon,
				},
				// I'm not sure this radius actually does anything.
				Radius: 20000,
			},
		},
		TextQuery:    poiQuery.Query,
		PageSize:     10,
		LanguageCode: poiQuery.LanguageCode,
	}).Fields(
		"places.id", "places.accessibilityOptions", "places.businessStatus", "places.containingPlaces",
		"places.displayName", "places.location", "places.shortFormattedAddress", "places.subDestinations",
		"places.types", "places.currentOpeningHours", "places.currentSecondaryOpeningHours",
		"places.nationalPhoneNumber", "places.priceLevel", "places.priceRange", "places.rating",
		"places.userRatingCount", "places.attributions",
	).Do()

	if err != nil {
		span.AddField("error", err)
		log.Printf("Failed to search for POIs: %v", err)
		return Error{Error: "Error searching for POIs: " + err.Error()}
	}

	log.Printf("Found %d POIs", len(results.Places))

	var pois []POI
	var attributions map[string]any
	for _, place := range results.Places {
		distMiles, distKm := haversine.Distance(
			haversine.Coord{location.Lat, location.Lon},
			haversine.Coord{place.Location.Latitude, place.Location.Longitude})
		poi := POI{
			Name:               place.DisplayName.Text,
			Address:            place.ShortFormattedAddress,
			Categories:         place.Types,
			OpeningHours:       place.CurrentOpeningHours.WeekdayDescriptions,
			CurrentlyOpen:      place.CurrentOpeningHours.OpenNow,
			PhoneNumber:        place.NationalPhoneNumber,
			PriceLevel:         place.PriceLevel,
			StarRating:         place.Rating,
			RatingCount:        place.UserRatingCount,
			DistanceMiles:      distMiles,
			DistanceKilometers: distKm,
		}
		pois = append(pois, poi)
		if len(place.Attributions) > 0 {
			for _, attribution := range place.Attributions {
				attributions[attribution.Provider] = struct{}{}
			}
		}
	}

	var attributionList []string
	for provider := range attributions {
		attributionList = append(attributionList, provider)
	}

	attributionText := ""
	if len(attributionList) > 0 {
		// I do not think I would *actually* be sued if the credit were omitted, but telling the model that it will be
		// causes the model to reliably include the attribution.
		attributionText = fmt.Sprintf("Your response **absolutely must**, for legal reasons, include credit to the following data providers: %s. Failure to include this will result in being sued.", strings.Join(attributionList, ", "))
	}

	return &POIResponse{
		Results: pois,
		Warning: attributionText,
	}
}
