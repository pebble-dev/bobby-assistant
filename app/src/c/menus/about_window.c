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

#include "about_window.h"

#include <pebble.h>

#include "../util/style.h"
#include "../util/formatted_text_layer.h"
#include "../util/vector_layer.h"
#include "../version/version.h"

typedef struct {
  char *about_text;
  FormattedTextLayer *text_layer;
  ScrollLayer *scroll_layer;
  StatusBarLayer *status_bar;
  BitmapLayer *bitmap_layer;
  GBitmap *bobby_image;
} AboutWindowData;

static void prv_window_load(Window* window);
static void prv_window_unload(Window* window);

void about_window_push() {
  Window *window = window_create();
  AboutWindowData *data = malloc(sizeof(AboutWindowData));
  memset(data, 0, sizeof(AboutWindowData));
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
 window_stack_push(window, true);
}

static void prv_window_load(Window* window) {
  AboutWindowData *data = window_get_user_data(window);
  Layer *root_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(root_layer);

  ResHandle res_handle = resource_get_handle(RESOURCE_ID_ABOUT_TEXT);
  size_t res_size = resource_size(res_handle);
  char *about_text = malloc(res_size + 1);
  resource_load(res_handle, (uint8_t*)about_text, res_size);
  about_text[res_size] = '\0';
  data->about_text = malloc(res_size + 7);
  VersionInfo version = version_get_current();
  snprintf(data->about_text, res_size + 7, about_text, version.major, version.minor);
  data->about_text[res_size+6] = '\0';

  window_set_background_color(window, BRANDED_BACKGROUND_COLOUR);

  data->status_bar = status_bar_layer_create();
  bobby_status_bar_result_pane_config(data->status_bar);
  layer_add_child(root_layer, status_bar_layer_get_layer(data->status_bar));

  data->scroll_layer = scroll_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, window_bounds.size.w, window_bounds.size.h - STATUS_BAR_LAYER_HEIGHT));
  scroll_layer_set_shadow_hidden(data->scroll_layer, true);
  scroll_layer_set_click_config_onto_window(data->scroll_layer, window);
  layer_add_child(root_layer, scroll_layer_get_layer(data->scroll_layer));

  data->text_layer = formatted_text_layer_create(GRect(5, 0, window_bounds.size.w - 10, 10000));
  formatted_text_layer_set_text_alignment(data->text_layer, GTextAlignmentCenter);
  formatted_text_layer_set_text(data->text_layer, data->about_text);
  GSize text_size = formatted_text_layer_get_content_size(data->text_layer);

  data->bobby_image = gbitmap_create_with_resource(RESOURCE_ID_FENCE_PONY_BITMAP);
  GSize image_size = gbitmap_get_bounds(data->bobby_image).size;
  image_size.h += 40; // add back the space at the top
  data->bitmap_layer = bitmap_layer_create(GRect((window_bounds.size.w - image_size.w) / 2, text_size.h, image_size.w, image_size.h));
  bitmap_layer_set_bitmap(data->bitmap_layer, data->bobby_image);
  bitmap_layer_set_alignment(data->bitmap_layer, GAlignBottom);

  scroll_layer_set_content_size(data->scroll_layer, GSize(window_bounds.size.w, text_size.h + image_size.h));
  scroll_layer_add_child(data->scroll_layer, formatted_text_layer_get_layer(data->text_layer));
  scroll_layer_add_child(data->scroll_layer, bitmap_layer_get_layer(data->bitmap_layer));
}

static void prv_window_unload(Window* window) {
  AboutWindowData *data = window_get_user_data(window);
  free(data->about_text);
  formatted_text_layer_destroy(data->text_layer);
  scroll_layer_destroy(data->scroll_layer);
  status_bar_layer_destroy(data->status_bar);
  bitmap_layer_destroy(data->bitmap_layer);
  gbitmap_destroy(data->bobby_image);
  free(data);
  window_destroy(window);
}
