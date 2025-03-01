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
#include "../root_window.h"

#include <pebble.h>
#include <pebble-events/pebble-events.h>

#define STAGE_LLM_WARNING 0
#define STAGE_GEMINI_CONSENT 1

typedef struct {
  ScrollLayer *scroll_layer;
  TextLayer *title_layer;
  TextLayer *text_layer;
  Layer *content_indicator_layer;
  const char* current_text;
  const char* title_text;
  GBitmap* select_indicator_bitmap;
  BitmapLayer* select_indicator_layer;
  int stage;
} ConsentWindowData;

static void prv_set_stage(Window* window, int stage);
static void prv_window_load(Window *window);
static void prv_window_unload(Window *window);
static void prv_window_appear(Window *window);
static void prv_click_config_provider(void *context);
static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context);
static bool prv_did_scroll_to_bottom(Window* window);

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
  return !persist_exists(PERSIST_KEY_CONSENT_GRANTED);
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
  switch (stage) {
  case STAGE_LLM_WARNING:
    data->current_text = "Bobby uses an AI language model to respond to your requests. Like all other AI language models, Bobby may lie, do the wrong thing, or make offensive or inappropriate comments.";
    data->title_text = "Important";
    break;
  case STAGE_GEMINI_CONSENT:
    data->current_text = "Bobby uses Google's Gemini API to process requests. To do this, all of your requests will be sent verbatim to Google. If you do not agree, you cannot use Bobby.";
    data->title_text = "Privacy";
    break;
  default:
    APP_LOG(APP_LOG_LEVEL_ERROR, "Unknown consent stage: %d", stage);
    return;
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
      persist_write_bool(PERSIST_KEY_CONSENT_GRANTED, true);
      RootWindow *root_window = root_window_create();
      window_stack_push(root_window_get_window(root_window), true);
      window_stack_remove(window, false);
      break;
  }
}
