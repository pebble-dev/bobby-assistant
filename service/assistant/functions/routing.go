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
	"cloud.google.com/go/maps/routing/apiv2"
	"cloud.google.com/go/maps/routing/apiv2/routingpb"
	"context"
	"encoding/json"
	"fmt"
	"github.com/honeycombio/beeline-go"
	"github.com/pebble-dev/bobby-assistant/service/assistant/query"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
	"google.golang.org/api/option"
	"google.golang.org/api/places/v1"
	"google.golang.org/genai"
	"google.golang.org/genproto/googleapis/type/latlng"
	"google.golang.org/grpc/metadata"
	"google.golang.org/protobuf/types/known/timestamppb"
	"log"
	"time"
)

type RoutingQuery struct {
	Origin        string `json:"origin"`
	Destination   string `json:"destination"`
	DepartureTime string `json:"departureTime"`
	ArrivalTime   string `json:"arrivalTime"`
	TravelMode    string `json:"travelMode"`
	LanguageCode  string `json:"languageCode"`
}

var routingClient *routing.RoutesClient

func init() {
	var err error
	routingClient, err = routing.NewRoutesClient(context.Background(), option.WithScopes("https://www.googleapis.com/auth/cloud-platform"))
	if err != nil {
		panic(err)
	}
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name: "find_route",
			Description: "Find a travel route between two locations, by car, bicycle, foot, or transit. " +
				"When using the result of this method, consider adding a ROUTE-MAP widget to show the route on a map. " +
				"If the user doesn't specify a mode of transport, assume driving directions. " +
				"If the user doesn't specify a starting point, assume 'here'. Because of the destination lookup, " +
				"*ALWAYS* mention the realOrigin and realDestination in your response if they are provided - " +
				"failure to do so may mislead the user.",
			Parameters: &genai.Schema{
				Type: genai.TypeObject,
				Properties: map[string]*genai.Schema{
					"destination": {
						Type:        genai.TypeString,
						Description: "The routing destination. A place name is sufficient, it doesn't need to be an address. If you provide 'here', uses the user's current location is used. The origin and destination cannot be the same.",
					},
					"origin": {
						Type:        genai.TypeString,
						Description: "Optional. The routing origin. A place name is sufficient, it doesn't have to be an address. If you provide 'here', uses the user's current location is used. You should always assume the origin is 'here' unless the users says otherwise - you MUST NOT ask them.",
					},
					"departureTime": {
						Type:        genai.TypeString,
						Description: "The time to depart. If omitted, uses the current time. Mutually exclusive with arrivalTime. Use ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'",
					},
					"arrivalTime": {
						Type:        genai.TypeString,
						Description: "The time to arrive. Mutually exclusive with departureTime. Use ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'",
					},
					"travelMode": {
						Type:        genai.TypeString,
						Description: "The mode of transport to use. If omitted, uses the default mode.",
						Enum:        []string{"driving", "bicycle", "walking", "transit"},
					},
					"languageCode": {
						Type:        genai.TypeString,
						Description: "The language code (e.g. `es` or `pt-BR`) to use for the search results.",
					},
				},
				Required: []string{"destination", "travelMode", "languageCode"},
			},
		},
		Fn:        findRoute,
		Thought:   findRouteThought,
		InputType: RoutingQuery{},
	})
}

func findRouteThought(args any) string {
	arg := args.(*RoutingQuery)
	return "Finding route to " + arg.Destination
}

func resolveLocation(ctx context.Context, location string) (*routingpb.Waypoint, error) {
	ctx, span := beeline.StartSpan(ctx, "resolve_location")
	defer span.Send()
	// The routing API only accepts things that actually look like addresses, so we do a text search to resolve to a
	// place ID first. Conveniently, Google doesn't actually charge for this, as long as we only ask for place IDs.
	placeService, err := places.NewService(ctx)
	if err != nil {
		span.AddField("error", err)
		return nil, err
	}
	userLocation := query.LocationFromContext(ctx)
	var locationBias *places.GoogleMapsPlacesV1SearchTextRequestLocationBias
	if userLocation != nil {
		locationBias = &places.GoogleMapsPlacesV1SearchTextRequestLocationBias{
			Circle: &places.GoogleMapsPlacesV1Circle{
				Center: &places.GoogleTypeLatLng{
					Latitude:  userLocation.Lat,
					Longitude: userLocation.Lon,
				},
				// I'm not sure this radius actually does anything.
				Radius: 20000,
			},
		}
	}
	results, err := placeService.Places.SearchText(&places.GoogleMapsPlacesV1SearchTextRequest{
		LocationBias: locationBias,
		TextQuery:    location,
		PageSize:     1,
	}).Fields("places.id").Do()
	if err != nil {
		span.AddField("error", err)
		return nil, err
	}
	if len(results.Places) == 0 {
		span.AddField("error", "no results found")
		return nil, fmt.Errorf("no results found for %q", location)
	}
	return &routingpb.Waypoint{
		LocationType: &routingpb.Waypoint_PlaceId{
			PlaceId: results.Places[0].Id,
		},
	}, nil
}

func placeIdToName(ctx context.Context, qt *quota.Tracker, placeId string) (string, string, error) {
	ctx, span := beeline.StartSpan(ctx, "place_id_to_name")
	defer span.Send()
	placeService, err := places.NewService(ctx)
	if err != nil {
		span.AddField("error", err)
		return "", "", err
	}
	// somehow, knowing the name of a place is "pro", not "essential"
	if err := qt.ChargeUserOrGlobalQuota(ctx, "gplaces_details_pro", 5000, quota.GPlacesLookupProCredits); err != nil {
		span.AddField("error", err)
		return "", "", err
	}
	result, err := placeService.Places.Get("places/" + placeId).Fields("displayName,primaryTypeDisplayName").Do()
	if err != nil {
		span.AddField("error", err)
		return "", "", err
	}
	if result.DisplayName.Text == "" {
		return "", "", fmt.Errorf("no name found for place ID %q", placeId)
	}
	return result.DisplayName.Text, result.PrimaryTypeDisplayName.Text, nil
}

func waypointFromString(ctx context.Context, location string) (*routingpb.Waypoint, error) {
	if location == "" || location == "here" {
		loc := query.LocationFromContext(ctx)
		if loc == nil {
			return nil, fmt.Errorf("no location available")
		}
		return &routingpb.Waypoint{
			LocationType: &routingpb.Waypoint_Location{
				Location: &routingpb.Location{
					LatLng: &latlng.LatLng{
						Latitude:  loc.Lat,
						Longitude: loc.Lon,
					},
				},
			},
		}, nil
	}
	return resolveLocation(ctx, location)
}

func travelModeFromString(mode string) (routingpb.RouteTravelMode, error) {
	switch mode {
	case "driving":
		return routingpb.RouteTravelMode_DRIVE, nil
	case "bicycle":
		return routingpb.RouteTravelMode_BICYCLE, nil
	case "walking":
		return routingpb.RouteTravelMode_WALK, nil
	case "transit":
		return routingpb.RouteTravelMode_TRANSIT, nil
	default:
		return routingpb.RouteTravelMode_TRAVEL_MODE_UNSPECIFIED, fmt.Errorf("unknown travel mode: %s", mode)
	}
}

func nullableTimestrampFromString(timeStr string) (*timestamppb.Timestamp, error) {
	if timeStr == "" {
		return nil, nil
	}
	t, err := time.Parse(time.RFC3339, timeStr)
	if err != nil {
		return nil, fmt.Errorf("error parsing timestamp: %w", err)
	}
	return timestamppb.New(t), nil
}

func findRoute(ctx context.Context, quotaTracker *quota.Tracker, args any) any {
	ctx, span := beeline.StartSpan(ctx, "find_route")
	defer span.Send()
	arg := args.(*RoutingQuery)

	origin, err := waypointFromString(ctx, arg.Origin)
	if err != nil {
		return Error{Error: "Error parsing origin: " + err.Error()}
	}
	destination, err := waypointFromString(ctx, arg.Destination)
	if err != nil {
		return Error{Error: "Error parsing destination: " + err.Error()}
	}

	travelMode, err := travelModeFromString(arg.TravelMode)
	if err != nil {
		return Error{Error: "Error parsing travel mode: " + err.Error()}
	}

	departureTime, err := nullableTimestrampFromString(arg.DepartureTime)
	if err != nil {
		return Error{Error: "Error parsing departure time: " + err.Error()}
	}
	arrivalTime, err := nullableTimestrampFromString(arg.ArrivalTime)
	if err != nil {
		return Error{Error: "Error parsing arrival time: " + err.Error()}
	}
	if departureTime != nil && arrivalTime != nil {
		return Error{Error: "departureTime and arrivalTime are mutually exclusive"}
	}

	err = quotaTracker.ChargeUserOrGlobalQuota(ctx, "gmaps_route", 5000, quota.PoiSearchCredits)
	if err != nil {
		span.AddField("error", err)
		return Error{Error: "Error charging quota: " + err.Error()}
	}

	crr := routingpb.ComputeRoutesRequest{
		Origin:           origin,
		Destination:      destination,
		TravelMode:       travelMode,
		PolylineQuality:  routingpb.PolylineQuality_OVERVIEW,
		PolylineEncoding: routingpb.PolylineEncoding_ENCODED_POLYLINE,
		DepartureTime:    departureTime,
		ArrivalTime:      arrivalTime,
		LanguageCode:     arg.LanguageCode,
	}

	if crr.TravelMode == routingpb.RouteTravelMode_DRIVE {
		crr.RoutingPreference = routingpb.RoutingPreference_TRAFFIC_AWARE_OPTIMAL
		crr.TrafficModel = routingpb.TrafficModel_BEST_GUESS
	}

	ctx = metadata.AppendToOutgoingContext(ctx,
		"x-goog-fieldmask",
		"routes.distanceMeters,routes.duration,routes.polyline,routes.description,routes.warnings,routes.travelAdvisory,"+
			"routes.localizedValues.distance,routes.localizedValues.duration,routes.legs.steps.distanceMeters,"+
			"routes.legs.steps.staticDuration,routes.legs.steps.navigationInstruction,routes.legs.steps.localizedValues,"+
			"routes.legs.steps.transitDetails,routes.legs.steps.travelMode,"+
			"geocodingResults.origin.placeId,geocodingResults.destination.placeId")

	response, err := routingClient.ComputeRoutes(ctx, &crr)

	if err != nil {
		log.Printf("Error finding route: %v", err)
		return Error{Error: "Error finding route: " + err.Error()}
	}

	if len(response.Routes) == 0 {
		return Error{Error: "No routes found"}
	}
	route := response.Routes[0]
	resp := map[string]any{
		"route": routeToLegible(route),
	}
	if originPlace, ok := origin.LocationType.(*routingpb.Waypoint_PlaceId); ok {
		placeName, placeType, err := placeIdToName(ctx, quotaTracker, originPlace.PlaceId)
		if err == nil {
			resp["realOrigin"] = map[string]string{
				"name": placeName,
				"type": placeType,
			}
		} else {
			log.Printf("Error getting place name for origin: %v", err)
		}
	}
	if destinationPlace, ok := destination.LocationType.(*routingpb.Waypoint_PlaceId); ok {
		placeName, placeType, err := placeIdToName(ctx, quotaTracker, destinationPlace.PlaceId)
		if err == nil {
			resp["realDestination"] = map[string]string{
				"name": placeName,
				"type": placeType,
			}
		} else {
			log.Printf("Error getting place name for destination: %v", err)
		}
	}

	threadContext := query.ThreadContextFromContext(ctx)
	threadContext.ContextStorage.LastRoute = resp

	return resp
}

func routeToLegible(route *routingpb.Route) map[string]any {
	// first just dump this straight to JSON and then parse it back
	j, _ := json.Marshal(route)
	var m map[string]any
	_ = json.Unmarshal(j, &m)
	// now we need to change numeric constants to string constants, so the LLM has some hope of divining their meaning.
	if m["legs"] != nil {
		legs := m["legs"].([]any)
		for _, leg := range legs {
			legMap := leg.(map[string]any)
			if legMap["steps"] != nil {
				steps := legMap["steps"].([]any)
				for _, step := range steps {
					stepMap := step.(map[string]any)
					if stepMap["travel_mode"] != nil {
						stepMap["travel_mode"] = routingpb.RouteTravelMode_name[int32(stepMap["travel_mode"].(float64))]
					}
					if stepMap["navigation_instruction"] != nil {
						navInsMap := stepMap["navigation_instruction"].(map[string]any)
						if maneuver, ok := navInsMap["maneuver"]; ok {
							navInsMap["maneuver"] = routingpb.Maneuver_name[int32(maneuver.(float64))]
						}
					}
				}
			}
		}
	}
	return m
}
