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

#include "settings.h"
#include <pebble.h>
#include <pebble-events/pebble-events.h>

#include "../util/persist_keys.h"

static EventHandle s_event_handle;

static void prv_app_message_handler(DictionaryIterator *iter, void *context);

void settings_init() {
  s_event_handle = events_app_message_register_inbox_received(prv_app_message_handler, NULL);
}

void settings_deinit() {
  events_app_message_unsubscribe(s_event_handle);
}

QuickLaunchBehaviour settings_get_quick_launch_behaviour() {
  int result = persist_read_int(PERSIST_KEY_QUICK_LAUNCH_BEHAVIOUR);
  if (result == 0) {
    return QuickLaunchBehaviourConverseWithTimeout;
  }
  return result;
}

VibePatternSetting settings_get_alarm_vibe_pattern() {
  int result = persist_read_int(PERSIST_KEY_ALARM_VIBE_PATTERN);
  if (result == 0) {
    return VibePatternSettingStandard;
  }
  return result;
}

VibePatternSetting settings_get_timer_vibe_pattern() {
  int result = persist_read_int(PERSIST_KEY_TIMER_VIBE_PATTERN);
  if (result == 0) {
    return VibePatternSettingStandard;
  }
  return result;
}

static void prv_app_message_handler(DictionaryIterator *iter, void *context) {
  for (Tuple *tuple = dict_read_first(iter); tuple; tuple = dict_read_next(iter)) {
    if (tuple->key == MESSAGE_KEY_QUICK_LAUNCH_BEHAVIOUR) {
      int value = atoi(tuple->value->cstring);
      persist_write_int(PERSIST_KEY_QUICK_LAUNCH_BEHAVIOUR, value);
    } else if (tuple->key == MESSAGE_KEY_ALARM_VIBE_PATTERN) {
      persist_write_int(PERSIST_KEY_ALARM_VIBE_PATTERN, atoi(tuple->value->cstring));
    } else if (tuple->key == MESSAGE_KEY_TIMER_VIBE_PATTERN) {
      persist_write_int(PERSIST_KEY_TIMER_VIBE_PATTERN, atoi(tuple->value->cstring));
    }
  }
}