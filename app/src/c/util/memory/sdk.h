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

Layer *blayer_create();
Layer *blayer_create_with_data(GRect frame, size_t data_size);
Window *bwindow_create();
ActionBarLayer *baction_bar_layer_create();
TextLayer *btext_layer_create(GRect frame);
MenuLayer *bmenu_layer_create(GRect frame);
SimpleMenuLayer *bsimple_menu_layer_create(GRect frame, Window *window, SimpleMenuSection *sections, int32_t num_sections, void *context);
BitmapLayer *bbitmap_layer_create(GRect frame);
ActionMenuLevel *baction_menu_level_create(int max_items);
ScrollLayer *bscroll_layer_create(GRect frame);
StatusBarLayer *bstatus_bar_layer_create();
GBitmap *bgbitmap_create_with_resource(uint32_t resource_id);
GDrawCommandImage *bgdraw_command_image_create_with_resource(uint32_t resource_id);
GDrawCommandSequence *bgdraw_command_sequence_create_with_resource(uint32_t resource_id);
