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

package config

import "os"

// TODO: something reasonable.

type Config struct {
	GeminiKey             string
	MapboxKey             string
	IBMKey                string
	RedisURL              string
	UserIdentificationURL string
	HoneycombKey          string
}

var c Config

func GetConfig() *Config {
	return &c
}

func init() {
	c = Config{
		GeminiKey:             os.Getenv("GEMINI_KEY"),
		MapboxKey:             os.Getenv("MAPBOX_KEY"),
		IBMKey:                os.Getenv("IBM_KEY"),
		RedisURL:              os.Getenv("REDIS_URL"),
		UserIdentificationURL: os.Getenv("USER_IDENTIFICATION_URL"),
		HoneycombKey:          os.Getenv("HONEYCOMB_KEY"),
	}
}
