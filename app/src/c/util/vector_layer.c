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

#include "vector_layer.h"
#include <pebble.h>

typedef struct {
  GDrawCommandImage *vector;
  GColor background_color;
} VectorLayerData;

static void prv_layer_update(Layer *layer, GContext *ctx);

VectorLayer* vector_layer_create(GRect frame) {
  Layer *layer = layer_create_with_data(frame, sizeof(VectorLayerData));
  VectorLayerData *vector_layer_data = layer_get_data(layer);
  vector_layer_data->vector = NULL;
  vector_layer_data->background_color = GColorClear;
  layer_set_update_proc(layer, prv_layer_update);
  return layer;
}

void vector_layer_destroy(VectorLayer *layer) {
  layer_destroy(vector_layer_get_layer(layer));
}

Layer* vector_layer_get_layer(VectorLayer *layer) {
  return layer;
}

void vector_layer_set_vector(VectorLayer *layer, GDrawCommandImage *image) {
  VectorLayerData *data = layer_get_data(layer);
  data->vector = image;
  layer_mark_dirty(layer);
}

GDrawCommandImage* vector_layer_get_vector(VectorLayer *layer) {
  VectorLayerData *data = layer_get_data(layer);
  return data->vector;
}

void vector_layer_set_background_color(VectorLayer *layer, GColor color) {
  VectorLayerData *data = layer_get_data(layer);
  data->background_color = color;
}


static void prv_layer_update(Layer *layer, GContext *ctx) {
  VectorLayerData *data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  if (!gcolor_equal(data->background_color, GColorClear)) {
    graphics_context_set_fill_color(ctx, data->background_color);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  }
  gdraw_command_image_draw(ctx, data->vector, GPoint(0, 0));
}
