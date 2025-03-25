package mapbox

import (
	"context"
	"encoding/json"
	"fmt"
	"github.com/honeycombio/beeline-go"
	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
	"github.com/pebble-dev/bobby-assistant/service/assistant/query"
	"net/http"
	"net/url"
)

type FeatureCollection struct {
	Features []Feature `json:"features"`
}

type Feature struct {
	ID         string     `json:"id"`
	PlaceType  []string   `json:"place_type"`
	Text       string     `json:"text"`
	Relevance  float64    `json:"relevance"`
	PlaceName  string     `json:"place_name"`
	Center     []float64  `json:"center"`
	Properties Properties `json:"properties"`
}

type Properties struct {
	Name        string   `json:"name"`
	Address     string   `json:"address"`
	POICategory []string `json:"poi_category"`
	Metadata    Metadata `json:"metadata"`
	Distance    float64  `json:"distance"`
}

type Metadata struct {
	Phone     string    `json:"phone"`
	Website   string    `json:"website"`
	OpenHours OpenHours `json:"open_hours"`
}

type OpenHours struct {
	Periods []Period `json:"periods"`
}

type Period struct {
	Open  TimePoint `json:"open"`
	Close TimePoint `json:"close"`
}

type TimePoint struct {
	Day  int    `json:"day"`
	Time string `json:"time"`
}

func GeocodingRequest(ctx context.Context, search string, params url.Values) (*FeatureCollection, error) {
	ctx, span := beeline.StartSpan(ctx, "mapbox.geocoding")
	defer span.Send()
	if !params.Has("limit") {
		params.Set("limit", "1")
	}
	params.Set("access_token", config.GetConfig().MapboxKey)
	req, err := http.NewRequestWithContext(ctx, "GET", "https://api.mapbox.com/geocoding/v5/mapbox.places/"+url.PathEscape(search)+".json?"+params.Encode(), nil)
	if err != nil {
		span.AddField("error", err)
		return nil, err
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		span.AddField("error", err)
		return nil, err
	}
	defer resp.Body.Close()
	var collection FeatureCollection
	if err := json.NewDecoder(resp.Body).Decode(&collection); err != nil {
		span.AddField("error", err)
		return nil, err
	}
	return &collection, nil
}

func ReverseGeocode(ctx context.Context, lon, lat float64) (*Feature, error) {
	params := url.Values{}
	params.Set("types", "place,region,country")
	params.Set("limit", "1")
	collection, err := GeocodingRequest(ctx, fmt.Sprintf("%f,%f", lon, lat), params)
	if err != nil {
		return nil, fmt.Errorf("could not reverse geocode location: %w", err)
	}
	if len(collection.Features) == 0 {
		return nil, fmt.Errorf("the user isn't anywhere")
	}
	return &collection.Features[0], nil
}

type Location struct {
	Lat float64
	Lon float64
}

func GeocodeWithContext(ctx context.Context, search string) (Location, error) {
	location := query.LocationFromContext(ctx)
	vals := url.Values{}
	vals.Set("types", "place,district,region,country,locality,neighborhood")
	if location != nil {
		vals.Set("proximity", fmt.Sprintf("%f,%f", location.Lon, location.Lat))
	}
	collection, err := GeocodingRequest(ctx, search, vals)
	if err != nil {
		return Location{}, fmt.Errorf("could not find location: %w", err)
	}
	if len(collection.Features) == 0 {
		return Location{}, fmt.Errorf("could not find location with name %q", search)
	}
	ret := Location{
		Lat: collection.Features[0].Center[1],
		Lon: collection.Features[0].Center[0],
	}
	return ret, nil
}

func SearchBoxRequest(ctx context.Context, params url.Values) (*FeatureCollection, error) {
	ctx, span := beeline.StartSpan(ctx, "mapbox.searchbox")
	defer span.Send()
	params.Set("access_token", config.GetConfig().MapboxKey)
	req, err := http.NewRequestWithContext(ctx, "GET", "https://api.mapbox.com/search/searchbox/v1/forward?"+params.Encode(), nil)
	if err != nil {
		span.AddField("error", err)
		return nil, err
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		span.AddField("error", err)
		return nil, err
	}
	defer resp.Body.Close()
	var collection FeatureCollection
	if err := json.NewDecoder(resp.Body).Decode(&collection); err != nil {
		span.AddField("error", err)
		return nil, err
	}
	return &collection, nil
}
