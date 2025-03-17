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

#ifndef APP_PERSIST_KEYS_H
#define APP_PERSIST_KEYS_H

// These keys are stored centrally so we can avoid accidental collisions.
// Remember: these numbers can *never* be changed.

// next key: 9

// We write the alarm count twice - once before doing any work, and once after.
// If they disagree we assume the lower number is correct.
#define PERSIST_KEY_ALARM_COUNT_ONE 1
#define PERSIST_KEY_ALARM_COUNT_TWO 2
#define PERSIST_KEY_ALARM_TIMES 3
#define PERSIST_KEY_ALARM_WAKEUP_IDS 4
#define PERSIST_KEY_ALARM_IS_TIMERS 5
#define PERSIST_KEY_ALARM_NAMES 8

// Store whether we have successfully requested location consent.
#define PERSIST_KEY_LOCATION_ENABLED 6

// Contains the version we were running the last time we were launched
#define PERSIST_KEY_VERSION 7

#endif //APP_PERSIST_KEYS_H
