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

package main

import (
	"github.com/honeycombio/beeline-go"
	"github.com/honeycombio/beeline-go/wrappers/hnynethttp"
	"github.com/pebble-dev/bobby-assistant/service/assistant"
	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/redact"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/storage"
	"log"
	"net/http"
)

func main() {
	beeline.Init(beeline.Config{
		WriteKey:    config.GetConfig().HoneycombKey,
		Dataset:     "rws",
		ServiceName: "bobby",
		PresendHook: redact.CleanHoneycomb,
	})
	defer beeline.Close()
	http.DefaultTransport = hnynethttp.WrapRoundTripper(http.DefaultTransport)
	service := assistant.NewService(storage.GetRedis())
	addr := "0.0.0.0:8080"
	log.Printf("Listening on %s.", addr)
	log.Fatal(service.ListenAndServe(addr))
}
