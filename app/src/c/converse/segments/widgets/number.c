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

#include "number.h"

typedef struct {
  ConversationEntry *entry;
  GFont number_font;
  int16_t number_height;
  int16_t unit_offset;
  bool fallback_mode;
} NumberWidgetData;

static void prv_layer_update(Layer *layer, GContext *ctx);
static void prv_choose_font(Layer *layer);
bool prv_is_sufficiently_numeric(const char* text);

NumberWidget* number_widget_create(GRect rect, ConversationEntry* entry) {
  Layer *number_layer = layer_create_with_data(GRect(rect.origin.x, rect.origin.y, rect.size.w, 60), sizeof(NumberWidgetData));
  NumberWidgetData *data = layer_get_data(number_layer);
  data->entry = entry;
  layer_set_update_proc(number_layer, prv_layer_update);
  prv_choose_font(number_layer);
  return number_layer;
}

ConversationEntry* number_widget_get_entry(NumberWidget* layer) {
  NumberWidgetData* data = layer_get_data(layer);
  return data->entry;
}

void number_widget_destroy(NumberWidget* layer) {
  layer_destroy(layer);
}

void number_widget_update(NumberWidget* layer) {
  // Nothing to do here.
}

bool prv_is_sufficiently_numeric(const char* text) {
  for (int i = 0; text[i] != '\0'; i++) {
    char c = text[i];
    if (c >= '0' && c <= '9') {
      continue;
    }
    if (c == '.' || c == ',' || c == '-' || c == '/' || c == ' ' || c == ':') {
      continue;
    }
    return false;
  }
  return true;
}

static void prv_choose_font(Layer *layer) {
  NumberWidgetData *data = layer_get_data(layer);
  ConversationWidgetNumber *widget = &conversation_entry_get_widget(data->entry)->widget.number;
  GRect bounds = layer_get_bounds(layer);
  GRect inset_bounds = grect_inset(bounds, GEdgeInsets(0, 5));

  GSize size;
  if (!prv_is_sufficiently_numeric(widget->number)) {
    data->fallback_mode = true;
    data->number_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    size = graphics_text_layout_get_content_size(widget->number, data->number_font, GRect(0, 0, inset_bounds.size.w, bounds.size.h + 100), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    data->number_height = size.h;
  } else {
    data->number_font = fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
    data->number_height = 32;
    size = graphics_text_layout_get_content_size(widget->number, data->number_font, GRect(0, 0, bounds.size.w + 100, bounds.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    if (size.w > inset_bounds.size.w) {
      data->number_font = fonts_get_system_font(FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM);
      data->number_height = 26;
      size = graphics_text_layout_get_content_size(widget->number, data->number_font, GRect(0, 0, bounds.size.w + 100, bounds.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      if (size.w > inset_bounds.size.w) {
        data->number_font = fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS);
        data->number_height = 20;
        size = graphics_text_layout_get_content_size(widget->number, data->number_font, GRect(0, 0, bounds.size.w + 100, bounds.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
        if (size.w > inset_bounds.size.w) {
          data->fallback_mode = true;
          data->number_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
          size = graphics_text_layout_get_content_size(widget->number, data->number_font, GRect(0, 0, inset_bounds.size.w, bounds.size.h + 100), GTextOverflowModeWordWrap, GTextAlignmentLeft);
          data->number_height = size.h;
        }
      }
    }
  }

  int16_t total_height = data->number_height;
  if (widget->unit) {
    if (data->fallback_mode) {
      data->unit_offset = -1;
    } else {
      GSize unit_size = graphics_text_layout_get_content_size(widget->unit, fonts_get_system_font(FONT_KEY_GOTHIC_24), GRect(0, 0, bounds.size.w + 100, bounds.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      if (size.w + unit_size.w + 3 <= inset_bounds.size.w) {
        data->unit_offset = size.w + 3;
      } else {
        data->unit_offset = -1;
      }
    }

    if (data->unit_offset == -1) {
      total_height += 24;
    }
  }
  GRect frame = layer_get_frame(layer);
  frame.size.h = total_height + 10;
  layer_set_frame(layer, frame);
}

static void prv_layer_update(Layer *layer, GContext *ctx) {
  NumberWidgetData *data = layer_get_data(layer);
  ConversationWidgetNumber *widget = &conversation_entry_get_widget(data->entry)->widget.number;
  GRect bounds = layer_get_bounds(layer);
  GRect inset_bounds = grect_inset(bounds, GEdgeInsets(0, 5));
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(bounds.size.w, 0));
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, widget->number, data->number_font, inset_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (widget->unit) {
    GRect unit_rect = GRect(inset_bounds.origin.x, inset_bounds.origin.y + data->number_height, inset_bounds.size.w, 20);
    if (data->unit_offset > -1) {
      unit_rect.origin.x = inset_bounds.origin.x + data->unit_offset;
      unit_rect.origin.y = data->number_height - 24;
    }
    graphics_draw_text(ctx, widget->unit, fonts_get_system_font(FONT_KEY_GOTHIC_24), unit_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}
