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

#ifndef APP_USAGE_LAYER_H
#define APP_USAGE_LAYER_H

#include <pebble.h>

typedef Layer UsageLayer;

#define PERCENTAGE_MAX 256

UsageLayer* usage_layer_create(GRect frame);
void usage_layer_destroy(UsageLayer* layer);
void usage_layer_set_percentage(UsageLayer* layer, int16_t percentage);

#endif //APP_USAGE_LAYER_H
