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
#include "../util/time.h"
#include "../util/vector_layer.h"

#include <pebble.h>
#include <pebble-events/pebble-events.h>

typedef struct {
  MenuLayer *menu_layer;
  StatusBarLayer *status_bar;
  TextLayer *empty_text_layer;
  bool for_timers;
  EventHandle tick_handle;
  GDrawCommandImage *sleeping_horse_image;
  VectorLayer *sleeping_horse_layer;
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
static void prv_show_empty(Window *window);

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
  menu_layer_set_highlight_colors(data->menu_layer, SELECTION_HIGHLIGHT_COLOUR, gcolor_legible_over(SELECTION_HIGHLIGHT_COLOUR));
  menu_layer_set_callbacks(data->menu_layer, window, (MenuLayerCallbacks) {
    .get_num_rows = prv_get_num_rows,
    .draw_row = prv_draw_row,
    .select_click = prv_select_click,
  });
  data->sleeping_horse_image = NULL;
  data->sleeping_horse_layer = NULL;
  data->status_bar = status_bar_layer_create();
  data->empty_text_layer = text_layer_create(GRect(10, 20, window_bounds.size.w - 20, 80));
  text_layer_set_text_color(data->empty_text_layer, gcolor_legible_over(ACCENT_COLOUR));
  text_layer_set_background_color(data->empty_text_layer, GColorClear);
  text_layer_set_font(data->empty_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(data->empty_text_layer, GTextAlignmentCenter);
  text_layer_set_text(data->empty_text_layer, data->for_timers ? "No timers set. Ask Bobby to set some." : "No alarms set. Ask Bobby to set some.");
  if (prv_get_num_rows(data->menu_layer, 0, window) == 0) {
    prv_show_empty(window);
  } else {
    window_set_background_color(window, GColorWhite);
    bobby_status_bar_config(data->status_bar);
    layer_add_child(root_layer, menu_layer_get_layer(data->menu_layer));
    menu_layer_set_click_config_onto_window(data->menu_layer, window);
  }
  layer_add_child(root_layer, (Layer *)data->status_bar);
}

static void prv_show_empty(Window *window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  Layer* root_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_frame(root_layer);

  // Remove the menu, if it's present.
  layer_remove_from_parent(menu_layer_get_layer(data->menu_layer));
  window_set_click_config_provider(window, NULL);

  data->sleeping_horse_image = gdraw_command_image_create_with_resource(RESOURCE_ID_SLEEPING_PONY);
  data->sleeping_horse_layer = vector_layer_create(GRect(window_bounds.size.w / 2 - 25, window_bounds.size.h - 55, 50, 50));
  vector_layer_set_vector(data->sleeping_horse_layer, data->sleeping_horse_image);
  window_set_background_color(window, BRANDED_BACKGROUND_COLOUR);
  bobby_status_bar_result_pane_config(data->status_bar);
  layer_add_child(root_layer, text_layer_get_layer(data->empty_text_layer));
  layer_add_child(root_layer, vector_layer_get_layer(data->sleeping_horse_layer));

}

static void prv_window_unload(Window* window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  menu_layer_destroy(data->menu_layer);
  status_bar_layer_destroy(data->status_bar);
  text_layer_destroy(data->empty_text_layer);
  if (data->sleeping_horse_layer != NULL) {
    vector_layer_destroy(data->sleeping_horse_layer);
  }
  if (data->sleeping_horse_image != NULL) {
    gdraw_command_image_destroy(data->sleeping_horse_image);
  }
  window_destroy(window);
  free(data);
}

static void prv_window_appear(Window* window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  if (!data->for_timers) {
    return;
  }
  data->tick_handle = events_tick_timer_service_subscribe_context(SECOND_UNIT, prv_tick_handler, window);
  // A potential reason for us disappearing and reappearing is an alarm going off, in which case our old data will no
  // longer make any sense.
  if (alarm_manager_get_alarm_count() == 0) {
    prv_show_empty(window);
  } else {
    menu_layer_reload_data(data->menu_layer);
  }
}

static void prv_window_disappear(Window* window) {
  AlarmMenuWindowData* data = window_get_user_data(window);
  if (!data->tick_handle) {
    return;
  }
  events_tick_timer_service_unsubscribe(data->tick_handle);
}

static uint16_t prv_get_num_rows(struct MenuLayer* menu_layer, uint16_t section_index, void* context) {
  AlarmMenuWindowData *data = window_get_user_data(context);
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
  AlarmMenuWindowData *data = window_get_user_data(context);
  int alarm_count = alarm_manager_get_alarm_count();
  int relevant_count = 0;
  for (int i = 0; i < alarm_count; ++i) {
    if (alarm_is_timer(alarm_manager_get_alarm(i)) == data->for_timers) {
      if (relevant_count == cell_index->row) {
        Alarm* alarm = alarm_manager_get_alarm(i);
        time_t t = alarm_get_time(alarm);
        time_t now = time(NULL);
        if (data->for_timers) {
          char name[10];
          time_t remaining = t - now;
          int hours = remaining / 3600;
          int minutes = (remaining / 60) % 60;
          int seconds = remaining % 60;
          snprintf(name, sizeof(name), "%d:%02d:%02d", hours, minutes, seconds);
          menu_cell_basic_draw(ctx, cell_layer, name, alarm_get_name(alarm), NULL);
        } else {
          char* alarm_name = alarm_get_name(alarm);
          if (!alarm_name) {
            char title[10];
            char subtitle[20];
            struct tm time_struct = *localtime(&t);
            format_time_ampm(title, sizeof(title), &time_struct);
            time_t midnight = time_start_of_today();
            if (t < midnight + 86400) {
              snprintf(subtitle, sizeof(subtitle), "Today");
            } else if (t < midnight + 86400 * 2) {
              snprintf(subtitle, sizeof(subtitle), "Tomorrow");
            } else {
              strftime(subtitle, sizeof(subtitle), "%a, %b %d", &time_struct);
            }
            menu_cell_basic_draw(ctx, cell_layer, title, subtitle, NULL);
          } else {
            char subtitle[40];
            format_datetime(subtitle, sizeof(subtitle), alarm_get_time(alarm));
            menu_cell_basic_draw(ctx, cell_layer, alarm_name, subtitle, NULL);
          }
        }
        return;
      }
      ++relevant_count;
    }
  }
}

static void prv_select_click(struct MenuLayer* menu_layer, MenuIndex* cell_index, void* context) {
  ActionMenuLevel *root_level = action_menu_level_create(1);
  action_menu_level_add_action(root_level, "Delete", prv_cancel_alarm, NULL);
  ActionMenuConfig config = (ActionMenuConfig) {
    .root_level = root_level,
    .colors = {
      .background = COLOR_FALLBACK(ACCENT_COLOUR, GColorWhite),
      .foreground = COLOR_FALLBACK(gcolor_legible_over(ACCENT_COLOUR), GColorBlack),
    },
    .align = ActionMenuAlignCenter,
    .did_close = prv_action_menu_close,
    .context = context,
  };
  action_menu_open(&config);
}

static void prv_tick_handler(struct tm* tick_time, TimeUnits units_changed, void* context) {
  AlarmMenuWindowData *data = window_get_user_data(context);
  if (!data->for_timers) {
    return;
  }
  menu_layer_reload_data(data->menu_layer);
}

static void prv_cancel_alarm(ActionMenu* layer, const ActionMenuItem* item, void* context) {
  AlarmMenuWindowData *data = window_get_user_data(context);
  int alarm_count = alarm_manager_get_alarm_count();
  int relevant_count = 0;
  for (int i = 0; i < alarm_count; ++i) {
    if (alarm_is_timer(alarm_manager_get_alarm(i)) == data->for_timers) {
      if (relevant_count == menu_layer_get_selected_index(data->menu_layer).row) {
        Alarm* alarm = alarm_manager_get_alarm(i);
        alarm_manager_cancel_alarm(alarm_get_time(alarm), alarm_is_timer(alarm));
        if (alarm_manager_get_alarm_count() == 0) {
          prv_show_empty(context);
        } else {
          menu_layer_reload_data(data->menu_layer);
        }
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
