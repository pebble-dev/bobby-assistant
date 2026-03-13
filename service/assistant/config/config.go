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

import (
	"log"
	"os"

	"github.com/joho/godotenv"
)

// TODO: something reasonable.

type Config struct {
	BaseURL               string
	RedisURL              string
	UserIdentificationURL string
	HoneycombKey          string
	// Telegram configuration for Clawd
	TelegramAppID      string
	TelegramAppHash    string
	TelegramSessionKey string // 32-byte encryption key for session storage
}

var c Config

func GetConfig() *Config {
	return &c
}

func init() {
	// Load .env file if it exists
	if err := godotenv.Load(); err != nil {
		// Only log if the file exists but couldn't be loaded
		if !os.IsNotExist(err) {
			log.Printf("Error loading .env file: %v", err)
		}
	}

	c = Config{
		BaseURL:              os.Getenv("BASE_URL"),
		RedisURL:             os.Getenv("REDIS_URL"),
		UserIdentificationURL: os.Getenv("USER_IDENTIFICATION_URL"),
		HoneycombKey:         os.Getenv("HONEYCOMB_KEY"),
		TelegramAppID:        os.Getenv("TELEGRAM_APP_ID"),
		TelegramAppHash:     os.Getenv("TELEGRAM_APP_HASH"),
		TelegramSessionKey:  os.Getenv("TELEGRAM_SESSION_KEY"),
	}
}
