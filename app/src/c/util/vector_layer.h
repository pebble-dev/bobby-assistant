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

#ifndef VECTOR_LAYER_H
#define VECTOR_LAYER_H

#include <pebble.h>

typedef Layer VectorLayer;

VectorLayer* vector_layer_create(GRect frame);
void vector_layer_destroy(VectorLayer *layer);
Layer* vector_layer_get_layer(VectorLayer *layer);
void vector_layer_set_vector(VectorLayer *layer, GDrawCommandImage *image);
GDrawCommandImage *vector_layer_get_vector(VectorLayer *layer);
void vector_layer_set_background_color(VectorLayer *layer, GColor color);

#endif //VECTOR_LAYER_H
