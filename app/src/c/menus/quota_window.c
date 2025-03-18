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

#include "quota_window.h"
#include "usage_layer.h"
#include "../util/vector_sequence_layer.h"
#include "../util/style.h"

#include <pebble.h>
#include <pebble-events/pebble-events.h>

typedef struct {
  UsageLayer* usage_layer;
  TextLayer* explanation_layer;
  GDrawCommandSequence *loading_sequence;
  VectorSequenceLayer* loading_layer;
  EventHandle app_message_handle;
  ScrollLayer* scroll_layer;
  StatusBarLayer* status_bar;
  char explanation[164];
} QuotaWindowData;

static void prv_window_load(Window* window);
static void prv_window_unload(Window* window);
static void prv_fetch_quota(Window* window);
static void prv_app_message_received(DictionaryIterator* iter, void* context);

void push_quota_window() {
  QuotaWindowData* data = malloc(sizeof(QuotaWindowData));
  Window* window = window_create();
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(window, true);
}

static void prv_window_load(Window* window) {
  QuotaWindowData* data = window_get_user_data(window);
  Layer* root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);
  data->status_bar = status_bar_layer_create();
  bobby_status_bar_config(data->status_bar);
  data->scroll_layer = scroll_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, bounds.size.w, bounds.size.h - STATUS_BAR_LAYER_HEIGHT));
  scroll_layer_set_content_size(data->scroll_layer, GSize(bounds.size.w, 300));
  scroll_layer_set_click_config_onto_window(data->scroll_layer, window);
  scroll_layer_set_shadow_hidden(data->scroll_layer, true);
  data->usage_layer = usage_layer_create(GRect(10, 5, bounds.size.w - 20, 20));
  data->explanation_layer = text_layer_create(GRect(10, 25, bounds.size.w - 20, 275));
  text_layer_set_font(data->explanation_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  scroll_layer_add_child(data->scroll_layer, (Layer *)data->explanation_layer);
  scroll_layer_add_child(data->scroll_layer, (Layer *)data->usage_layer);
  // We need to look up the quota, so we'll show a running pony while we do that.
  data->loading_sequence = gdraw_command_sequence_create_with_resource(RESOURCE_ID_RUNNING_PONY);
  GSize pony_size = gdraw_command_sequence_get_bounds_size(data->loading_sequence);
  data->loading_layer = vector_sequence_layer_create(GRect(bounds.size.w / 2 - pony_size.w / 2, bounds.size.h / 2 - pony_size.h / 2, pony_size.w, pony_size.h));
  vector_sequence_layer_set_sequence(data->loading_layer, data->loading_sequence);
  layer_add_child(root_layer, data->loading_layer);
  layer_add_child(root_layer, (Layer *)data->status_bar);
  vector_sequence_layer_play(data->loading_layer);
  data->app_message_handle = events_app_message_register_inbox_received(prv_app_message_received, window);
  prv_fetch_quota(window);
}

static void prv_window_unload(Window* window) {
  QuotaWindowData* data = window_get_user_data(window);
  usage_layer_destroy(data->usage_layer);
  text_layer_destroy(data->explanation_layer);
  vector_sequence_layer_destroy(data->loading_layer);
  gdraw_command_sequence_destroy(data->loading_sequence);
  events_app_message_unsubscribe(data->app_message_handle);
  scroll_layer_destroy(data->scroll_layer);
  status_bar_layer_destroy(data->status_bar);
  free(data);
  window_destroy(window);
}

static void prv_fetch_quota(Window* window) {
  DictionaryIterator* iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, MESSAGE_KEY_QUOTA_REQUEST, 1);
  app_message_outbox_send();
}

static void prv_app_message_received(DictionaryIterator* iter, void* context) {
  Window *window = (Window *)context;
  QuotaWindowData* data = window_get_user_data(window);
  Layer* root_layer = window_get_root_layer(window);
  Tuple* tuple = dict_find(iter, MESSAGE_KEY_QUOTA_RESPONSE_USED);
  if(!tuple) {
    return;
  }
  int used = tuple->value->int32;
  tuple = dict_find(iter, MESSAGE_KEY_QUOTA_RESPONSE_REMAINING);
  if (!tuple) {
    return;
  }
  int remaining = tuple->value->int32;
  uint64_t percentage = PERCENTAGE_MAX;
  APP_LOG(APP_LOG_LEVEL_INFO, "Quota: %d used, %d remaining", used, remaining);
  if (used == 0 && remaining == 0) {
    strncpy(data->explanation, "You need a Rebble subscription to use Bobby. You can sign up at auth.rebble.io.", sizeof(data->explanation));
  } else {
    int display_percent = (int)(((uint64_t)used * 100) / (used + remaining));
    percentage = ((uint64_t)used * PERCENTAGE_MAX) / (used + remaining);
    snprintf(data->explanation, sizeof(data->explanation), "You've used %d%% of your Bobby quota for this month. Once you've used 100%%, Bobby will stop working until next month. Quota resets on the first day of each month.", display_percent);
  }
  text_layer_set_text(data->explanation_layer, data->explanation);
  usage_layer_set_percentage(data->usage_layer, (int16_t)percentage);
  vector_sequence_layer_stop(data->loading_layer);
  layer_remove_from_parent(data->loading_layer);
  layer_add_child(root_layer, (Layer *)data->scroll_layer);
}
