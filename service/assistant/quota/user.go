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

package quota

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"strings"

	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
)

type UserInfo struct {
	UserId          int  `json:"user_id"`
	HasSubscription bool `json:"has_subscription"`
}

func GetUserInfo(ctx context.Context, token string) (*UserInfo, error) {
	if token == "" {
		return nil, fmt.Errorf("no token provided")
	}
	values := &url.Values{}
	values.Set("token", token)
	s := values.Encode()
	r := strings.NewReader(s)
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, config.GetConfig().UserIdentificationURL, r)
	if err != nil {
		log.Printf("Error creating user id request: %v", err)
		return nil, err
	}
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		log.Printf("Error getting user id: %v", err)
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		log.Printf("Error getting user id: %v", resp.Status)
		result, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("error from user id service: %s", string(result))
	}
	var u UserInfo
	if err := json.NewDecoder(resp.Body).Decode(&u); err != nil {
		log.Printf("Error decoding user id response: %v", err)
		return nil, err
	}
	return &u, nil
}
