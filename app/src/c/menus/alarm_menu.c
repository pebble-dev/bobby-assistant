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

#include "alarm_menu.h"
#include "../alarms/manager.h"
#include "../util/style.h"

#include <pebble.h>
#include <pebble-events/pebble-events.h>

typedef struct {
  MenuLayer *menu_layer;
  StatusBarLayer *status_bar;
  TextLayer *empty_text_layer;
  bool for_timers;
  EventHandle tick_handle;
} AlarmMenuWindowData;

static void prv_window_load(Window* window);
static void prv_window_unload(Window* window);
static void prv_window_appear(Window* window);
static void prv_window_disappear(Window* window);
static uint16_t prv_get_num_rows(struct MenuLayer* menu_layer, uint16_t section_index, void* context);
static void prv_draw_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index, void* context);
static void prv_select_click(struct MenuLayer* menu_layer, MenuIndex* cell_index, void* context);
static void prv_tick_handler(struct tm* tick_time, TimeUnits units_changed, void* context);
static void prv_cancel_alarm(ActionMenu* layer, const ActionMenuItem* item, void* context);
static void prv_action_menu_close(ActionMenu* action_menu, const ActionMenuItem* item, void* context);

void alarm_menu_window_push(bool for_timers) {
  AlarmMenuWindowData *data = malloc(sizeof(AlarmMenuWindowData));
  memset(data, 0, sizeof(*data));
  data->for_timers = for_timers;

  Window *window = window_create();
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
    .appear = prv_window_appear,
    .disappear = prv_window_disappear,
  });
  window_stack_push(window, true);
}

static void prv_window_load(Window* window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  Layer* root_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_frame(root_layer);
  data->menu_layer = menu_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, window_bounds.size.w, window_bounds.size.h - STATUS_BAR_LAYER_HEIGHT));
  menu_layer_set_callbacks(data->menu_layer, data, (MenuLayerCallbacks) {
    .get_num_rows = prv_get_num_rows,
    .draw_row = prv_draw_row,
    .select_click = prv_select_click,
  });
  data->status_bar = status_bar_layer_create();
  bobby_status_bar_config(data->status_bar);
  data->empty_text_layer = text_layer_create(GRect(0, 50, window_bounds.size.w, window_bounds.size.h));
  text_layer_set_font(data->empty_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(data->empty_text_layer, GTextAlignmentCenter);
  text_layer_set_text(data->empty_text_layer, data->for_timers ? "No timers set. Ask Bobby to set some." : "No alarms set. Ask Bobby to set some.");
  if (prv_get_num_rows(data->menu_layer, 0, data) == 0) {
    layer_add_child(root_layer, text_layer_get_layer(data->empty_text_layer));
  } else {
    layer_add_child(root_layer, menu_layer_get_layer(data->menu_layer));
    menu_layer_set_click_config_onto_window(data->menu_layer, window);
  }
  layer_add_child(root_layer, (Layer *)data->status_bar);
}

static void prv_window_unload(Window* window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  menu_layer_destroy(data->menu_layer);
  status_bar_layer_destroy(data->status_bar);
  free(data);
}

static void prv_window_appear(Window* window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  if (!data->for_timers) {
    return;
  }
  data->tick_handle = events_tick_timer_service_subscribe_context(SECOND_UNIT, prv_tick_handler, data);
  // A potential reason for us disappearing and reappearing is an alarm going off, in which case our old data will no
  // longer make any sense.
  menu_layer_reload_data(data->menu_layer);
}

static void prv_window_disappear(Window* window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  if (!data->tick_handle) {
    return;
  }
  events_tick_timer_service_unsubscribe(data->tick_handle);
}

static uint16_t prv_get_num_rows(struct MenuLayer* menu_layer, uint16_t section_index, void* context) {
  AlarmMenuWindowData *data = context;
  int alarm_count = alarm_manager_get_alarm_count();
  int relevant_count = 0;
  for (int i = 0; i < alarm_count; ++i) {
    if (alarm_is_timer(alarm_manager_get_alarm(i)) == data->for_timers) {
      ++relevant_count;
    }
  }
  return relevant_count;
}


static void prv_draw_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index, void* context) {
  AlarmMenuWindowData *data = context;
  int alarm_count = alarm_manager_get_alarm_count();
  int relevant_count = 0;
  for (int i = 0; i < alarm_count; ++i) {
    if (alarm_is_timer(alarm_manager_get_alarm(i)) == data->for_timers) {
      if (relevant_count == cell_index->row) {
        char name[10];
        time_t t = alarm_get_time(alarm_manager_get_alarm(i));
        time_t now = time(NULL);
        if (data->for_timers) {
          time_t remaining = t - now;
          int hours = remaining / 3600;
          int minutes = (remaining / 60) % 60;
          int seconds = remaining % 60;
          snprintf(name, sizeof(name), "%d:%02d:%02d", hours, minutes, seconds);
          menu_cell_basic_draw(ctx, cell_layer, name, NULL, NULL);
        } else {
          char subtitle[20];
          struct tm time_struct = *localtime(&t);
          if (clock_is_24h_style()) {
            strftime(name, sizeof(name), "%H:%M", &time_struct);
          } else {
            // I disagree with the leading zeros, so we end up doing this manually...
            int hour = time_struct.tm_hour % 12;
            if (hour == 0) {
              hour = 12;
            }
            snprintf(name, sizeof(name), "%d:%02d %s", hour, time_struct.tm_min, time_struct.tm_hour < 12 ? "AM" : "PM");
          }
          struct tm* now_struct = localtime(&now);
          now_struct->tm_hour = 0;
          now_struct->tm_min = 0;
          now_struct->tm_sec = 0;
          time_t midnight = mktime(now_struct);
          if (t < midnight + 86400) {
            snprintf(subtitle, sizeof(subtitle), "Today");
          } else if (t < midnight + 86400 * 2) {
            snprintf(subtitle, sizeof(subtitle), "Tomorrow");
          } else {
            strftime(subtitle, sizeof(subtitle), "%a, %b %d", &time_struct);
          }
          menu_cell_basic_draw(ctx, cell_layer, name, subtitle, NULL);
        }
        return;
      }
      ++relevant_count;
    }
  }
}

static void prv_select_click(struct MenuLayer* menu_layer, MenuIndex* cell_index, void* context) {
  AlarmMenuWindowData *data = context;
  ActionMenuLevel *root_level = action_menu_level_create(1);
  action_menu_level_add_action(root_level, "Delete", prv_cancel_alarm, NULL);
  ActionMenuConfig config = (ActionMenuConfig) {
    .root_level = root_level,
    .colors = {
      .background = GColorBlack, // TODO: something nicer on colour screens, probably
      .foreground = GColorWhite,
    },
    .align = ActionMenuAlignCenter,
    .did_close = prv_action_menu_close,
    .context = data,
  };
  action_menu_open(&config);
}

static void prv_tick_handler(struct tm* tick_time, TimeUnits units_changed, void* context) {
  AlarmMenuWindowData *data = context;
  if (!data->for_timers) {
    return;
  }
  menu_layer_reload_data(data->menu_layer);
}

static void prv_cancel_alarm(ActionMenu* layer, const ActionMenuItem* item, void* context) {
  AlarmMenuWindowData *data = context;
  int alarm_count = alarm_manager_get_alarm_count();
  int relevant_count = 0;
  for (int i = 0; i < alarm_count; ++i) {
    if (alarm_is_timer(alarm_manager_get_alarm(i)) == data->for_timers) {
      if (relevant_count == menu_layer_get_selected_index(data->menu_layer).row) {
        Alarm* alarm = alarm_manager_get_alarm(i);
        alarm_manager_cancel_alarm(alarm_get_time(alarm), alarm_is_timer(alarm));
        menu_layer_reload_data(data->menu_layer);
        action_menu_close(layer, true);
        return;
      }
      ++relevant_count;
    }
  }
}

static void prv_action_menu_close(ActionMenu* action_menu, const ActionMenuItem* item, void* context) {
  action_menu_hierarchy_destroy(action_menu_get_root_level(action_menu), NULL, NULL);
}
