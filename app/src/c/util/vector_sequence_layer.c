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

#include "vector_sequence_layer.h"
#include <pebble.h>

typedef struct {
  GDrawCommandSequence *sequence;
  AppTimer *timer;
  uint32_t current_frame;
  uint32_t play_count;
  GColor background_color;
} VectorSequenceLayerData;

static void prv_layer_update(Layer *layer, GContext *ctx);
static void prv_timer_callback(void *context);

VectorSequenceLayer* vector_sequence_layer_create(GRect frame) {
  Layer *layer = layer_create_with_data(frame, sizeof(VectorSequenceLayerData));
  VectorSequenceLayerData *vector_sequence_layer_data = layer_get_data(layer);
  vector_sequence_layer_data->sequence = NULL;
  vector_sequence_layer_data->background_color = GColorClear;
  vector_sequence_layer_data->timer = NULL;
  vector_sequence_layer_data->current_frame = 0;
  layer_set_update_proc(layer, prv_layer_update);
  return layer;
}

void vector_sequence_layer_destroy(VectorSequenceLayer *layer) {
  VectorSequenceLayerData *data = layer_get_data(layer);
  if (data->timer) {
    app_timer_cancel(data->timer);
  }
  layer_destroy(vector_sequence_layer_get_layer(layer));
}

Layer* vector_sequence_layer_get_layer(VectorSequenceLayer *layer) {
  return layer;
}

void vector_sequence_layer_set_sequence(VectorSequenceLayer *layer, GDrawCommandSequence *sequence) {
  VectorSequenceLayerData *data = layer_get_data(layer);
  data->sequence = sequence;
  layer_mark_dirty(layer);
}

GDrawCommandSequence* vector_sequence_layer_get_sequence(VectorSequenceLayer *layer) {
  VectorSequenceLayerData *data = layer_get_data(layer);
  return data->sequence;
}

void vector_sequence_layer_set_background_color(VectorSequenceLayer *layer, GColor color) {
  VectorSequenceLayerData *data = layer_get_data(layer);
  data->background_color = color;
}

void vector_sequence_layer_play(VectorSequenceLayer *layer) {
  VectorSequenceLayerData *data = layer_get_data(layer);
  if (data->timer) {
    app_timer_cancel(data->timer);
  }

  GDrawCommandFrame *frame = gdraw_command_sequence_get_frame_by_index(data->sequence, 0);
  data->timer = app_timer_register(gdraw_command_frame_get_duration(frame), prv_timer_callback, layer);
}

void vector_sequence_layer_stop(VectorSequenceLayer *layer) {
  VectorSequenceLayerData *data = layer_get_data(layer);
  if (data->timer) {
    app_timer_cancel(data->timer);
  }
}

static void prv_layer_update(Layer *layer, GContext *ctx) {
  VectorSequenceLayerData *data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  if (!gcolor_equal(data->background_color, GColorClear)) {
    graphics_context_set_fill_color(ctx, data->background_color);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  }
  GDrawCommandFrame *frame = gdraw_command_sequence_get_frame_by_index(data->sequence, data->current_frame);
  gdraw_command_frame_draw(ctx, data->sequence, frame, GPoint(0, 0));
}

static void prv_timer_callback(void *context) {
  VectorSequenceLayer *layer = context;
  VectorSequenceLayerData *data = layer_get_data(layer);
  uint32_t next_frame = data->current_frame + 1;
  if (next_frame >= gdraw_command_sequence_get_num_frames(data->sequence)) {
    uint32_t max_plays = gdraw_command_sequence_get_play_count(data->sequence);
    if (max_plays == PLAY_COUNT_INFINITE || data->play_count < max_plays) {
      next_frame = 0;
      data->play_count++;
    } else {
      return;
    }
  }
  data->current_frame = next_frame;
  GDrawCommandFrame *frame = gdraw_command_sequence_get_frame_by_index(data->sequence, data->current_frame);
  data->timer = app_timer_register(gdraw_command_frame_get_duration(frame), prv_timer_callback, layer);
  layer_mark_dirty(layer);
}
