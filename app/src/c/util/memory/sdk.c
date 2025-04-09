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

#include "sdk.h"
#include "malloc.h"

#include <pebble.h>

#include "pressure.h"

Layer *blayer_create(GRect frame) {
  void *space = bmalloc(64);
  free(space);
  return layer_create(frame);
}

Layer *blayer_create_with_data(GRect frame, size_t data_size) {
  void *space = bmalloc(data_size + 64 + 8);
  free(space);
  return layer_create_with_data(frame, data_size);
}

Window *bwindow_create() {
  void *space = bmalloc(144);
  free(space);
  return window_create();
}

ActionBarLayer *baction_bar_layer_create() {
  void *space = bmalloc(176);
  free(space);
  return action_bar_layer_create();
}

TextLayer *btext_layer_create(GRect frame) {
  void *space = bmalloc(96);
  free(space);
  return text_layer_create(frame);
}

MenuLayer *bmenu_layer_create(GRect frame) {
  void *space = bmalloc(456);
  free(space);
  return menu_layer_create(frame);
}

SimpleMenuLayer *bsimple_menu_layer_create(GRect frame, Window *window, SimpleMenuSection *sections, int32_t num_sections, void *context) {
  void *space = bmalloc(500);
  free(space);
  return simple_menu_layer_create(frame, window, sections, num_sections, context);
}

BitmapLayer *bbitmap_layer_create(GRect frame) {
  void *space = bmalloc(80);
  free(space);
  return bitmap_layer_create(frame);
}

ActionMenuLevel *baction_menu_level_create(int max_items) {
  void *space = bmalloc(36 + 20 * max_items);
  free(space);
  return action_menu_level_create(max_items);
}

ScrollLayer *bscroll_layer_create(GRect frame) {
  void *space = bmalloc(216);
  free(space);
  return scroll_layer_create(frame);
}

StatusBarLayer *bstatus_bar_layer_create() {
  void *space = bmalloc(204);
  free(space);
  return status_bar_layer_create();
}

GBitmap *bgbitmap_create_with_resource(uint32_t resource_id) {
  int heap_size = heap_bytes_free();
  void *ptr = gbitmap_create_with_resource(resource_id);
  if (ptr) {
    return ptr;
  }
  while (true) {
    void *ptr = gbitmap_create_with_resource(resource_id);
    if (ptr) {
      return ptr;
    }
    if (!memory_pressure_try_free()) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocate memory: couldn't free enough heap.");
      return NULL;
    }
    int new_heap_size = heap_bytes_free();
    APP_LOG(APP_LOG_LEVEL_INFO, "Freed %d bytes, heap size is now %d. Retrying gbitmap allocation.", heap_size - new_heap_size, new_heap_size);
  }
  return NULL;
}

GDrawCommandImage *bgdraw_command_image_create_with_resource(uint32_t resource_id) {
  int heap_size = heap_bytes_free();
  void *ptr = gdraw_command_image_create_with_resource(resource_id);
  if (ptr) {
    return ptr;
  }
  while (true) {
    void *ptr = gdraw_command_image_create_with_resource(resource_id);
    if (ptr) {
      return ptr;
    }
    if (!memory_pressure_try_free()) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocate memory: couldn't free enough heap.");
      return NULL;
    }
    int new_heap_size = heap_bytes_free();
    APP_LOG(APP_LOG_LEVEL_INFO, "Freed %d bytes, heap size is now %d. Retrying gdrawcommandimage allocation.", heap_size - new_heap_size, new_heap_size);
  }
  return NULL;
}

GDrawCommandImage *bgdraw_command_sequence_create_with_resource(uint32_t resource_id) {
  int heap_size = heap_bytes_free();
  void *ptr = gdraw_command_sequence_create_with_resource(resource_id);
  if (ptr) {
    return ptr;
  }
  while (true) {
    void *ptr = gdraw_command_sequence_create_with_resource(resource_id);
    if (ptr) {
      return ptr;
    }
    if (!memory_pressure_try_free()) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocate memory: couldn't free enough heap.");
      return NULL;
    }
    int new_heap_size = heap_bytes_free();
    APP_LOG(APP_LOG_LEVEL_INFO, "Freed %d bytes, heap size is now %d. Retrying gdrawcommandsequence allocation.", heap_size - new_heap_size, new_heap_size);
  }
  return NULL;
}
