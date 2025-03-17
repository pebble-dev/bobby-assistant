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

#include "timer.h"
#include "../../conversation.h"
#include "../../../util/style.h"
#include <pebble.h>
#include <pebble-events/pebble-events.h>

typedef struct {
  ConversationEntry* entry;
  GDrawCommandImage *icon;
  EventHandle event_handle;
  char text[12];
} TimerWidgetData;

static void prv_layer_update(Layer *layer, GContext *ctx);
static void prv_handle_tick(struct tm *tick_time, TimeUnits units_changed, void *context);
static void prv_update_text_buffer(TimerWidgetData* data);

TimerWidget* timer_widget_create(GRect rect, ConversationEntry* entry) {
  Layer *layer = layer_create_with_data(GRect(rect.origin.x, rect.origin.y, rect.size.w, 53), sizeof(TimerWidgetData));
  TimerWidgetData* data = layer_get_data(layer);

  data->entry = entry;
  data->icon = gdraw_command_image_create_with_resource(RESOURCE_ID_TIMER_ICON);
  prv_update_text_buffer(data);
  layer_set_update_proc(layer, prv_layer_update);

  data->event_handle = events_tick_timer_service_subscribe_context(SECOND_UNIT, prv_handle_tick, layer);
  return layer;
}

ConversationEntry* timer_widget_get_entry(TimerWidget* layer) {
  TimerWidgetData* data = layer_get_data(layer);
  return data->entry;
}

void timer_widget_destroy(TimerWidget* layer) {
  TimerWidgetData* data = layer_get_data(layer);
  gdraw_command_image_destroy(data->icon);
  events_tick_timer_service_unsubscribe(data->event_handle);
  layer_destroy(layer);
}

void timer_widget_update(TimerWidget* layer) {
  // nothing to do here.
}

static void prv_layer_update(Layer *layer, GContext *ctx) {
  TimerWidgetData* data = layer_get_data(layer);
  ConversationWidgetTimer *widget = &conversation_entry_get_widget(data->entry)->widget.timer;
  GRect bounds = layer_get_bounds(layer);
#if defined(PBL_COLOR)
  graphics_context_set_fill_color(ctx, BRANDED_BACKGROUND_COLOUR);
  graphics_context_set_text_color(ctx, gcolor_legible_over(BRANDED_BACKGROUND_COLOUR));
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
#else
  graphics_context_set_text_color(ctx, GColorBlack);
#endif
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(bounds.size.w, 0));
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));

  gdraw_command_image_draw(ctx, data->icon, GPoint(5, 3));

  const int16_t icon_space = 26;
  const GRect title_rect = GRect(icon_space, bounds.origin.y, bounds.size.w - icon_space, 20);
  const GRect time_rect = GRect(5, bounds.origin.y + 16, bounds.size.w - 5, bounds.size.h);

  GFont time_font = fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
  GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  graphics_draw_text(ctx, widget->name ? widget->name : "Timer", title_font, title_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, data->text, time_font, time_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void prv_handle_tick(struct tm *tick_time, TimeUnits units_changed, void *context) {
  TimerWidget* layer = context;
  TimerWidgetData* data = layer_get_data(layer);
  prv_update_text_buffer(data);
  layer_mark_dirty(context);
}

static void prv_update_text_buffer(TimerWidgetData* data) {
  time_t now = time(NULL);
  ConversationWidgetTimer *widget = &conversation_entry_get_widget(data->entry)->widget.timer;

  if (widget->target_time <= now) {
    strncpy(data->text, "0:00", sizeof(data->text));
    return;
  }
  time_t interval = widget->target_time - now;
  int hours = interval / 3600;
  int minutes = (interval % 3600) / 60;
  int seconds = interval % 60;
  if (hours >= 10) {
    snprintf(data->text, sizeof(data->text), "%d:%02d", hours, minutes);
  } else if (hours > 0) {
    snprintf(data->text, sizeof(data->text), "%d:%02d:%02d", hours, minutes, seconds);
  } else {
    snprintf(data->text, sizeof(data->text), "%d:%02d", minutes, seconds);
  }
  data->text[sizeof(data->text) - 1] = '\0';
}