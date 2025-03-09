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

#include "weather_util.h"
#include <pebble.h>

int weather_widget_get_resource_for_condition(int condition) {
  switch (condition) {
    case 1:
      return RESOURCE_ID_WEATHER_LIGHT_RAIN;
    case 2:
      return RESOURCE_ID_WEATHER_HEAVY_RAIN;
    case 3:
      return RESOURCE_ID_WEATHER_LIGHT_SNOW;
    case 4:
      return RESOURCE_ID_WEATHER_HEAVY_SNOW;
    case 5:
      return RESOURCE_ID_WEATHER_CLOUDY;
    case 6:
      return RESOURCE_ID_WEATHER_GENERIC;
    case 7:
      return RESOURCE_ID_WEATHER_PARTLY_CLOUDY;
    case 8:
      return RESOURCE_ID_WEATHER_SUNNY;
    default:
      return RESOURCE_ID_WEATHER_GENERIC;
  }
}
