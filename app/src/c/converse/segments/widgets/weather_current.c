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

#include "weather_current.h"
#include "weather_util.h"
#include <pebble.h>

typedef struct {
  ConversationEntry* entry;
  GDrawCommandImage *icon;
  char temp_string[10];
  char feels_like_string[20];
  char wind_speed[10];
} WeatherCurrentWidgetData;

static void prv_layer_update(Layer *layer, GContext *ctx);

WeatherCurrentWidget* weather_current_widget_create(GRect rect, ConversationEntry* entry) {
  Layer *layer = layer_create_with_data(GRect(rect.origin.x, rect.origin.y, rect.size.w, 85), sizeof(WeatherCurrentWidgetData));
  WeatherCurrentWidgetData *data = layer_get_data(layer);
  ConversationWidgetWeatherCurrent *w = &conversation_entry_get_widget(entry)->widget.weather_current;

  data->entry = entry;
  data->icon = gdraw_command_image_create_with_resource(weather_widget_get_medium_resource_for_condition(w->condition));
  layer_set_update_proc(layer, prv_layer_update);

  snprintf(data->temp_string, sizeof(data->temp_string), "%d°", w->temperature);
  snprintf(data->wind_speed, sizeof(data->wind_speed), "%d %s", w->wind_speed, w->wind_speed_unit);
  snprintf(data->feels_like_string, sizeof(data->feels_like_string), "Seems %d°", w->feels_like);
  return layer;
}

static void prv_layer_update(Layer *layer, GContext *ctx) {
  WeatherCurrentWidgetData *data = layer_get_data(layer);
  ConversationWidgetWeatherCurrent *widget = &conversation_entry_get_widget(data->entry)->widget.weather_current;
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
  graphics_draw_text(ctx, data->temp_string, fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS), GRect(5, 15, bounds.size.w, 50), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, data->feels_like_string, fonts_get_system_font(FONT_KEY_GOTHIC_18), GRect(5, 45, bounds.size.w - 70, 40), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, widget->summary, fonts_get_system_font(FONT_KEY_GOTHIC_18), GRect(5, 62, bounds.size.w - 10, 20), GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  if (data->icon) {
    gdraw_command_image_draw(ctx, data->icon, GPoint(bounds.size.w - 60, 20));
  }
}

ConversationEntry* weather_current_widget_get_entry(WeatherCurrentWidget* layer) {
  WeatherCurrentWidgetData *data = layer_get_data(layer);
  return data->entry;
}

void weather_current_widget_destroy(WeatherCurrentWidget* layer) {
  WeatherCurrentWidgetData *data = layer_get_data(layer);
  if (data->icon) {
    gdraw_command_image_destroy(data->icon);
  }
  layer_destroy(layer);
}

void weather_current_widget_update(WeatherCurrentWidget* layer) {
  // nothing to do here.
}

