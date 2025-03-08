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

#include "result_window.h"
#include "vector_layer.h"
#include "style.h"
#include <pebble.h>

typedef struct {
  StatusBarLayer *status_bar;
  TextLayer* title_layer;
  TextLayer* text_layer;
  VectorLayer* image_layer;
#ifdef PBL_COLOR
  GColor background_color;
#endif
  char* title_text;
  char* text_text;
  GDrawCommandImage* image;
  AppTimer* timer;
} ResultWindowData;

static void prv_window_load(Window* window);
static void prv_window_unload(Window* window);
static void prv_window_appear(Window* window);
static void prv_timer_expired(void* context);

void result_window_push(const char* title, const char* text, GDrawCommandImage *image, GColor background_color) {
  Window* window = window_create();
  ResultWindowData* data = malloc(sizeof(ResultWindowData));
  // Only bother setting the background colour if we're on a colour device.
#ifdef PBL_COLOR
  window_set_background_color(window, background_color);
  data->background_color = background_color;
#endif
  data->image = image;
  data->title_text = malloc(strlen(title) + 1);
  strncpy(data->title_text, title, strlen(title) + 1);
  data->text_text = malloc(strlen(text) + 1);
  strncpy(data->text_text, text, strlen(text) + 1);
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
    .appear = prv_window_appear,
  });
  window_stack_push(window, true);
}

static void prv_window_load(Window* window) {
  ResultWindowData* data = window_get_user_data(window);
  Layer* root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);

  data->status_bar = status_bar_layer_create();
  bobby_status_bar_result_pane_config(data->status_bar);
#ifdef PBL_COLOR
  status_bar_layer_set_colors(data->status_bar, data->background_color, GColorBlack);
#endif
  layer_add_child(root_layer, status_bar_layer_get_layer(data->status_bar));

  data->title_layer = text_layer_create(GRect(0, 15, bounds.size.w, 35));
  text_layer_set_background_color(data->title_layer, GColorClear);
  text_layer_set_font(data->title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text(data->title_layer, data->title_text);
  text_layer_set_text_alignment(data->title_layer, GTextAlignmentCenter);
  layer_add_child(root_layer, text_layer_get_layer(data->title_layer));

  data->text_layer = text_layer_create(GRect(0, 50, bounds.size.w, bounds.size.h - 105));
  text_layer_set_background_color(data->text_layer, GColorClear);
  text_layer_set_text_alignment(data->text_layer, GTextAlignmentCenter);
  text_layer_set_font(data->text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(data->text_layer, data->text_text);
  layer_add_child(root_layer, text_layer_get_layer(data->text_layer));

  GSize image_size = gdraw_command_image_get_bounds_size(data->image);

  data->image_layer = vector_layer_create(GRect(bounds.size.w / 2 - image_size.w / 2, bounds.size.h - image_size.h - 5, image_size.w, image_size.h));
  vector_layer_set_vector(data->image_layer, data->image);
  layer_add_child(root_layer, vector_layer_get_layer(data->image_layer));
}

static void prv_window_unload(Window* window) {
  ResultWindowData* data = window_get_user_data(window);
  text_layer_destroy(data->title_layer);
  text_layer_destroy(data->text_layer);
  vector_layer_destroy(data->image_layer);
  status_bar_layer_destroy(data->status_bar);
  app_timer_cancel(data->timer);
  gdraw_command_image_destroy(data->image);
  free(data->title_text);
  free(data->text_text);
  free(data);
}

static void prv_window_appear(Window* window) {
  ResultWindowData* data = window_get_user_data(window);
  if (data->timer) {
    app_timer_cancel(data->timer);
  }
  data->timer = app_timer_register(4000, prv_timer_expired, window);
}

static void prv_timer_expired(void* context) {
  Window *window = context;
  window_stack_remove(window, true);
}
