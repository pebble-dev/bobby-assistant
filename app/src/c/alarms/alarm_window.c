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
#include "../util/vector_sequence_layer.h"
#include "../util/result_window.h"
#include "../settings/settings.h"
#include "../vibes/sad_vibe_score.h"

#include <pebble-events/pebble-events.h>
#include <pebble.h>

#include "manager.h"

typedef struct {
  time_t time;
  bool is_timer;
  char *name;
  TextLayer *title_layer;
  TextLayer *time_layer;
  StatusBarLayer *status_bar;
  AppTimer* timer;
  EventHandle tick_handle;
  VectorSequenceLayer *animation_layer;
  GDrawCommandSequence *draw_commands;
  GBitmap *icon_snooze;
  GBitmap *icon_x;
  ActionBarLayer *action_bar;
  char time_content[20];
  SadVibeScore* vibes;
} AlarmWindowData;

static void prv_window_load(Window *window);
static void prv_window_appear(Window *window);
static void prv_window_disappear(Window *window);
static void prv_window_unload(Window *window);
static void prv_do_vibe(Window *window);
static void prv_stop_vibe(Window *window);
static void prv_timer_callback(void* ctx);
static void prv_tick_callback(struct tm *tick_time, TimeUnits units_changed, void* context);
static void prv_click_config_provider(void *context);
static void prv_handle_snooze(ClickRecognizerRef recognizer, void *context);
static void prv_handle_dismiss(ClickRecognizerRef recognizer, void *context);
static uint32_t prv_resource_id_for_setting(VibePatternSetting setting);
static SadVibeScore *prv_load_vibe_score(bool is_timer);

void alarm_window_push(time_t alarm_time, bool is_timer, char *name) {
  Window* window = window_create();
  AlarmWindowData *data = malloc(sizeof(AlarmWindowData));
  data->time = alarm_time;
  data->is_timer = is_timer;
  data->timer = NULL;
  data->name = NULL;
  if (name) {
    size_t len = strlen(name);
    data->name = malloc(len + 1);
    strncpy(data->name, name, len + 1);
  }
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
  data->title_layer = text_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, rect.size.w - ACTION_BAR_WIDTH, 70));
  if (data->name) {
    text_layer_set_text(data->title_layer, data->name);
  } else {
    text_layer_set_text(data->title_layer, data->is_timer ? "Time's up!" : "Alarm!");
  }
  text_layer_set_font(data->title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(data->title_layer, GTextAlignmentCenter);
  text_layer_set_background_color(data->title_layer, GColorClear);
  layer_add_child(root_layer, (Layer *)data->title_layer);
  GSize title_size = text_layer_get_content_size(data->title_layer);
  int16_t remaining_height = rect.size.h - STATUS_BAR_LAYER_HEIGHT - title_size.h - 49;
  data->time_layer = text_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT + title_size.h + remaining_height / 2 - 22 / 2, rect.size.w - ACTION_BAR_WIDTH, 32));
  text_layer_set_font(data->time_layer, fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS));
  text_layer_set_text_alignment(data->time_layer, GTextAlignmentCenter);
  text_layer_set_background_color(data->time_layer, GColorClear);
  layer_add_child(root_layer, (Layer *)data->time_layer);
  data->tick_handle = events_tick_timer_service_subscribe_context(SECOND_UNIT, prv_tick_callback, window);
  time_t now = time(NULL);
  prv_tick_callback(localtime(&now), SECOND_UNIT, window);
  window_set_background_color(window, COLOR_FALLBACK(ACCENT_COLOUR, GColorWhite));
  if (data->is_timer) {
    data->status_bar = status_bar_layer_create();
    layer_set_frame(status_bar_layer_get_layer(data->status_bar), GRect(0, 0, rect.size.w - ACTION_BAR_WIDTH, STATUS_BAR_LAYER_HEIGHT));
    bobby_status_bar_result_pane_config(data->status_bar);
    layer_add_child(root_layer, (Layer *)data->status_bar);
  } else {
    data->status_bar = NULL;
  }
  data->icon_snooze = gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_SNOOZE);
  data->icon_x = gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_X);
  data->action_bar = action_bar_layer_create();
  action_bar_layer_set_icon(data->action_bar, BUTTON_ID_UP, data->icon_snooze);
  action_bar_layer_set_icon(data->action_bar, BUTTON_ID_DOWN, data->icon_x);
  action_bar_layer_set_context(data->action_bar, window);
  action_bar_layer_set_click_config_provider(data->action_bar, prv_click_config_provider);
  action_bar_layer_add_to_window(data->action_bar, window);
  data->animation_layer = vector_sequence_layer_create(GRect((rect.size.w - ACTION_BAR_WIDTH) / 2 - 25, rect.size.h - 55, 50, 50));
  data->draw_commands = gdraw_command_sequence_create_with_resource(RESOURCE_ID_TIRED_PONY);
  data->vibes = prv_load_vibe_score(data->is_timer);
  vector_sequence_layer_set_sequence(data->animation_layer, data->draw_commands);
  layer_add_child(root_layer, (Layer *)data->animation_layer);
}

static void prv_window_unload(Window *window) {
  AlarmWindowData* data = window_get_user_data(window);
  text_layer_destroy(data->title_layer);
  text_layer_destroy(data->time_layer);
  if (data->status_bar) {
    status_bar_layer_destroy(data->status_bar);
  }
  action_bar_layer_destroy(data->action_bar);
  gbitmap_destroy(data->icon_snooze);
  gbitmap_destroy(data->icon_x);
  gdraw_command_sequence_destroy(data->draw_commands);
  vector_sequence_layer_destroy(data->animation_layer);
  events_tick_timer_service_unsubscribe(data->tick_handle);
  if (data->name) {
    free(data->name);
  }
  sad_vibe_score_destroy(data->vibes);
  free(data);
  window_destroy(window);
}

static void prv_window_appear(Window *window) {
  AlarmWindowData* data = window_get_user_data(window);
  light_enable_interaction();
  prv_do_vibe(window);
  vector_sequence_layer_play(data->animation_layer);
}

static void prv_window_disappear(Window *window) {
  AlarmWindowData* data = window_get_user_data(window);
  prv_stop_vibe(window);
  vector_sequence_layer_stop(data->animation_layer);
}

static void prv_do_vibe(Window *window) {
  AlarmWindowData* data = window_get_user_data(window);
  // Register a timer to stop vibing after a while.
  data->timer = app_timer_register(600000, prv_timer_callback, window);
  sad_vibe_score_play(data->vibes);
}

static void prv_stop_vibe(Window *window) {
  AlarmWindowData* data = window_get_user_data(window);
  if (data->timer != NULL) {
    app_timer_cancel(data->timer);
    data->timer = NULL;
  }
  sad_vibe_score_stop();
}

static void prv_timer_callback(void* ctx) {
  sad_vibe_score_stop();
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

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_handle_snooze);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_handle_dismiss);
}

static void prv_handle_snooze(ClickRecognizerRef recognizer, void *context) {
  Window *window = context;
  AlarmWindowData* data = window_get_user_data(window);
  int result;
  if (data->is_timer) {
    result = alarm_manager_add_alarm(time(NULL) + 60, true, data->name, false);
  } else {
    result = alarm_manager_add_alarm(time(NULL) + 600, false, data->name, false);
  }
  if (result == S_SUCCESS) {
    const char* text = data->is_timer ? "Snoozed for 1 minute" : "Snoozed for 10 minutes";
    result_window_push("Snoozed", text, gdraw_command_image_create_with_resource(RESOURCE_ID_SLEEPING_PONY), BRANDED_BACKGROUND_COLOUR);
  } else {
    const char* text = data->is_timer ? "Failed to snooze. Timer dismissed." : "Failed to snooze. Alarm dismissed.";
    result_window_push("Failed", text, gdraw_command_image_create_with_resource(RESOURCE_ID_FAILED_PONY), GColorSunsetOrange);
  }
  window_stack_remove(window, false);
}

static void prv_handle_dismiss(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop(true);
}

static uint32_t prv_resource_id_for_setting(VibePatternSetting setting) {
  switch (setting) {
    case VibePatternSettingReveille:
      return RESOURCE_ID_VIBE_REVEILLE;
    case VibePatternSettingJackhammer:
      return RESOURCE_ID_VIBE_JACKHAMMER;
    case VibePatternSettingMario:
      return RESOURCE_ID_VIBE_MARIO;
    case VibePatternSettingStandard:
      return RESOURCE_ID_VIBE_STANDARD;
    case VibePatternSettingNudgeNudge:
      return RESOURCE_ID_VIBE_NUDGE_NUDGE;
  }
  return RESOURCE_ID_VIBE_STANDARD;
}

static SadVibeScore *prv_load_vibe_score(bool is_timer) {
  VibePatternSetting vibe_setting;
  if (is_timer) {
    vibe_setting = settings_get_timer_vibe_pattern();
  } else {
    vibe_setting = settings_get_alarm_vibe_pattern();
  }
  uint32_t resource_id = prv_resource_id_for_setting(vibe_setting);
  return sad_vibe_score_create_with_resource(resource_id);
}