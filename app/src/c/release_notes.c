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

#include "release_notes.h"
#include <pebble.h>

#include "util/formatted_text_layer.h"
#include "util/logging.h"
#include "util/memory/malloc.h"
#include "util/memory/sdk.h"
#include "version/version.h"

typedef struct {
  Window *window;
  ScrollLayer *scroll_layer;
  FormattedTextLayer *text_layer;
  char* text;
} ReleaseNotesWindowData;

static void prv_load(Window *window);
static void prv_unload(Window *window);
// static void prv_click_config_provider(void *context);
static char* prv_create_release_notes();

void prv_release_notes_push() {
  Window *window = bwindow_create();
  ReleaseNotesWindowData *data = bmalloc(sizeof(ReleaseNotesWindowData));
  data->window = window;
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_load,
    .unload = prv_unload,
  });
  window_stack_push(window, true);
}

void release_notes_maybe_push() {
  if (version_is_updated() && !version_is_first_launch()) {
    BOBBY_LOG(APP_LOG_LEVEL_INFO, "Showing release notes");
    prv_release_notes_push();
  } else {
    BOBBY_LOG(APP_LOG_LEVEL_INFO, "Not showing release notes. Is updated: %d, Is first launch: %d", version_is_updated(), version_is_first_launch());
  }
}

static char* prv_create_release_notes() {
  ResHandle handle = resource_get_handle(RESOURCE_ID_CHANGELOG_1_4);
  size_t size = resource_size(handle) + 1;
  char* text = bmalloc(size);
  text[size - 1] = '\0';
  resource_load(handle, (uint8_t*)text, size);
  return text;
}

static void prv_load(Window *window) {
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  ReleaseNotesWindowData *data = window_get_user_data(window);
  data->text = prv_create_release_notes();
  data->text_layer = formatted_text_layer_create(GRect(5, 0, bounds.size.w - 10, 5000));
  formatted_text_layer_set_text(data->text_layer, data->text);
  GSize content_size = formatted_text_layer_get_content_size(data->text_layer);
  layer_set_frame(data->text_layer, GRect(5, 0, bounds.size.w - 10, content_size.h + 10));
  data->scroll_layer = scroll_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  scroll_layer_set_shadow_hidden(data->scroll_layer, true);
  scroll_layer_set_content_size(data->scroll_layer, layer_get_frame(data->text_layer).size);
  scroll_layer_set_click_config_onto_window(data->scroll_layer, window);
  scroll_layer_add_child(data->scroll_layer, data->text_layer);
  layer_add_child(window_get_root_layer(window), scroll_layer_get_layer(data->scroll_layer));
  scroll_layer_set_click_config_onto_window(data->scroll_layer, window);
}

static void prv_unload(Window *window) {
  ReleaseNotesWindowData *data = window_get_user_data(window);
  free(data->text);
  formatted_text_layer_destroy(data->text_layer);
  scroll_layer_destroy(data->scroll_layer);
  free(data);
  window_destroy(window);
}
