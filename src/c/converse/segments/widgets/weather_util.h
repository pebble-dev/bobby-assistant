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

#ifndef WEATHER_UTIL_H
#define WEATHER_UTIL_H

#include <pebble.h>

int weather_widget_get_medium_resource_for_condition(int condition);
int weather_widget_get_small_resource_for_condition(int condition);
GColor weather_widget_get_colour_for_condition(int condition);

#endif //WEATHER_UTIL_H
