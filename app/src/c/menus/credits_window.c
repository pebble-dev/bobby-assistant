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

#include "credits_window.h"
#include <pebble.h>

typedef struct {
 TextLayer *text_layer;
 ScrollLayer *scroll_layer;
 StatusBarLayer *status_bar;
} CreditsWindowData;

const char * const credits_text = "Gemini\n"
                                  "AI processing is provided by Google's Gemini under the terms at https://ai.google.dev/gemini-api/terms (Bobby is a 'paid service').\n\n"
                                  "Weather\n"
                                  "Weather data provided by The Weather Channel.\n\n"
                                  "POIs\n"
                                  "© 2025 Mapbox and its suppliers. All rights reserved. Use of this data is subject to the Mapbox Terms of Service. (https://www.mapbox.com/about/maps/)\n\n"
                                  "Wikipedia\n"
                                  "Some grounding information is fetched from Wikipedia during request processing. Wikipedia content is available under the Creative Commons Attribution-ShareAlike License.\n\n"
                                  "Disclaimers\n"
                                  "Bobby includes experimental technology and may sometimes provide inaccurate or offensive content that doesn't represent Rebble's views.\n"
                                  "Use discretion before relying on, publishing, or otherwise using content provided by Bobby.\n"
                                  "Don't rely on Bobby for medical, legal, financial, or other professional advice. "
                                  "Any content regarding those topics is provided for informational purposes only and is not a substitute for "
                                  "advice from a qualified professional. Content does not constitute medical treatment or diagnosis.";

static void prv_window_load(Window* window);
static void prv_window_unload(Window* window);

void credits_menu_window_push() {
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
 Layer *root_layer = window_get_root_layer(window);
 GRect window_bounds = layer_get_bounds(root_layer);
 data->status_bar = status_bar_layer_create();
 status_bar_layer_set_colors(data->status_bar, GColorWhite, GColorBlack);
 status_bar_layer_set_separator_mode(data->status_bar, StatusBarLayerSeparatorModeDotted);
 layer_add_child(root_layer, status_bar_layer_get_layer(data->status_bar));
 data->scroll_layer = scroll_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, window_bounds.size.w, window_bounds.size.h - STATUS_BAR_LAYER_HEIGHT));
 scroll_layer_set_shadow_hidden(data->scroll_layer, true);
 scroll_layer_set_click_config_onto_window(data->scroll_layer, window);
 scroll_layer_set_content_size(data->scroll_layer, GSize(window_bounds.size.w, 1770));
 layer_add_child(root_layer, scroll_layer_get_layer(data->scroll_layer));
 data->text_layer = text_layer_create(GRect(5, 0, window_bounds.size.w - 10, 1770));
 text_layer_set_font(data->text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
 text_layer_set_text(data->text_layer, credits_text);
 scroll_layer_add_child(data->scroll_layer, text_layer_get_layer(data->text_layer));
}

static void prv_window_unload(Window* window) {
 CreditsWindowData *data = window_get_user_data(window);
 text_layer_destroy(data->text_layer);
 scroll_layer_destroy(data->scroll_layer);
 status_bar_layer_destroy(data->status_bar);
 free(data);
}
