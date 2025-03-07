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

#ifndef MESSAGE_LAYER_H
#define MESSAGE_LAYER_H

#include "../conversation.h"
#include <pebble.h>

typedef Layer MessageLayer;

MessageLayer* message_layer_create(GRect rect, ConversationEntry* entry);
void message_layer_destroy(MessageLayer* layer);
void message_layer_update(MessageLayer* layer);

#endif //MESSAGE_LAYER_H
