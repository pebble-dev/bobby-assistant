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

package currencies

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"net/url"
	"sync"
	"time"

	"github.com/honeycombio/beeline-go"
	"github.com/redis/go-redis/v9"

	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/storage"
)

type CurrencyExchangeData struct {
	Result             string             `json:"result"`
	ErrorType          string             `json:"error-type,omitempty"`
	TimeLastUpdateUnix int                `json:"time_last_update_unix"`
	TimeNextUpdateUnix int                `json:"time_next_update_unix"`
	BaseCode           string             `json:"base_code"`
	ConversionRates    map[string]float64 `json:"conversion_rates"`
}

var sharedCurrencyDataManager *DataManager
var sharedCurrencyDataManagerOnce sync.Once

func GetCurrencyDataManager() *DataManager {
	sharedCurrencyDataManagerOnce.Do(func() {
		sharedCurrencyDataManager = &DataManager{
			redisClient: storage.GetRedis(),
		}
	})
	return sharedCurrencyDataManager
}

type DataManager struct {
	redisClient *redis.Client
}

var ErrUnknownCurrency = errors.New("unknown currency code")
var ErrQuotaExceeded = errors.New("quota exceeded")

func (dm *DataManager) GetExchangeData(ctx context.Context, from string) (*CurrencyExchangeData, error) {
	ctx, span := beeline.StartSpan(ctx, "get_exchange_data")
	defer span.Send()
	if !IsValidCurrency(from) {
		return nil, fmt.Errorf("unknown currency code %q", from)
	}
	data, err := dm.loadCachedData(ctx, from)
	if err != nil {
		return nil, fmt.Errorf("couldn't load cached data: %w", err)
	}
	if data != nil {
		return data, nil
	}
	// TODO: if we had high usage, we should prevent multiple concurrent requests for the same currency
	//       but in practice this seems unlikely to be a major issue at our anticipated scale.
	data, err = dm.fetchExchangeRateData(ctx, from)
	if err != nil {
		return nil, fmt.Errorf("couldn't fetch exchange rate data: %w", err)
	}
	if err := dm.cacheData(ctx, from, data); err != nil {
		return nil, fmt.Errorf("error caching exchange rate data: %w", err)
	}
	return data, nil
}

func (dm *DataManager) fetchExchangeRateData(ctx context.Context, from string) (*CurrencyExchangeData, error) {
	ctx, span := beeline.StartSpan(ctx, "fetch_exchange_rate_data")
	defer span.Send()
	escaped := url.QueryEscape(from)
	request, err := http.NewRequest("GET", "https://v6.exchangerate-api.com/v6/"+config.GetConfig().ExchangeRateApiKey+"/latest/"+escaped, nil)
	if err != nil {
		return nil, err
	}
	resp, err := http.DefaultClient.Do(request)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	var data CurrencyExchangeData
	if err := json.NewDecoder(resp.Body).Decode(&data); err != nil {
		return nil, err
	}
	if data.Result != "success" {
		if data.Result != "error" {
			return nil, fmt.Errorf("unexpected result %q", data.Result)
		}
		switch data.ErrorType {
		case "unsupported-code":
			return nil, ErrUnknownCurrency
		case "quota-reached":
			return nil, ErrQuotaExceeded
		default:
			return nil, fmt.Errorf("error fetching currency data: %s", data.ErrorType)
		}
	}
	return &data, nil
}

func (dm *DataManager) cacheData(ctx context.Context, currency string, data *CurrencyExchangeData) error {
	ctx, span := beeline.StartSpan(ctx, "cache_data")
	defer span.Send()
	encoded, err := json.Marshal(data)
	if err != nil {
		return err
	}
	expirationTime := time.Unix(int64(data.TimeNextUpdateUnix+5), 0)
	if err := dm.redisClient.Set(ctx, keyFromCurrency(currency), encoded, expirationTime.Sub(time.Now())).Err(); err != nil {
		return err
	}
	return nil
}

func (dm *DataManager) loadCachedData(ctx context.Context, currency string) (*CurrencyExchangeData, error) {
	ctx, span := beeline.StartSpan(ctx, "load_cached_data")
	defer span.Send()
	data, err := dm.redisClient.Get(ctx, keyFromCurrency(currency)).Result()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			return nil, nil
		}
		return nil, err
	}
	var decoded CurrencyExchangeData
	if err := json.Unmarshal([]byte(data), &decoded); err != nil {
		return nil, err
	}
	return &decoded, nil
}

func keyFromCurrency(from string) string {
	return "currency:" + from
}
