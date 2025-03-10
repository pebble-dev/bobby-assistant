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

#include "weather_multi_day.h"
#include <pebble.h>
#include "weather_util.h"

typedef struct {
  ConversationEntry *entry;
  GDrawCommandImage *icons[3];
  char rendered_highs[3][6];
  char rendered_lows[3][6];
} WeatherMultiDayWidgetData;

static void prv_layer_update(Layer *layer, GContext *ctx);

WeatherMultiDayWidget* weather_multi_day_widget_create(GRect rect, ConversationEntry* entry) {
  Layer *layer = layer_create_with_data(GRect(rect.origin.x, rect.origin.y, rect.size.w, 110), sizeof(WeatherMultiDayWidgetData));
  WeatherMultiDayWidgetData *data = layer_get_data(layer);
  ConversationWidgetWeatherMultiDay *w = &conversation_entry_get_widget(entry)->widget.weather_multi_day;

  data->entry = entry;
  for (int i = 0; i < 3; i++) {
    data->icons[i] = gdraw_command_image_create_with_resource(weather_widget_get_small_resource_for_condition(w->days[i].condition));
    snprintf(data->rendered_highs[i], sizeof(data->rendered_highs[i]), "%d°", w->days[i].high);
    snprintf(data->rendered_lows[i], sizeof(data->rendered_lows[i]), "%d°", w->days[i].low);
  }

  layer_set_update_proc(layer, prv_layer_update);
  return layer;
}

static void prv_layer_update(Layer *layer, GContext *ctx) {
  WeatherMultiDayWidgetData *data = layer_get_data(layer);
  ConversationWidgetWeatherMultiDay *widget = &conversation_entry_get_widget(data->entry)->widget.weather_multi_day;
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(bounds.size.w, 0));
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, widget->location, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(0, 0, bounds.size.w, 20), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  const int SEGMENT_WIDTH = bounds.size.w / 3;
  for (int i = 0; i < 3; i++) {
    int x = i * SEGMENT_WIDTH;
#if defined(PBL_COLOR)
    graphics_context_set_text_color(ctx, GColorBlack);
#endif
    if (data->icons[i]) {
#if defined(PBL_COLOR)
      graphics_context_set_fill_color(ctx, weather_widget_get_colour_for_condition(widget->days[i].condition));
      graphics_fill_circle(ctx, GPoint(x + SEGMENT_WIDTH / 2, 52), 18);
#endif
      gdraw_command_image_draw(ctx, data->icons[i], GPoint(x + SEGMENT_WIDTH / 2 - 12, 40));
    }
    graphics_draw_text(ctx, widget->days[i].day, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(x, 17, SEGMENT_WIDTH, 20), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    // The temperatures are bumped a couple of pixels right to look more balanced
    graphics_draw_text(ctx, data->rendered_highs[i], fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS), GRect(x+2, 65, SEGMENT_WIDTH, 25), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
#if defined(PBL_COLOR)
    graphics_context_set_text_color(ctx, GColorDarkGray);
#endif
    graphics_draw_text(ctx, data->rendered_lows[i], fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS), GRect(x+2, 85, SEGMENT_WIDTH, 25), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

ConversationEntry* weather_multi_day_widget_get_entry(WeatherMultiDayWidget* layer) {
  WeatherMultiDayWidgetData *data = layer_get_data(layer);
  return data->entry;
}

void weather_multi_day_widget_destroy(WeatherMultiDayWidget* layer) {
  WeatherMultiDayWidgetData *data = layer_get_data(layer);
  for (int i = 0; i < 3; i++) {
    if (data->icons[i]) {
      gdraw_command_image_destroy(data->icons[i]);
    }
  }
}

void weather_multi_day_widget_update(WeatherMultiDayWidget* layer) {
  // nothing to do here.
}
