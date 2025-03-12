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

#include <pebble.h>
#include <pebble-events/pebble-events.h>

#include "root_window.h"
#include "talking_horse_layer.h"
#include "converse/session_window.h"
#include "menus/root_menu.h"
#include "util/style.h"
#include "version/version.h"

struct RootWindow {
  Window* window;
  ActionBarLayer* action_bar;
  SessionWindow* session_window;
  GBitmap* dictation_icon;
  GBitmap* more_icon;
  TextLayer* time_layer;
  TextLayer* version_layer;
  TalkingHorseLayer* talking_horse_layer;
  EventHandle event_handle;
  char time_string[6];
  char version_string[9];
};

static void prv_window_load(Window* window);
static void prv_window_appear(Window* window);
static void prv_window_disappear(Window* window);
static void prv_click_config_provider(void *context);
static void prv_prompt_clicked(ClickRecognizerRef recognizer, void *context);
static void prv_more_clicked(ClickRecognizerRef recognizer, void* context);
static void prv_up_clicked(ClickRecognizerRef recognizer, void *context);
static void prv_time_changed(struct tm *tick_time, TimeUnits time_changed, void *context);

RootWindow* root_window_create() {
  RootWindow* window = malloc(sizeof(RootWindow));
  window->window = window_create();
  window_set_window_handlers(window->window, (WindowHandlers) {
    .load = prv_window_load,
    .appear = prv_window_appear,
    .disappear = prv_window_disappear,
  });
  GRect bounds = layer_get_bounds(window_get_root_layer(window->window));
  window_set_background_color(window->window, COLOR_FALLBACK(ACCENT_COLOUR, GColorWhite));
  window->dictation_icon = gbitmap_create_with_resource(RESOURCE_ID_DICTATION_ICON);
  window->more_icon = gbitmap_create_with_resource(RESOURCE_ID_MORE_ICON);
  window->action_bar = action_bar_layer_create();
  action_bar_layer_set_context(window->action_bar, window);
  action_bar_layer_set_icon(window->action_bar, BUTTON_ID_SELECT, window->dictation_icon);
  action_bar_layer_set_icon(window->action_bar, BUTTON_ID_DOWN, window->more_icon);
  window_set_user_data(window->window, window);
  window->time_layer = text_layer_create(GRect(0, 5, 144 - ACTION_BAR_WIDTH, 40));
  window->event_handle = NULL;
  text_layer_set_text_alignment(window->time_layer, GTextAlignmentCenter);
  text_layer_set_font(window->time_layer, fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS));
  text_layer_set_text(window->time_layer, "12:34");
  text_layer_set_background_color(window->time_layer, GColorClear);
  layer_add_child(window_get_root_layer(window->window), (Layer *)window->time_layer);
  window->talking_horse_layer = talking_horse_layer_create(GRect(0, 56, 144 - ACTION_BAR_WIDTH, 112));
  layer_add_child(window_get_root_layer(window->window), (Layer *)window->talking_horse_layer);

  VersionInfo version_info = version_get_current();
  snprintf(window->version_string, sizeof(window->version_string), "v%d.%d", version_info.major, version_info.minor);
  window->version_string[sizeof(window->version_string) - 1] = '\0';
  window->version_layer = text_layer_create(GRect(0, bounds.size.h - 18, bounds.size.w - ACTION_BAR_WIDTH - 4, 18));
  text_layer_set_font(window->version_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(window->version_layer, GTextAlignmentRight);
  text_layer_set_background_color(window->version_layer, GColorClear);
  text_layer_set_text(window->version_layer, window->version_string);
  layer_add_child(window_get_root_layer(window->window), (Layer *)window->version_layer);

  return window;
}

void root_window_push(RootWindow* window) {
  window_stack_push(window->window, true);
}

void root_window_destroy(RootWindow* window) {
  window_destroy(window->window);
  action_bar_layer_destroy(window->action_bar);
  gbitmap_destroy(window->dictation_icon);
  gbitmap_destroy(window->more_icon);
  text_layer_destroy(window->time_layer);
  text_layer_destroy(window->version_layer);
  talking_horse_layer_destroy(window->talking_horse_layer);
  free(window);
}

Window* root_window_get_window(RootWindow* window) {
  return window->window;
}

static void prv_window_load(Window *window) {
  RootWindow* root_window = (RootWindow*)window_get_user_data(window);
  action_bar_layer_add_to_window(root_window->action_bar, window);
  action_bar_layer_set_click_config_provider(root_window->action_bar, prv_click_config_provider);
}

static void prv_window_appear(Window* window) {
  RootWindow* rw = window_get_user_data(window);
  if (!rw->event_handle) {
    rw->event_handle = events_tick_timer_service_subscribe_context(MINUTE_UNIT, prv_time_changed, rw);
    time_t now = time(NULL);
    prv_time_changed(localtime(&now), MINUTE_UNIT, rw);
  }
}

static void prv_window_disappear(Window* window) {
  RootWindow* rw = window_get_user_data(window);
  if (rw->event_handle) {
    events_tick_timer_service_unsubscribe(rw->event_handle);
    rw->event_handle = NULL;
  }
}


static void prv_time_changed(struct tm *tick_time, TimeUnits time_changed, void *context) {
  RootWindow* rw = context;
  if (clock_is_24h_style()) {
    strftime(rw->time_string, 6, "%H:%M", tick_time);
  } else if (tick_time->tm_hour > 0 && tick_time->tm_hour < 10) {
    snprintf(rw->time_string, 2, "%d", tick_time->tm_hour);
    strftime(rw->time_string + 1, 5, ":%M", tick_time);
  } else if (tick_time->tm_hour > 12 && tick_time->tm_hour < 22) {
    snprintf(rw->time_string, 2, "%d", tick_time->tm_hour - 12);
    strftime(rw->time_string + 1, 5, ":%M", tick_time);
  } else {
    strftime(rw->time_string, 6, "%I:%M", tick_time);
  }
  if (tick_time->tm_hour >= 6 && tick_time->tm_hour < 12) {
    talking_horse_layer_set_text(rw->talking_horse_layer, "Good morning!");
  } else if (tick_time->tm_hour >= 12 && tick_time->tm_hour < 18) {
    talking_horse_layer_set_text(rw->talking_horse_layer, "Good afternoon!");
  } else if (tick_time->tm_hour >= 18 && tick_time->tm_hour < 22) {
    talking_horse_layer_set_text(rw->talking_horse_layer, "Good evening!");
  } else {
    talking_horse_layer_set_text(rw->talking_horse_layer, "Hey there, night owl!");
  }
  text_layer_set_text(rw->time_layer, rw->time_string);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_clicked);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_prompt_clicked);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_more_clicked);
}

static void prv_up_clicked(ClickRecognizerRef recognizer, void *context) {
  RootWindow* rw = context;
  talking_horse_layer_set_text(rw->talking_horse_layer, "I'm doing thanks! How help?");
}

static void prv_prompt_clicked(ClickRecognizerRef recognizer, void *context) {
  session_window_push();
}

static void prv_more_clicked(ClickRecognizerRef recognizer, void* context) {
  root_menu_window_push();
}
