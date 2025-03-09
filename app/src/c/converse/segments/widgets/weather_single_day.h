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

#ifndef WEATHER_SINGLE_DAY_H
#define WEATHER_SINGLE_DAY_H

#include <pebble.h>
#include "../../conversation.h"

typedef Layer WeatherSingleDayWidget;

WeatherSingleDayWidget* weather_single_day_widget_create(GRect rect, ConversationEntry* entry);
ConversationEntry* weather_single_day_widget_get_entry(WeatherSingleDayWidget* layer);
void weather_single_day_widget_destroy(WeatherSingleDayWidget* layer);
void weather_single_day_widget_update(WeatherSingleDayWidget* layer);

#endif //WEATHER_SINGLE_DAY_H
