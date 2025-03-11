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

#include "talking_horse_layer.h"
#include <pebble.h>

typedef struct {
  const char* text;
  GDrawCommandImage* pony;
  int16_t text_height;
} TalkingHorseLayerData;

const int SPEECH_BUBBLE_BASELINE = 59;

static void prv_update_layer(Layer *layer, GContext *ctx);

TalkingHorseLayer *talking_horse_layer_create(GRect frame) {
  Layer *layer = layer_create_with_data(frame, sizeof(TalkingHorseLayerData));
  TalkingHorseLayerData *data = layer_get_data(layer);
  data->text = NULL;
  data->text_height = 0;
  data->pony = gdraw_command_image_create_with_resource(RESOURCE_ID_ROOT_SCREEN_PONY);
  layer_set_update_proc(layer, prv_update_layer);
  return layer;
}

void talking_horse_layer_destroy(TalkingHorseLayer *layer) {
  TalkingHorseLayerData *data = layer_get_data(layer);
  gdraw_command_image_destroy(data->pony);
  layer_destroy(layer);
}

void talking_horse_layer_set_text(TalkingHorseLayer *layer, const char *text) {
  TalkingHorseLayerData *data = layer_get_data(layer);
  data->text = text;
  GRect bounds = layer_get_bounds(layer);
  GSize text_size = graphics_text_layout_get_content_size(text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GRect(0, 0, bounds.size.w - 18, bounds.size.h - 70), GTextOverflowModeWordWrap, GTextAlignmentLeft);
  data->text_height = text_size.h;
  layer_mark_dirty(layer);
}

static void prv_update_layer(Layer *layer, GContext *ctx) {
  TalkingHorseLayerData *data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  GSize size = bounds.size;
  int text_height = data->text_height + 5;
  int speech_bubble_top = size.h - SPEECH_BUBBLE_BASELINE - text_height + 1;
  int bubble_width = size.w - 16;
  const int corner_offset = 6;
  GPath bubble_path = {
    .num_points = 11,
    .offset = GPoint(8, speech_bubble_top),
    .rotation = 0,
    .points = (GPoint[]) {
      // top left
      {0, corner_offset},
      {corner_offset, 0},
      // top right
      {bubble_width - corner_offset, 0},
      {bubble_width, corner_offset},
      // bottom right
      {bubble_width, text_height},
      {bubble_width - corner_offset, text_height + corner_offset},
      // tail
      {bubble_width - 20, text_height + corner_offset},
      {57, size.h - 25 - speech_bubble_top},
      {bubble_width - 30, text_height + corner_offset},
      // bottom left
      {corner_offset, text_height + corner_offset},
      {0, text_height},
    }
  };
  graphics_context_set_fill_color(ctx, GColorWhite);
  gpath_draw_filled(ctx, &bubble_path);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 3);
  gpath_draw_outline(ctx, &bubble_path);

  graphics_context_set_text_color(ctx, GColorBlack);
  GRect text_bounds = GRect(8 + corner_offset, speech_bubble_top + corner_offset - 5, bubble_width - corner_offset, text_height);
  graphics_draw_text(ctx, data->text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), text_bounds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  gdraw_command_image_draw(ctx, data->pony, GPoint(0, size.h - 59));
}