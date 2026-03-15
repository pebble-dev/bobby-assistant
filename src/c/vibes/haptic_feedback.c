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

#include "haptic_feedback.h"
#include "sad_vibe_score.h"

static SadVibeScore *s_haptic_feedback = NULL;

void vibe_haptic_feedback() {
  if (!s_haptic_feedback) {
    s_haptic_feedback = sad_vibe_score_create_with_resource(RESOURCE_ID_VIBE_HAPTIC_FEEDBACK);
  }
  sad_vibe_score_play(s_haptic_feedback);
}
