package functions

import (
	"context"
	"fmt"
	"github.com/pebble-dev/bobby-assistant/service/assistant/query"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/mapbox"
	"github.com/umahmood/haversine"
	"google.golang.org/genai"

	"github.com/honeycombio/beeline-go"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
)

type LocationResponse struct {
	Latitude           float64 `json:"latitude"`
	Longitude          float64 `json:"longitude"`
	DistanceKilometers float64 `json:"distance_meters,omitempty"`
	DistanceMiles      float64 `json:"distance_miles,omitempty"`
}

type GetLocationInput struct {
	// The name of a place to locate, e.g. "San Francisco, CA, USA" or "Paris, France".
	PlaceName string `json:"place_name"`
}

func init() {
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "get_location",
			Description: "Get the latitude and longitude of a given location. If the user's location is available, also provides the distance from the user.",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: false,
				Properties: map[string]*genai.Schema{
					"place_name": {
						Type:        genai.TypeString,
						Description: `The name of a place to locate, e.g. "San Francisco, CA, USA" or "Paris, France".`,
						Nullable:    false,
					},
				},
				Required: []string{"place_name"},
			},
		},
		Fn:        getLocationImpl,
		Thought:   getLocationThought,
		InputType: GetLocationInput{},
	})
}

func getLocationThought(args interface{}) string {
	arg := args.(*GetLocationInput)
	return fmt.Sprintf("Locating %q", arg.PlaceName)
}

func getLocationImpl(ctx context.Context, quotaTracker *quota.Tracker, args interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "get_location")
	defer span.Send()
	arg := args.(*GetLocationInput)
	location, err := mapbox.GeocodeWithContext(ctx, arg.PlaceName)
	if err != nil {
		return fmt.Errorf("failed to geocode %q: %w", arg.PlaceName, err)
	}
	userLocation := query.LocationFromContext(ctx)
	lr := LocationResponse{
		Latitude:  location.Lat,
		Longitude: location.Lon,
	}
	if userLocation != nil {
		uh := haversine.Coord{Lat: userLocation.Lat, Lon: userLocation.Lon}
		lh := haversine.Coord{Lat: location.Lat, Lon: location.Lon}
		lr.DistanceMiles, lr.DistanceKilometers = haversine.Distance(uh, lh)
	}
	return lr
}
