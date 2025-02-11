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

package util

import "strings"

var languages = map[string]string{
	"af":  "Afrikaans",
	"cs":  "Czech",
	"da":  "Danish",
	"de":  "German",
	"en":  "English",
	"fi":  "Finnish",
	"fil": "Filipino",
	"fr":  "French",
	"gl":  "Galician",
	"id":  "Indonesian",
	"is":  "Icelandic",
	"it":  "Italian",
	"ko":  "Korean",
	"lv":  "Latvian",
	"lt":  "Lithuanian",
	"hr":  "Croatian",
	"hu":  "Hungarian",
	"ms":  "Malay",
	"nl":  "Dutch",
	"no":  "Norwegian",
	"pt":  "Portuguese",
	"pl":  "Polish",
	"ro":  "Romanian",
	"ru":  "Russian",
	"es":  "Spanish",
	"sk":  "Slovak",
	"sl":  "Slovenian",
	"sv":  "Swedish",
	"sw":  "Swahili",
	"tr":  "Turkish",
	"zu":  "Zulu",
}

func GetLanguageName(code string) string {
	code = strings.SplitN(code, "_", 2)[0]
	code = strings.ToLower(code)
	if val, ok := languages[code]; ok {
		return val
	}
	return ""
}
