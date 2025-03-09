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

#include "weather_single_day.h"
#include "weather_util.h"
#include <pebble.h>

typedef struct {
  ConversationEntry *entry;
  GDrawCommandImage *icon;
  char temp_summary[20];
} WeatherSingleDayWidgetData;

static void prv_layer_update(Layer *layer, GContext *ctx);

WeatherSingleDayWidget* weather_single_day_widget_create(GRect rect, ConversationEntry* entry) {
  // Our contract is that we will actually disregard the provided height and resize ourselves as necessary.
  Layer *layer = layer_create_with_data(GRect(rect.origin.x, rect.origin.y, rect.size.w, 90), sizeof(WeatherSingleDayWidgetData));
  WeatherSingleDayWidgetData *data = layer_get_data(layer);
  ConversationWidgetWeatherSingleDay *w = &conversation_entry_get_widget(entry)->widget.weather_single_day;

  data->entry = entry;
  data->icon = gdraw_command_image_create_with_resource(weather_widget_get_resource_for_condition(w->condition));
  layer_set_update_proc(layer, prv_layer_update);

  snprintf(data->temp_summary, sizeof(data->temp_summary)-1, "H: %d°\nL: %d°", w->high, w->low);
  return layer;
}

static void prv_layer_update(Layer *layer, GContext *ctx) {
  WeatherSingleDayWidgetData *data = layer_get_data(layer);
  ConversationWidgetWeatherSingleDay *widget = &conversation_entry_get_widget(data->entry)->widget.weather_single_day;
  GRect bounds = layer_get_bounds(layer);
#if defined(PBL_COLOR)
  graphics_context_set_fill_color(ctx, widget->background_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
#endif
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(bounds.size.w, 0));
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(gcolor_legible_over(widget->background_color), GColorBlack));
  graphics_draw_text(ctx, widget->location, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(5, 0, bounds.size.w, 20), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, widget->day, fonts_get_system_font(FONT_KEY_GOTHIC_18), GRect(5, 15, bounds.size.w, 20), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, data->temp_summary, fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS), GRect(5, 40, bounds.size.w, 50), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, widget->summary, fonts_get_system_font(FONT_KEY_GOTHIC_18), GRect(0, 67, bounds.size.w - 5, 20), GTextOverflowModeFill, GTextAlignmentRight, NULL);

  if (data->icon) {
    gdraw_command_image_draw(ctx, data->icon, GPoint(bounds.size.w - 60, 20));
  }
}

ConversationEntry* weather_single_day_widget_get_entry(WeatherSingleDayWidget* layer) {
  WeatherSingleDayWidgetData *data = layer_get_data(layer);
  return data->entry;
}

void weather_single_day_widget_destroy(WeatherSingleDayWidget* layer) {
  WeatherSingleDayWidgetData *data = layer_get_data(layer);
  if (data->icon) {
    gdraw_command_image_destroy(data->icon);
  }
  layer_destroy(layer);
}

void weather_single_day_widget_update(WeatherSingleDayWidget* layer) {
  // nothing to do here.
}
