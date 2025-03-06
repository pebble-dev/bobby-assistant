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

#include "alarm_window.h"
#include "../util/style.h"

#include <pebble-events/pebble-events.h>
#include <pebble.h>

typedef struct {
  time_t time;
  bool is_timer;
  TextLayer *title_layer;
  TextLayer *time_layer;
  StatusBarLayer *status_bar;
  AppTimer* timer;
  EventHandle tick_handle;
  char time_content[20];
} AlarmWindowData;

static void prv_window_load(Window *window);
static void prv_window_appear(Window *window);
static void prv_window_disappear(Window *window);
static void prv_window_unload(Window *window);
static void prv_do_vibe(Window *window);
static void prv_stop_vibe(Window *window);
static void prv_timer_callback(void* ctx);
static void prv_tick_callback(struct tm *tick_time, TimeUnits units_changed, void* context);

void alarm_window_push(time_t alarm_time, bool is_timer) {
  Window* window = window_create();
  AlarmWindowData *data = malloc(sizeof(AlarmWindowData));
  data->time = alarm_time;
  data->is_timer = is_timer;
  data->timer = NULL;
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
      .load = prv_window_load,
      .unload = prv_window_unload,
      .appear = prv_window_appear,
      .disappear = prv_window_disappear,
  });
  window_stack_push(window, true);
}

static void prv_window_load(Window *window) {
  AlarmWindowData* data = window_get_user_data(window);
  Layer* root_layer = window_get_root_layer(window);
  GRect rect = layer_get_bounds(root_layer);
  data->title_layer = text_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, rect.size.w, 35));
  text_layer_set_text(data->title_layer, data->is_timer ? "Time's up!" : "Alarm!");
  text_layer_set_font(data->title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(data->title_layer, GTextAlignmentCenter);
  text_layer_set_background_color(data->title_layer, GColorClear);
  layer_add_child(root_layer, (Layer *)data->title_layer);
  data->time_layer = text_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT + 35 + 20, rect.size.w, 45));
  text_layer_set_font(data->time_layer, fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS));
  text_layer_set_text_alignment(data->time_layer, GTextAlignmentCenter);
  text_layer_set_background_color(data->time_layer, GColorClear);
  layer_add_child(root_layer, (Layer *)data->time_layer);
  data->tick_handle = events_tick_timer_service_subscribe_context(SECOND_UNIT, prv_tick_callback, window);
  time_t now = time(NULL);
  prv_tick_callback(localtime(&now), SECOND_UNIT, window);
  window_set_background_color(window, COLOR_FALLBACK(ACCENT_COLOUR, GColorWhite));
  if (data->is_timer) {
    data->status_bar = status_bar_layer_create();
    bobby_status_bar_result_pane_config(data->status_bar);
    layer_add_child(root_layer, (Layer *)data->status_bar);
  } else {
    data->status_bar = NULL;
  }
}

static void prv_window_unload(Window *window) {
  AlarmWindowData* data = window_get_user_data(window);
  text_layer_destroy(data->title_layer);
  text_layer_destroy(data->time_layer);
  if (data->status_bar) {
    status_bar_layer_destroy(data->status_bar);
  }
  events_tick_timer_service_unsubscribe(data->tick_handle);
  free(data);
}

static void prv_window_appear(Window *window) {
  light_enable_interaction();
  prv_do_vibe(window);
}

static void prv_window_disappear(Window *window) {
  prv_stop_vibe(window);
}

static void prv_do_vibe(Window *window) {
  AlarmWindowData* data = window_get_user_data(window);
  // If we've been vibing for over ten minutes, give up.
  if (time(NULL) - data->time > 600) {
    prv_stop_vibe(window);
    return;
  }
  if (data->timer != NULL) {
    app_timer_cancel(data->timer);
  }
  data->timer = app_timer_register(6000, prv_timer_callback, window);
  static const uint32_t const vibe_segments[] = {2000, 1000, 2000};
  vibes_enqueue_custom_pattern((VibePattern) {
    .durations = vibe_segments,
    .num_segments = ARRAY_LENGTH(vibe_segments),
  });
}

static void prv_stop_vibe(Window *window) {
  AlarmWindowData* data = window_get_user_data(window);
  if (data->timer != NULL) {
    app_timer_cancel(data->timer);
    data->timer = NULL;
  }
  vibes_cancel();
}

static void prv_timer_callback(void* ctx) {
  prv_do_vibe(ctx);
}

static void prv_tick_callback(struct tm *tick_time, TimeUnits units_changed, void* context) {
    Window* window = context;
    AlarmWindowData* data = window_get_user_data(window);
    if (data->is_timer) {
      int difference = time(NULL) - data->time;
      int minutes = difference / 60;
      int seconds = difference % 60;
      if (minutes > 59) {
        int hours = difference / 3600;
        minutes = minutes % 60;
        snprintf(data->time_content, 20, "-%02d:%02d", hours, minutes);
      } else {
        snprintf(data->time_content, 20, "-%02d:%02d", minutes, seconds);
      }
    } else {
      int hours = tick_time->tm_hour;
      int minutes = tick_time->tm_min;
      if (clock_is_24h_style()) {
        snprintf(data->time_content, 20, "%02d:%02d", hours, minutes);
      } else {
        snprintf(data->time_content, 20, "%d:%02d", hours % 12, minutes);
      }
    }
    text_layer_set_text(data->time_layer, data->time_content);
};
