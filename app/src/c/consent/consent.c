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

#include "consent.h"
#include "../util/persist_keys.h"
#include "../util/style.h"
#include "../version/version.h"
#include "../root_window.h"

#include <pebble.h>
#include <pebble-events/pebble-events.h>


#define STAGE_LLM_WARNING 0
#define STAGE_GEMINI_CONSENT 1
#define STAGE_LOCATION_CONSENT 2

typedef struct {
  ScrollLayer *scroll_layer;
  TextLayer *title_layer;
  TextLayer *text_layer;
  Layer *content_indicator_layer;
  char* current_text;
  const char* title_text;
  GBitmap* select_indicator_bitmap;
  BitmapLayer* select_indicator_layer;
  ActionMenu* action_menu;
  int stage;
  int expected_app_response;
  EventHandle app_message_handle;
} ConsentWindowData;

static void prv_set_stage(Window* window, int stage);
static void prv_window_load(Window *window);
static void prv_window_unload(Window *window);
static void prv_window_appear(Window *window);
static void prv_click_config_provider(void *context);
static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context);
static bool prv_did_scroll_to_bottom(Window* window);
static void prv_present_consent_menu(Window* window);
static void prv_consent_menu_select_callback(ActionMenu *action_menu, const ActionMenuItem *action, void *context);
static void prv_action_menu_close(ActionMenu* action_menu, const ActionMenuItem* item, void* context);
static void prv_app_message_handler(DictionaryIterator *iter, void *context);
static void prv_mark_consents_complete();

void consent_window_push() {
  Window* window = window_create();
  ConsentWindowData* data = malloc(sizeof(ConsentWindowData));
  memset(data, 0, sizeof(ConsentWindowData));
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
    .appear = prv_window_appear,
  });
  window_stack_push(window, true);
}

bool must_present_consent() {
  return persist_read_int(PERSIST_KEY_CONSENTS_COMPLETED) < 1;
}

void consent_migrate() {
  if (version_is_updated() && !version_is_first_launch()) {
    // If we're updating from version 1.1 or older, consent agreement was implied by LOCATION_ENABLED being set
    // (either true or false).
    if (version_info_compare(version_get_last_launch(), (VersionInfo) {1, 1}) <= 0) {
      APP_LOG(APP_LOG_LEVEL_INFO, "Performing consent migration from version 1.1.");
      // If the location enabled state is set, that's equivalent to consent agreement version 1.
      if (persist_exists(PERSIST_KEY_LOCATION_ENABLED)) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Marking consent as 1.");;
        persist_write_int(PERSIST_KEY_CONSENTS_COMPLETED, 1);
      } else {
        APP_LOG(APP_LOG_LEVEL_INFO, "Not marking consent.");;
      }
    }
  }
}

static void prv_window_load(Window *window) {
  ConsentWindowData *data = window_get_user_data(window);
  Layer *root_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_frame(root_layer);
  data->scroll_layer = scroll_layer_create(window_bounds);
  scroll_layer_set_click_config_onto_window(data->scroll_layer, window);
  data->title_layer = text_layer_create(GRect(0, 0, window_bounds.size.w, 30));
  text_layer_set_text_alignment(data->title_layer, GTextAlignmentCenter);
  text_layer_set_font(data->title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  data->text_layer = text_layer_create(GRect(10, 30, window_bounds.size.w - 20, window_bounds.size.h - 30));
  text_layer_set_font(data->text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  data->select_indicator_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BUTTON_INDICATOR);
  data->select_indicator_layer = bitmap_layer_create(
    GRect(window_bounds.size.w - 5, window_bounds.size.h / 2 - 10, 5, 20));
  bitmap_layer_set_bitmap(data->select_indicator_layer, data->select_indicator_bitmap);
  bitmap_layer_set_compositing_mode(data->select_indicator_layer, GCompOpSet);
  data->content_indicator_layer = layer_create(
    GRect(0, window_bounds.size.h - STATUS_BAR_LAYER_HEIGHT, window_bounds.size.w, STATUS_BAR_LAYER_HEIGHT));
  scroll_layer_set_shadow_hidden(data->scroll_layer, true);
  ContentIndicator *indicator = scroll_layer_get_content_indicator(data->scroll_layer);
  const ContentIndicatorConfig content_indicator_config = (ContentIndicatorConfig) {
    .layer = data->content_indicator_layer,
    .alignment = GAlignCenter,
    .colors = {
      .background = GColorWhite,
      .foreground = GColorBlack,
    }
  };
  content_indicator_configure_direction(indicator, ContentIndicatorDirectionDown, &content_indicator_config);
  layer_add_child(root_layer, scroll_layer_get_layer(data->scroll_layer));
  layer_add_child(root_layer, (Layer *) data->select_indicator_layer);
  scroll_layer_add_child(data->scroll_layer, (Layer *) data->title_layer);
  scroll_layer_add_child(data->scroll_layer, (Layer *) data->text_layer);
  layer_add_child(root_layer, data->content_indicator_layer);
  scroll_layer_set_callbacks(data->scroll_layer, (ScrollLayerCallbacks) {
    .click_config_provider = prv_click_config_provider,
  });
  scroll_layer_set_context(data->scroll_layer, window);
  data->app_message_handle = events_app_message_register_inbox_received(prv_app_message_handler, window);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
}


static void prv_window_appear(Window *window) {
  prv_set_stage(window, STAGE_LLM_WARNING);
}

static void prv_window_unload(Window *window) {
  ConsentWindowData* data = window_get_user_data(window);
  scroll_layer_destroy(data->scroll_layer);
  text_layer_destroy(data->title_layer);
  text_layer_destroy(data->text_layer);
  gbitmap_destroy(data->select_indicator_bitmap);
  bitmap_layer_destroy(data->select_indicator_layer);
  layer_destroy(data->content_indicator_layer);
  free(data);
}

static void prv_set_stage(Window* window, int stage) {
  ConsentWindowData *data = window_get_user_data(window);
  if (data->current_text) {
    free(data->current_text);
    data->current_text = NULL;
  }
  ResHandle res_handle = NULL;
  switch (stage) {
  case STAGE_LLM_WARNING:
    res_handle = resource_get_handle(RESOURCE_ID_LLM_WARNING_TEXT);
    data->title_text = "Important";
    break;
  case STAGE_GEMINI_CONSENT:
    res_handle = resource_get_handle(RESOURCE_ID_GEMINI_CONSENT_TEXT);
    data->title_text = "Privacy";
    break;
  case STAGE_LOCATION_CONSENT:
    res_handle = resource_get_handle(RESOURCE_ID_LOCATION_CONSENT_TEXT);
    data->title_text = "Location";
    break;
  default:
    APP_LOG(APP_LOG_LEVEL_ERROR, "Unknown consent stage: %d", stage);
    return;
  }
  if (res_handle != NULL) {
    size_t res_size = resource_size(res_handle);
    data->current_text = malloc(res_size + 1);
    resource_load(res_handle, (uint8_t*)data->current_text, res_size);
    data->current_text[res_size] = '\0';
  }
  data->stage = stage;
  text_layer_set_text(data->title_layer, data->title_text);
  text_layer_set_text(data->text_layer, data->current_text);
  GSize window_size = layer_get_frame(window_get_root_layer(window)).size;
  GSize text_size = graphics_text_layout_get_content_size(
    data->current_text,
    fonts_get_system_font(FONT_KEY_GOTHIC_24),
    GRect(10, 30, window_size.w - 20, 1000),
    GTextOverflowModeWordWrap,
    GTextAlignmentLeft);
  text_size.h += 5;
  text_layer_set_size(data->text_layer, text_size);
  scroll_layer_set_content_size(data->scroll_layer, GSize(window_size.w, 33 + text_size.h));
  scroll_layer_set_content_offset(data->scroll_layer, GPoint(0, 0), false);
}

static bool prv_did_scroll_to_bottom(Window* window) {
  ConsentWindowData *data = window_get_user_data(window);
  ScrollLayer *scroll_layer = data->scroll_layer;
  int16_t offset = -scroll_layer_get_content_offset(data->scroll_layer).y;
  return (offset + layer_get_frame((Layer *)scroll_layer).size.h >= scroll_layer_get_content_size(scroll_layer).h - 10);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  Window* window = context;
  ConsentWindowData *data = window_get_user_data(window);
  if (!prv_did_scroll_to_bottom(window)) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "User clicked select but hasn't scrolled to bottom; ignoring.");
    return;
  }
  switch (data->stage) {
    case STAGE_LLM_WARNING:
      prv_set_stage(window, STAGE_GEMINI_CONSENT);
      break;
    case STAGE_GEMINI_CONSENT:
      prv_set_stage(window, STAGE_LOCATION_CONSENT);
      break;
    case STAGE_LOCATION_CONSENT:
      prv_present_consent_menu(window);
      break;
  }
}

static void prv_present_consent_menu(Window* window) {
  ConsentWindowData* data = window_get_user_data(window);
  ActionMenuLevel* root_level = action_menu_level_create(2);
  action_menu_level_add_action(root_level, "Allow", prv_consent_menu_select_callback, (void *)true);
  action_menu_level_add_action(root_level, "Deny", prv_consent_menu_select_callback, (void *)false);
  ActionMenuConfig config = (ActionMenuConfig) {
    .root_level = root_level,
    .colors = {
      .background = ACCENT_COLOUR,
      .foreground = gcolor_legible_over(ACCENT_COLOUR),
    },
    .align = ActionMenuAlignCenter,
    .context = window,
    .did_close = prv_action_menu_close,
  };
  data->action_menu = action_menu_open(&config);
}

static void prv_consent_menu_select_callback(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
  Window *window = context;
  ConsentWindowData *data = window_get_user_data(window);
  data->expected_app_response = STAGE_LOCATION_CONSENT;
  bool choice = (int)action_menu_item_get_action_data(action);
  action_menu_freeze(action_menu);
  // We need to inform the phone of the user's choice.
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_int16(iter, MESSAGE_KEY_LOCATION_ENABLED, choice);
  app_message_outbox_send();
}

static void prv_app_message_handler(DictionaryIterator *iter, void *context) {
  Window* window = context;
  ConsentWindowData* data = window_get_user_data(window);
  if (data->expected_app_response != STAGE_LOCATION_CONSENT) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring unexpected location consent response.");
    return;
  }
  Tuple *tuple = dict_find(iter, MESSAGE_KEY_LOCATION_ENABLED);
  if (tuple == NULL) {
    return;
  }
  data->expected_app_response = 0;
  APP_LOG(APP_LOG_LEVEL_INFO, "Got location enabled reply, dismissing dialog.");
  events_app_message_unsubscribe(data->app_message_handle);
  bool location_enabled = tuple->value->int16;
  persist_write_bool(PERSIST_KEY_LOCATION_ENABLED, location_enabled);
  prv_mark_consents_complete();
  RootWindow *root_window = root_window_create();
  action_menu_set_result_window(data->action_menu, root_window_get_window(root_window));
  action_menu_close(data->action_menu, true);
  window_stack_remove(window, false);
}

static void prv_action_menu_close(ActionMenu* action_menu, const ActionMenuItem* item, void* context) {
  action_menu_hierarchy_destroy(action_menu_get_root_level(action_menu), NULL, NULL);
}

static void prv_mark_consents_complete() {
  persist_write_int(PERSIST_KEY_CONSENTS_COMPLETED, 1);
}
