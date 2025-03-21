/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "sad_vibe_score.h"

#include <pebble.h>

struct __attribute__((__packed__)) SadVibeScore {
  uint32_t pattern_duration;
  uint32_t repeat_delay_ms;
  uint16_t note_count;
  uint32_t *notes;
};

SadVibeScore *s_active_vibe_score = NULL;
AppTimer *s_timer = NULL;

static void prv_vibe_timer_callback(void* context);

SadVibeScore* sad_vibe_score_create_with_resource(uint32_t resource_id) {
  ResHandle res_handle = resource_get_handle(resource_id);
  SadVibeScore* score = malloc(sizeof(SadVibeScore));
  resource_load_byte_range(res_handle, 0, (uint8_t*)score, 10);
  score->notes = malloc(score->note_count * sizeof(uint32_t));
  resource_load_byte_range(res_handle, 10, (uint8_t*)score->notes, score->note_count * sizeof(uint32_t));
  return score;
}

void sad_vibe_score_destroy(SadVibeScore* score) {
  if (s_active_vibe_score == score) {
    sad_vibe_score_stop();
  }
  free(score->notes);
  free(score);
}

void sad_vibe_score_play(SadVibeScore* score) {
  sad_vibe_score_stop();
  s_active_vibe_score = score;
  vibes_enqueue_custom_pattern((VibePattern) {
    .durations = score->notes,
    .num_segments = score->note_count,
  });
  if (score->repeat_delay_ms) {
    s_timer = app_timer_register(score->pattern_duration + score->repeat_delay_ms, prv_vibe_timer_callback, NULL);
  }
}

void sad_vibe_score_stop() {
  if (s_active_vibe_score) {
    vibes_cancel();
    s_active_vibe_score = NULL;
  }
}

static void prv_vibe_timer_callback(void* context) {
  if (s_active_vibe_score) {
    sad_vibe_score_play(s_active_vibe_score);
  }
}
