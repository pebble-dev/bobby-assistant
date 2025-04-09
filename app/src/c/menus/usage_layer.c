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

#include "usage_layer.h"
#include "../util/memory/sdk.h"

#include <pebble.h>

typedef struct {
  int16_t percentage;
} UsageLayerData;

static void prv_layer_update(Layer* layer, GContext* ctx);

UsageLayer* usage_layer_create(GRect frame) {
  UsageLayer* layer = blayer_create_with_data(frame, sizeof(UsageLayerData));
  UsageLayerData* data = layer_get_data(layer);
  layer_set_update_proc(layer, prv_layer_update);
  data->percentage = 0;
  return layer;
}

void usage_layer_destroy(UsageLayer* layer) {
  layer_destroy(layer);
}

void usage_layer_set_percentage(UsageLayer* layer, int16_t percentage) {
  UsageLayerData* data = layer_get_data(layer);
  data->percentage = percentage;
  layer_mark_dirty(layer);
}

static void prv_layer_update(Layer* layer, GContext* ctx) {
  UsageLayerData* data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(0, 0, (bounds.size.w * data->percentage) / PERCENTAGE_MAX, bounds.size.h), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, bounds);
}
