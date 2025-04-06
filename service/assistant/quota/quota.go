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
	"fmt"
	"github.com/honeycombio/beeline-go"
	"log"
	"time"

	"github.com/redis/go-redis/v9"
)

// one credit is worth $0.000000025.
const InputTokenCredits = 4
const OutputTokenCredits = 16
const LiteInputTokenCredits = 3
const LiteOutputTokenCredits = 12
const WeatherQueryCredits = 21_000
const PoiSearchCredits = 1_400_000
const RouteCalculationCredits = 80_000
const MonthlyQuotaCredits = 80_000_000

type Tracker struct {
	redis  *redis.Client
	userId int
}

func NewTracker(redisClient *redis.Client, userId int) *Tracker {
	return &Tracker{
		redis:  redisClient,
		userId: userId,
	}
}

func (q *Tracker) ChargeInputQuota(ctx context.Context, tokenCount int) (credits int, err error) {
	credits = tokenCount * InputTokenCredits
	total, err := q.chargeCredits(ctx, q.userId, credits)
	return total, err
}

func (q *Tracker) ChargeOutputQuota(ctx context.Context, tokenCount int) (credits int, err error) {
	total, err := q.chargeCredits(ctx, q.userId, tokenCount*OutputTokenCredits)
	return total, err
}

func (q *Tracker) GetQuota(ctx context.Context) (used, remaining int, err error) {
	ctx, span := beeline.StartSpan(ctx, "get_quota")
	defer span.Send()
	result := q.redis.Get(ctx, keyForUserQuota(q.userId))
	if result.Err() == redis.Nil {
		return 0, MonthlyQuotaCredits, nil
	}
	if result.Err() != nil {
		return 0, 0, result.Err()
	}
	used, err = result.Int()
	if err != nil {
		return 0, 0, err
	}
	return used, MonthlyQuotaCredits - used, nil
}

func keyForUserQuota(user int) string {
	now := time.Now()
	return fmt.Sprintf("quota:%02d%02d:%d", now.Year()%100, now.Month(), user)
}

func (q *Tracker) chargeCredits(ctx context.Context, user, credits int) (int, error) {
	ctx, span := beeline.StartSpan(ctx, "charge_credits")
	defer span.Send()
	result := q.redis.IncrBy(ctx, keyForUserQuota(user), int64(credits))
	if result.Err() != nil {
		span.AddField("error", result.Err())
		return 0, result.Err()
	}
	i, err := result.Uint64()
	if err != nil {
		span.AddField("error", result.Err())
		return 0, err
	}
	if int(i) == credits {
		_, err = q.redis.Expire(ctx, keyForUserQuota(user), 45*24*time.Hour).Result()
		if err != nil {
			span.AddField("error", result.Err())
			return 0, err
		}
	}
	return int(i), nil
}

func (q *Tracker) ChargeCredits(ctx context.Context, credits int) error {
	used, err := q.chargeCredits(ctx, q.userId, credits)
	log.Printf("Charging %d credits to user %d. Total used: %d\n", credits, q.userId, used)
	return err
}

func (q *Tracker) ChargeUserOrGlobalQuota(ctx context.Context, quotaType string, globalMax int, userCredits int) error {
	ctx, span := beeline.StartSpan(ctx, "charge_user_or_global_quota")
	defer span.Send()
	// Try charging the global quota first
	charged, err := q.ChargeGlobalQuota(ctx, quotaType, globalMax)
	if err != nil {
		return err
	}
	if charged {
		log.Printf("Charged against global quota %s\n", quotaType)
		return nil
	}
	// Charge the user for the function call.
	err = q.ChargeCredits(ctx, userCredits)
	if err != nil {
		span.AddField("error", err)
		return err
	}
	return nil
}

// MaybeChargeGlobalQuota charges a single point against a global monthly quota and returns true, if the quota is below
// max; otherwise still charges it but returns false.
func (q *Tracker) ChargeGlobalQuota(ctx context.Context, quotaType string, max int) (bool, error) {
	ctx, span := beeline.StartSpan(ctx, "maybe_charge_global_quota")
	defer span.Send()
	// The quota key is per month, so we use the current month and year as a prefix.
	now := time.Now()
	quotaKey := fmt.Sprintf("global_quota:%02d%02d:%s", now.Year()%100, now.Month(), quotaType)
	result := q.redis.Incr(ctx, quotaKey)
	if result.Err() != nil {
		span.AddField("error", result.Err())
		return false, result.Err()
	}
	// If the quota is 1 (i.e. we are the first to set it), set the expiration to 45 days so it eventually cleans up.
	if result.Val() == 1 {
		_, err := q.redis.Expire(ctx, quotaKey, 45*24*time.Hour).Result()
		if err != nil {
			span.AddField("error", result.Err())
			return false, err
		}
	}
	// Return true if we're at or below the max.
	return result.Val() <= int64(max), nil
}
