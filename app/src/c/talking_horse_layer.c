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
#include "util/perimeter.h"
#include <pebble.h>

typedef struct {
  GPerimeter perimeter;
  Layer *layer;
  const char* text;
  GDrawCommandImage* pony;
  GSize text_size;
  GTextAttributes *text_attributes;
} TalkingHorseLayerData;

const int SPEECH_BUBBLE_BASELINE = 59;

static void prv_update_layer(Layer *layer, GContext *ctx);
static GTextAttributes *prv_create_text_attributes(TalkingHorseLayer *layer);
static GRangeHorizontal prv_perimeter_callback(const GPerimeter *perimeter, const GSize *ctx_size, GRangeVertical vertical_range, uint16_t inset);


TalkingHorseLayer *talking_horse_layer_create(GRect frame) {
  Layer *layer = layer_create_with_data(frame, sizeof(TalkingHorseLayerData));
  TalkingHorseLayerData *data = layer_get_data(layer);
  data->perimeter = (GPerimeter) { .callback = prv_perimeter_callback };
  data->layer = layer;
  data->text = NULL;
  data->text_size = GSizeZero;
  data->pony = gdraw_command_image_create_with_resource(RESOURCE_ID_ROOT_SCREEN_PONY);
  data->text_attributes = prv_create_text_attributes(layer);
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
  data->text_size = graphics_text_layout_get_content_size_with_attributes(text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GRect(0, 1, bounds.size.w - 18, bounds.size.h - 15), GTextOverflowModeWordWrap, GTextAlignmentLeft, data->text_attributes);
  layer_mark_dirty(layer);
}

static void prv_update_layer(Layer *layer, GContext *ctx) {
  TalkingHorseLayerData *data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  GSize size = bounds.size;

  const int text_height = data->text_size.h + 5;
  const int speech_bubble_top = 1;
  const int available_space = bounds.size.w - 18 - data->text_size.w - 10;
  const int bubble_width = size.w - 16 - available_space;
  const int corner_offset = 6;

  GPoint tail_origin = GPoint(55 - available_space, size.h - 30 - speech_bubble_top);
  // When the text is three lines long, the tail runs into the bubble, so we need to move it.
  if (tail_origin.y < text_height + corner_offset) {
    tail_origin = GPoint(55 - available_space, size.h - 20 - speech_bubble_top);
  }

  GPath bubble_path = {
    .num_points = 11,
    .offset = GPoint(8 + available_space, speech_bubble_top),
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
      tail_origin,
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
  GRect text_bounds = GRect(8 + corner_offset + available_space, speech_bubble_top + corner_offset - 5, data->text_size.w, data->text_size.h);
  graphics_draw_text(ctx, data->text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), text_bounds, GTextOverflowModeWordWrap, GTextAlignmentLeft, data->text_attributes);
  gdraw_command_image_draw(ctx, data->pony, GPoint(0, size.h - 59));
}

static GTextAttributes* prv_create_text_attributes(TalkingHorseLayer *layer) {
  TalkingHorseLayerData *data = layer_get_data(layer);
  GTextAttributes *attributes = graphics_text_attributes_create();
  attributes->flow_data.perimeter.impl = &data->perimeter;
  attributes->flow_data.perimeter.inset = 0;
  return attributes;
}


static GRangeHorizontal prv_perimeter_callback(const GPerimeter *perimeter, const GSize *ctx_size, GRangeVertical vertical_range, uint16_t inset) {
  // We don't get a reference to the original layer, but we do get this perimeter pointer. By putting the perimeter at
  // the top of the struct, we can make this cast and get away with it.
  TalkingHorseLayerData *data = (TalkingHorseLayerData*)perimeter;
  Layer *layer = data->layer;
  // the top right of the pony is 59 pixels from the bottom of the layer, and we need it in screen space
  const int16_t pony_size = 59;
  GRect bounds = layer_get_bounds(layer);
  GPoint wrap_point = layer_convert_point_to_screen(layer, GPoint(pony_size, bounds.size.h - pony_size));
  // We know the pony is at the bottom of our layer, so we don't bother worrying about text being rendered past it.
  if (vertical_range.origin_y + vertical_range.size_h < wrap_point.y) {
    // nothing to do here - implement the inset while we're here, though.
    return (GRangeHorizontal) { .origin_x = inset, .size_w = ctx_size->w - inset * 2 };
  } else {
    // The pony is in the way, so we need to indent the text on the left.
    return (GRangeHorizontal) { .origin_x = wrap_point.x + inset, .size_w = ctx_size->w - wrap_point.x - inset * 2 };
  }
}