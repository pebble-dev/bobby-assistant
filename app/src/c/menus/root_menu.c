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

#include "root_menu.h"
#include "quota_window.h"
#include "alarm_menu.h"
#include "about_window.h"
#include "legal_window.h"
#include "reminders_menu.h"
#include "feedback_window.h"
#include "../util/style.h"
#include <pebble.h>


static void prv_window_load(Window* window);
static void prv_window_unload(Window* window);
static void prv_push_quota_screen(int index, void* context);
static void prv_push_alarm_screen(int index, void* context);
static void prv_push_timer_screen(int index, void* context);
static void prv_push_about_screen(int index, void* context);
static void prv_push_legal_screen(int index, void* context);
static void prv_push_reminders_screen(int index, void* context);
static void prv_push_feedback_screen(int index, void* context);

static SimpleMenuSection s_menu_section = {
  .num_items = 0,
};
static SimpleMenuItem s_menu_items[7];

typedef struct {
  SimpleMenuLayer *menu_layer;
  StatusBarLayer *status_bar;
} RootMenuWindowData;

void root_menu_window_push() {
  Window* window = window_create();
  RootMenuWindowData* data = malloc(sizeof(RootMenuWindowData));
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  window_stack_push(window, true);
}

static void prv_window_load(Window* window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Loading root menu window...");
  // This setup has to be done separately because otherwise the initializer isn't constant.
  if (s_menu_section.num_items == 0) {
    s_menu_items[0] = (SimpleMenuItem) {
      .title = "Alarms",
      .callback = prv_push_alarm_screen,
      .icon = gbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_ALARMS),
    };
    s_menu_items[1] = (SimpleMenuItem) {
      .title = "Timers",
      .callback = prv_push_timer_screen,
      .icon = gbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_TIMERS),
    };
    s_menu_items[2] = (SimpleMenuItem) {
      .title = "Reminders",
      .callback = prv_push_reminders_screen,
      .icon = gbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_REMINDERS),
    };
    s_menu_items[3] = (SimpleMenuItem) {
      .title = "Quota",
      .callback = prv_push_quota_screen,
      .icon = gbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_QUOTA),
    };
    s_menu_items[4] = (SimpleMenuItem) {
      .title = "Feedback",
      .callback = prv_push_feedback_screen,
      .icon = gbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_FEEDBACK),
    };
    s_menu_items[5] = (SimpleMenuItem) {
      .title = "About",
      .callback = prv_push_about_screen,
      .icon = gbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_ABOUT),
    };
    s_menu_items[6] = (SimpleMenuItem) {
      .title = "Legal",
      .callback = prv_push_legal_screen,
      .icon = gbitmap_create_with_resource(RESOURCE_ID_MENU_ICON_LEGAL),
    };
    s_menu_section.num_items = 7;
    s_menu_section.items = s_menu_items;
  }

  RootMenuWindowData* data = window_get_user_data(window);
  Layer* root_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_frame(root_layer);
  data->status_bar = status_bar_layer_create();
  bobby_status_bar_config(data->status_bar);
  layer_add_child(root_layer, status_bar_layer_get_layer(data->status_bar));
  data->menu_layer = simple_menu_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, window_bounds.size.w, window_bounds.size.h - STATUS_BAR_LAYER_HEIGHT), window, &s_menu_section, 1, window);
  menu_layer_set_highlight_colors(simple_menu_layer_get_menu_layer(data->menu_layer), SELECTION_HIGHLIGHT_COLOUR, gcolor_legible_over(SELECTION_HIGHLIGHT_COLOUR));
  layer_add_child(root_layer, simple_menu_layer_get_layer(data->menu_layer));
  APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "Root menu window loaded");
}

static void prv_window_unload(Window* window) {
  RootMenuWindowData* data = window_get_user_data(window);
  simple_menu_layer_destroy(data->menu_layer);
  status_bar_layer_destroy(data->status_bar);
  for (uint32_t i = 0; i < s_menu_section.num_items; i++) {
    if (s_menu_items[i].icon) {
      gbitmap_destroy(s_menu_items[i].icon);
      s_menu_items[i].icon = NULL;
    }
  }
  s_menu_section.num_items = 0;
  free(data);
  window_destroy(window);
}

static void prv_push_quota_screen(int index, void* context) {
  push_quota_window();
}

static void prv_push_alarm_screen(int index, void* context) {
  alarm_menu_window_push(false);
}

static void prv_push_timer_screen(int index, void* context) {
  alarm_menu_window_push(true);
}

static void prv_push_legal_screen(int index, void* context) {
  legal_window_push();
}

static void prv_push_reminders_screen(int index, void* context) {
  reminders_menu_push();
}

static void prv_push_feedback_screen(int index, void* context) {
  feedback_window_push();
}

static void prv_push_about_screen(int index, void* context) {
  about_window_push();
}
