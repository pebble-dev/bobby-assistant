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

#include "time.h"
#include <pebble.h>

void format_time(char *buffer, size_t size, struct tm *time) {
  if (clock_is_24h_style()) {
    strftime(buffer, size, "%H:%M", time);
  } else if (time->tm_hour > 0 && time->tm_hour < 10) {
    snprintf(buffer, size, "%d", time->tm_hour);
    strftime(buffer + 1, size - 1, ":%M", time);
  } else if (time->tm_hour > 12 && time->tm_hour < 22) {
    snprintf(buffer, 2, "%d", time->tm_hour - 12);
    strftime(buffer + 1, size - 1, ":%M", time);
  } else {
    strftime(buffer, size, "%I:%M", time);
  }
}

void format_time_ampm(char *buffer, size_t size, struct tm *time) {
  format_time(buffer, size, time);
  if (!clock_is_24h_style()) {
    size_t length = strlen(buffer);
    strftime(buffer + length, size - length, " %p", time);
  }
}