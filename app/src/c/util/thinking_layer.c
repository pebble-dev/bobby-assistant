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

#include "thinking_layer.h"

#include <pebble.h>

//#define CIRCLE_MAX_RADIUS 15
//#define CIRCLE_MIN_RADIUS 8

static void prv_animation_update(Animation* animation, const AnimationProgress progress);
static void prv_layer_render(Layer* layer, GContext* ctx);
int prv_progress_to_radius(AnimationProgress progress, int section, int max_radius);

static AnimationImplementation s_animation_implementation;

typedef struct {
  Animation *animation;
  AnimationProgress progress;
} ThinkingLayerData;

ThinkingLayer* thinking_layer_create(GRect rect) {
  if (s_animation_implementation.update == NULL) {
    s_animation_implementation = (AnimationImplementation){
        .update = prv_animation_update
    };
  }
  
  ThinkingLayer* layer = layer_create_with_data(rect, sizeof(ThinkingLayerData));
  ThinkingLayerData* data = layer_get_data(layer);
  data->progress = 0;
  data->animation = animation_create();
  animation_set_curve(data->animation, AnimationCurveLinear);
  animation_set_duration(data->animation, 1500);
  animation_set_play_count(data->animation, ANIMATION_PLAY_COUNT_INFINITE);
  animation_set_handlers(data->animation, (AnimationHandlers){
                                              .started = NULL,
                                              .stopped = NULL,
}, layer);
  animation_set_implementation(data->animation, &s_animation_implementation);
  layer_set_update_proc(layer, prv_layer_render);
  animation_schedule(data->animation);
  return layer;
}

void thinking_layer_destroy(ThinkingLayer *layer) {
  ThinkingLayerData* data = layer_get_data(layer);
  animation_destroy(data->animation);
  data->animation = NULL;
  layer_destroy(layer);
}

static void prv_animation_update(Animation* animation, const AnimationProgress progress) {
  ThinkingLayer* layer = animation_get_context(animation);
  ThinkingLayerData* data = layer_get_data(layer);
  data->progress = progress;
  layer_mark_dirty(layer);
}

static void prv_layer_render(Layer* layer, GContext* ctx) {
  ThinkingLayerData* data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  int max_radius = bounds.size.h / 2;
  graphics_context_set_fill_color(ctx, GColorBlack); // TODO: probably grey on colour devices.
  graphics_fill_circle(ctx, GPoint(bounds.origin.x + max_radius, max_radius), prv_progress_to_radius(data->progress, 0, max_radius));
  graphics_fill_circle(ctx, GPoint(bounds.origin.x + bounds.size.w / 2, max_radius), prv_progress_to_radius(data->progress, 1, max_radius));
  graphics_fill_circle(ctx, GPoint(bounds.origin.x + bounds.size.w - max_radius - 1, max_radius), prv_progress_to_radius(data->progress, 2, max_radius));
}

int prv_progress_to_radius(AnimationProgress progress, int section, int max_radius) {
  const int SEGMENT_SIZE = ANIMATION_NORMALIZED_MAX / 3;
  int min_radius = max_radius / 3 * 2;
  if (progress < section * SEGMENT_SIZE || progress > (section + 1) * SEGMENT_SIZE) {
    return min_radius;
  }
  progress = progress % SEGMENT_SIZE;
  if (progress < SEGMENT_SIZE / 2) {
    return min_radius + ((max_radius - min_radius) * progress / (SEGMENT_SIZE/2));
  } else {
    return max_radius - ((max_radius - min_radius) * (progress - SEGMENT_SIZE / 2) / (SEGMENT_SIZE/2));
  }
}
