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

#ifndef VECTOR_SEQUENCE_LAYER_H
#define VECTOR_SEQUENCE_LAYER_H

#include <pebble.h>

typedef Layer VectorSequenceLayer;

VectorSequenceLayer* vector_sequence_layer_create(GRect frame);
void vector_sequence_layer_destroy(VectorSequenceLayer *layer);
Layer* vector_sequence_layer_get_layer(VectorSequenceLayer *layer);
void vector_sequence_layer_set_sequence(VectorSequenceLayer *layer, GDrawCommandSequence *image);
GDrawCommandSequence *vector_sequence_layer_get_sequence(VectorSequenceLayer *layer);
void vector_sequence_layer_set_background_color(VectorSequenceLayer *layer, GColor color);
void vector_sequence_layer_play(VectorSequenceLayer *layer);
void vector_sequence_layer_stop(VectorSequenceLayer *layer);

#endif //VECTOR_SEQUENCE_LAYER_H
