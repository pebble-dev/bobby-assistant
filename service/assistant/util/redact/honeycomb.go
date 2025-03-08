package redact

import (
	"golang.org/x/exp/slices"
	"net/url"
	"regexp"
)

var sensitiveQueryParams = []string{
	"access_token", // our mapbox API key
	"prompt",       // the user's prompt
	"threadId",     // the thread the prompt belongs to, if any
	"lon", "lat",   // user's location as sent to us
	"geocode",   // the user's location as sent to the weather API
	"apiKey",    // the API key for the weather service
	"proximity", // the target location for a POI lookup
	"tzOffset",  // user's timezone offset as sent to us
	"token",     // user's auth (timeline) token, identifies them uniquely.
}
var mapboxPathRegex = regexp.MustCompile(`^/geocoding/v5/mapbox\.places/.+?.json$`)

func redactQuery(query string) string {
	values, err := url.ParseQuery(query)
	if err != nil {
		return "[parse error redacted for safety]"
	}
	newValues := url.Values{}
	for k, v := range values {
		if slices.Contains(sensitiveQueryParams, k) {
			newValues[k] = []string{"redacted"}
		} else {
			newValues[k] = v
		}
	}
	return newValues.Encode()
}

func cleanPath(path string) string {
	if mapboxPathRegex.MatchString(path) {
		return "/geocoding/v5/mapbox.places/[place].json"
	}
	return path
}

func cleanUrl(u string) string {
	parsedUrl, err := url.Parse(u)
	if err != nil {
		return "[parse error redacted for safety]"
	}
	parsedUrl.Path = cleanPath(parsedUrl.Path)
	parsedUrl.RawQuery = redactQuery(parsedUrl.RawQuery)
	return parsedUrl.String()
}

func CleanHoneycomb(data map[string]interface{}) {
	// HTTP requests contain a bunch of PII. We don't want to send that to Honeycomb.
	if query, ok := data["request.query"]; ok {
		if queryStr, ok := query.(string); ok {
			data["request.query"] = redactQuery(queryStr)
		}
	}
	if path, ok := data["request.path"]; ok {
		if pathStr, ok := path.(string); ok {
			data["request.path"] = cleanPath(pathStr)
		}
	}
	if u, ok := data["request.url"]; ok {
		if urlStr, ok := u.(string); ok {
			data["request.url"] = cleanUrl(urlStr)
		}
	}
}
