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

package widgets

import (
	"context"
	"strings"
)

type NumberWidget struct {
	Number string `json:"number"`
	Unit   string `json:"unit,omitempty"`
}

func numberWidget(ctx context.Context, number, unit string) (*NumberWidget, error) {
	unit = strings.TrimSpace(unit)
	if strings.EqualFold(unit, "none") {
		unit = ""
	}
	return &NumberWidget{
		Number: number,
		Unit:   unit,
	}, nil
}
