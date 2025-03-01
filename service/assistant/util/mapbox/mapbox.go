package mapbox

import (
	"context"
	"encoding/json"
	"github.com/honeycombio/beeline-go"
	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
	"net/http"
	"net/url"
)

type FeatureCollection struct {
	Features []Feature `json:"features"`
}

type Feature struct {
	ID        string    `json:"id"`
	PlaceType []string  `json:"place_type"`
	Text      string    `json:"text"`
	Relevance float64   `json:"relevance"`
	PlaceName string    `json:"place_name"`
	Center    []float64 `json:"center"`
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
