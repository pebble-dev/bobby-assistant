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

#include "legal_window.h"
#include <pebble.h>
#include "../util/style.h"
#include "../util/formatted_text_layer.h"

typedef struct {
 char *legal_text;
 FormattedTextLayer *text_layer;
 ScrollLayer *scroll_layer;
 StatusBarLayer *status_bar;
} CreditsWindowData;

static void prv_window_load(Window* window);
static void prv_window_unload(Window* window);

void legal_window_push() {
 Window *window = window_create();
 CreditsWindowData *data = malloc(sizeof(CreditsWindowData));
 memset(data, 0, sizeof(CreditsWindowData));
 window_set_user_data(window, data);
 window_set_window_handlers(window, (WindowHandlers) {
   .load = prv_window_load,
   .unload = prv_window_unload,
 });
 window_stack_push(window, true);
}

static void prv_window_load(Window* window) {
 CreditsWindowData *data = window_get_user_data(window);
 ResHandle res_handle = resource_get_handle(RESOURCE_ID_LEGAL_TEXT);
 size_t res_size = resource_size(res_handle);
 data->legal_text = malloc(res_size + 1);
 resource_load(res_handle, (uint8_t*)data->legal_text, res_size);
 data->legal_text[res_size] = '\0';
 Layer *root_layer = window_get_root_layer(window);
 GRect window_bounds = layer_get_bounds(root_layer);
 data->status_bar = status_bar_layer_create();
 bobby_status_bar_config(data->status_bar);
 layer_add_child(root_layer, status_bar_layer_get_layer(data->status_bar));
 data->scroll_layer = scroll_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, window_bounds.size.w, window_bounds.size.h - STATUS_BAR_LAYER_HEIGHT));
 scroll_layer_set_shadow_hidden(data->scroll_layer, true);
 scroll_layer_set_click_config_onto_window(data->scroll_layer, window);
 layer_add_child(root_layer, scroll_layer_get_layer(data->scroll_layer));
 data->text_layer = formatted_text_layer_create(GRect(5, 0, window_bounds.size.w - 10, 10000));
 formatted_text_layer_set_text(data->text_layer, data->legal_text);
 GSize text_size = formatted_text_layer_get_content_size(data->text_layer);
 scroll_layer_set_content_size(data->scroll_layer, GSize(window_bounds.size.w, text_size.h + 10));
 scroll_layer_add_child(data->scroll_layer, formatted_text_layer_get_layer(data->text_layer));
}

static void prv_window_unload(Window* window) {
 CreditsWindowData *data = window_get_user_data(window);
 free(data->legal_text);
 formatted_text_layer_destroy(data->text_layer);
 scroll_layer_destroy(data->scroll_layer);
 status_bar_layer_destroy(data->status_bar);
 free(data);
 window_destroy(window);
}
