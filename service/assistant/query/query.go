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

package query

import (
	"context"
	"net/url"
	"strconv"
	"strings"

	"golang.org/x/exp/slices"
)

type Location struct {
	Lat float64
	Lon float64
}

type queryContext struct {
	location          *Location
	tzOffset          int
	supportedActions  []string
	supportedWidgets  []string
	preferredLanguage string
	preferredUnits    string
	threadId          string
}

type qckt int

var queryContextKey qckt

func ContextWith(ctx context.Context, q url.Values) context.Context {
	var location *Location
	if q.Get("lat") != "" && q.Get("lon") != "" {
		lat, letErr := strconv.ParseFloat(q.Get("lat"), 64)
		lon, lonErr := strconv.ParseFloat(q.Get("lon"), 64)
		if letErr == nil && lonErr == nil {
			location = &Location{
				Lat: lat,
				Lon: lon,
			}
		}
	}
	offset, _ := strconv.Atoi(q.Get("tzOffset"))
	supportedActions := strings.Split(q.Get("actions"), ",")
	supportedWidgets := strings.Split(q.Get("widgets"), ",")
	preferredLanguage := q.Get("lang")
	preferredUnits := q.Get("units")
	threadId := q.Get("threadId")
	qc := queryContext{
		location:          location,
		tzOffset:          offset,
		supportedActions:  supportedActions,
		supportedWidgets:  supportedWidgets,
		preferredLanguage: preferredLanguage,
		preferredUnits:    preferredUnits,
		threadId:          threadId,
	}
	ctx = context.WithValue(ctx, queryContextKey, qc)
	return ctx
}

func TzOffsetFromContext(ctx context.Context) int {
	return ctx.Value(queryContextKey).(queryContext).tzOffset
}

func SupportedActionsFromContext(ctx context.Context) []string {
	return ctx.Value(queryContextKey).(queryContext).supportedActions
}

func SupportedWidgetsFromContext(ctx context.Context) []string {
	return ctx.Value(queryContextKey).(queryContext).supportedWidgets
}

func PreferredLanguageFromContext(ctx context.Context) string {
	return ctx.Value(queryContextKey).(queryContext).preferredLanguage
}

func PreferredUnitsFromContext(ctx context.Context) string {
	return ctx.Value(queryContextKey).(queryContext).preferredUnits
}

func LocationFromContext(ctx context.Context) *Location {
	return ctx.Value(queryContextKey).(queryContext).location
}

func SupportsAction(ctx context.Context, action string) bool {
	return slices.Contains(SupportedActionsFromContext(ctx), action)
}

func SupportsAnyWidgets(ctx context.Context) bool {
	return len(SupportedWidgetsFromContext(ctx)) > 0
}

func SupportsWidget(ctx context.Context, widget string) bool {
	return slices.Contains(SupportedWidgetsFromContext(ctx), widget)
}

func ThreadIdFromContext(ctx context.Context) string {
	return ctx.Value(queryContextKey).(queryContext).threadId
}
