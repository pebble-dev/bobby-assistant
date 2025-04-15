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
#include "util/logging.h"
#include "util/style.h"
#include "util/time.h"
#include "util/memory/malloc.h"
#include "util/memory/sdk.h"
#include "version/version.h"
#include "vibes/haptic_feedback.h"

struct RootWindow {
  Window* window;
  ActionBarLayer* action_bar;
  SessionWindow* session_window;
  GBitmap* question_icon;
  GBitmap* dictation_icon;
  GBitmap* more_icon;
  TextLayer* time_layer;
  TextLayer* version_layer;
  TalkingHorseLayer* talking_horse_layer;
  EventHandle event_handle;
  EventHandle app_message_handle;
  char time_string[6];
  char version_string[9];
  char** sample_prompts;
  bool talking_horse_overridden;
};

static void prv_window_load(Window* window);
static void prv_window_appear(Window* window);
static void prv_window_disappear(Window* window);
static void prv_click_config_provider(void *context);
static void prv_prompt_clicked(ClickRecognizerRef recognizer, void *context);
static void prv_more_clicked(ClickRecognizerRef recognizer, void* context);
static void prv_up_clicked(ClickRecognizerRef recognizer, void *context);
static void prv_time_changed(struct tm *tick_time, TimeUnits time_changed, void *context);
static int prv_load_suggestions(char*** suggestions);
static void prv_action_menu_closed(ActionMenu *action_menu, const ActionMenuItem *performed_action, void *context);
static void prv_suggestion_clicked(ActionMenu *action_menu, const ActionMenuItem *action, void *context);
static void prv_app_message_handler(DictionaryIterator *iter, void *context);

RootWindow* root_window_create() {
  RootWindow* rw = bmalloc(sizeof(RootWindow));
  memset(rw, 0, sizeof(RootWindow));
  rw->window = bwindow_create();
  window_set_window_handlers(rw->window, (WindowHandlers) {
    .load = prv_window_load,
    .appear = prv_window_appear,
    .disappear = prv_window_disappear,
  });
  window_set_user_data(rw->window, rw);
  return rw;
}

void root_window_push(RootWindow* window) {
  window_stack_push(window->window, true);
}

void root_window_destroy(RootWindow* window) {
  window_destroy(window->window);
  free(window);
}

Window* root_window_get_window(RootWindow* window) {
  return window->window;
}

static void prv_window_load(Window *window) {
  // RootWindow* root_window = (RootWindow*)window_get_user_data(window);
}

static void prv_window_appear(Window* window) {
  size_t heap_size = heap_bytes_free();
  RootWindow* rw = window_get_user_data(window);
  GRect bounds = layer_get_bounds(window_get_root_layer(rw->window));
  window_set_background_color(rw->window, COLOR_FALLBACK(ACCENT_COLOUR, GColorWhite));
  rw->question_icon = bgbitmap_create_with_resource(RESOURCE_ID_QUESTION_ICON);
  rw->dictation_icon = bgbitmap_create_with_resource(RESOURCE_ID_DICTATION_ICON);
  rw->more_icon = bgbitmap_create_with_resource(RESOURCE_ID_MORE_ICON);
  rw->action_bar = baction_bar_layer_create();
  action_bar_layer_set_context(rw->action_bar, rw);
  action_bar_layer_set_icon(rw->action_bar, BUTTON_ID_UP, rw->question_icon);
  action_bar_layer_set_icon(rw->action_bar, BUTTON_ID_SELECT, rw->dictation_icon);
  action_bar_layer_set_icon(rw->action_bar, BUTTON_ID_DOWN, rw->more_icon);
  action_bar_layer_add_to_window(rw->action_bar, window);
  action_bar_layer_set_click_config_provider(rw->action_bar, prv_click_config_provider);
  rw->time_layer = btext_layer_create(GRect(0, 5, 144 - ACTION_BAR_WIDTH, 40));
  text_layer_set_text_alignment(rw->time_layer, GTextAlignmentCenter);
  text_layer_set_font(rw->time_layer, fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS));
  text_layer_set_text(rw->time_layer, "12:34");
  text_layer_set_background_color(rw->time_layer, GColorClear);
  layer_add_child(window_get_root_layer(rw->window), (Layer *)rw->time_layer);
  rw->talking_horse_layer = talking_horse_layer_create(GRect(0, 56, 144 - ACTION_BAR_WIDTH, 112));
  layer_add_child(window_get_root_layer(rw->window), (Layer *)rw->talking_horse_layer);
  rw->talking_horse_overridden = false;
  if (version_is_updated() || rand() < RAND_MAX / 10) {
    rw->talking_horse_overridden = true;
    talking_horse_layer_set_text(rw->talking_horse_layer, "Try holding select in chat!");
  }

  VersionInfo version_info = version_get_current();
  snprintf(rw->version_string, sizeof(rw->version_string), "v%d.%d", version_info.major, version_info.minor);
  rw->version_string[sizeof(rw->version_string) - 1] = '\0';
  rw->version_layer = btext_layer_create(GRect(0, bounds.size.h - 18, bounds.size.w - ACTION_BAR_WIDTH - 4, 18));
  text_layer_set_font(rw->version_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(rw->version_layer, GTextAlignmentRight);
  text_layer_set_background_color(rw->version_layer, GColorClear);
  text_layer_set_text(rw->version_layer, rw->version_string);
  layer_add_child(window_get_root_layer(rw->window), (Layer *)rw->version_layer);


  if (!rw->event_handle) {
    rw->event_handle = events_tick_timer_service_subscribe_context(MINUTE_UNIT, prv_time_changed, rw);
    time_t now = time(NULL);
    prv_time_changed(localtime(&now), MINUTE_UNIT, rw);
  }
  if (!rw->app_message_handle) {
    rw->app_message_handle = events_app_message_register_inbox_received(prv_app_message_handler, rw);
  }
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Window appeared. Heap usage increased %d bytes", heap_size - heap_bytes_free());
}

static void prv_window_disappear(Window* window) {
  size_t heap_size = heap_bytes_free();
  RootWindow* rw = window_get_user_data(window);
  if (rw->event_handle) {
    events_tick_timer_service_unsubscribe(rw->event_handle);
    rw->event_handle = NULL;
  }
  if (rw->app_message_handle) {
    events_app_message_unsubscribe(rw->app_message_handle);
  }
  action_bar_layer_destroy(rw->action_bar);
  gbitmap_destroy(rw->question_icon);
  gbitmap_destroy(rw->dictation_icon);
  gbitmap_destroy(rw->more_icon);
  text_layer_destroy(rw->time_layer);
  text_layer_destroy(rw->version_layer);
  talking_horse_layer_destroy(rw->talking_horse_layer);
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Window disappeared. Heap usage decreased %d bytes", heap_bytes_free() - heap_size);
}

static void prv_app_message_handler(DictionaryIterator *iter, void *context) {
  RootWindow* rw = context;
  Tuple *tuple = dict_find(iter, MESSAGE_KEY_COBBLE_WARNING);
  if (!tuple) {
    return;
  }
  if (tuple->value->int32 == 1) {
    rw->talking_horse_overridden = true;
    talking_horse_layer_set_text(rw->talking_horse_layer, "Cobble has many Bobby bugs.");
    window_set_background_color(rw->window, COLOR_FALLBACK(GColorRed, GColorDarkGray));
    vibe_haptic_feedback();
  }
}

static void prv_time_changed(struct tm *tick_time, TimeUnits time_changed, void *context) {
  RootWindow* rw = context;
  format_time(rw->time_string, sizeof(rw->time_string), tick_time);
  text_layer_set_text(rw->time_layer, rw->time_string);
  if (rw->talking_horse_overridden) {
    return;
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
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_clicked);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_prompt_clicked);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_more_clicked);
}

static void prv_up_clicked(ClickRecognizerRef recognizer, void *context) {
  RootWindow* rw = context;
  // talking_horse_layer_set_text(rw->talking_horse_layer, "I'm doing thanks! How help?");
  char **suggestions;
  int count = prv_load_suggestions(&suggestions);
  ActionMenuLevel *level = baction_menu_level_create(count);
  for (int i = 0; i < count; ++i) {
    action_menu_level_add_action(level, suggestions[i], prv_suggestion_clicked, rw);
  }
  ActionMenuConfig config = (ActionMenuConfig) {
    .root_level = level,
    .colors = {
      .background = BRANDED_BACKGROUND_COLOUR,
      .foreground = gcolor_legible_over(BRANDED_BACKGROUND_COLOUR),
    },
    .align = ActionMenuAlignTop,
    .context = rw,
    .did_close = prv_action_menu_closed,
  };
  rw->sample_prompts = suggestions;
  action_menu_open(&config);
}

static void prv_action_menu_closed(ActionMenu *action_menu, const ActionMenuItem *performed_action, void *context) {
  RootWindow* rw = context;
  action_menu_hierarchy_destroy(action_menu_get_root_level(action_menu), NULL, NULL);
  // memory is allocated for sample_prompts[0], but not the rest of the entries - so just free the first one.
  free(rw->sample_prompts[0]);
  free(rw->sample_prompts);
}

static void prv_suggestion_clicked(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
  RootWindow* rw = context;
  char* suggestion = action_menu_item_get_label(action);
  session_window_push(0, suggestion);
}

static void prv_prompt_clicked(ClickRecognizerRef recognizer, void *context) {
  session_window_push(0, NULL);
}

static void prv_more_clicked(ClickRecognizerRef recognizer, void* context) {
  root_menu_window_push();
}

static int prv_load_suggestions(char*** suggestions) {
  ResHandle handle = resource_get_handle(RESOURCE_ID_SAMPLE_PROMPTS);
  size_t size = resource_size(handle);
  char* buffer = bmalloc(size);
  resource_load(handle, (uint8_t*)buffer, size);
  int count = 1;
  for (size_t i = 0; i < size; ++i) {
    if (buffer[i] == '\n') {
      ++count;
    }
  }
  *suggestions = bmalloc(sizeof(char*) * count);
  for (int i = 0; i < count; ++i) {
    (*suggestions)[i] = buffer;
    buffer = strchr(buffer, '\n');
    if (buffer) {
      *buffer = '\0';
      ++buffer;
    }
  }
  return count;
}
