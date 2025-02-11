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

#ifndef ALARMS_MANAGER_H
#define ALARMS_MANAGER_H

#include <pebble.h>

typedef struct AlarmManager AlarmManager;
typedef struct Alarm Alarm;

void alarm_manager_init();
int alarm_manager_add_alarm(time_t when, bool is_timer);
int alarm_manager_cancel_alarm(time_t when, bool is_timer);
int alarm_manager_get_alarm_count();
Alarm* alarm_manager_get_alarm(int index);
bool alarm_manager_maybe_alarm();

time_t alarm_get_time(Alarm* alarm);
bool alarm_is_timer(Alarm* alarm);

#endif
