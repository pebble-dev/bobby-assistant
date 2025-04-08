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
	"encoding/json"
	"log"
	"regexp"
)

var valueRegexp = regexp.MustCompile(`(?m)^\s*".+?"\s*:\s*(.+?),?\s*$`)

// FixupBrokenJson takes a string that is supposed to be JSON, but erroneously contains expressions where there
// should be numbers, and evaluates those expressions.
func FixupBrokenJson(j string) string {
	var throwaway any
	if err := json.Unmarshal([]byte(j), &throwaway); err == nil {
		return j
	}
	values := valueRegexp.FindAllStringSubmatchIndex(j, -1)
	newRequest := ""
	lastIndex := 0
	for _, v := range values {
		newRequest += j[lastIndex:v[2]]
		expression := j[v[2]:v[3]]
		err := json.Unmarshal([]byte(expression), &throwaway)
		if err != nil {
			replacement := evalExpression(expression)
			log.Printf("Replacing expression %q with its value %q", expression, replacement)
			expression = replacement
		}
		newRequest += expression
		lastIndex = v[3]
	}
	newRequest += j[lastIndex:]
	return newRequest
}
