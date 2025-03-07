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

#include "segment_layer.h"
#include "../conversation.h"
#include "info_layer.h"
#include "message_layer.h"

#include <pebble.h>

#define CONTENT_FONT FONT_KEY_GOTHIC_24_BOLD
#define NAME_HEIGHT 15

typedef enum {
  SegmentTypeMessage,
  SegmentTypeInfo,
} SegmentType;

typedef struct {
  ConversationEntry* entry;
  SegmentType type;
  union {
    // fun fact: every member of this union is actually a Layer*.
    // given this, we can get away with using a generic layer for layer-accepting functions.
    // also as a result, there is absolutely no warning if you call the wrong methods on these, sigh.
    Layer* layer;
    InfoLayer* info_layer;
    MessageLayer* message_layer;
  };
} SegmentLayerData;

static SegmentType prv_get_segment_type(ConversationEntry* entry);

SegmentLayer* segment_layer_create(GRect rect, ConversationEntry* entry) {
  Layer* layer = layer_create_with_data(rect, sizeof(SegmentLayerData));
  SegmentLayerData* data = layer_get_data(layer);
  data->entry = entry;
  data->type = prv_get_segment_type(entry);
  GRect child_frame = GRect(0, 0, rect.size.w, rect.size.h);
  switch (data->type) {
    case SegmentTypeMessage:
      data->message_layer = message_layer_create(child_frame, entry);
      layer_add_child(layer, data->message_layer);
      break;
    case SegmentTypeInfo:
      data->info_layer = info_layer_create(child_frame, entry);
      layer_add_child(layer, data->info_layer);
      break;
  }
  GSize child_size = layer_get_frame(data->layer).size;
  layer_set_frame(layer, GRect(rect.origin.x, rect.origin.y, child_size.w, child_size.h));
  return layer;
}

void segment_layer_destroy(SegmentLayer* layer) {
  SegmentLayerData* data = layer_get_data(layer);
  switch (data->type) {
    case SegmentTypeMessage:
      message_layer_destroy(data->message_layer);
      break;
    case SegmentTypeInfo:
      info_layer_destroy(data->info_layer);
      break;
  }
}

ConversationEntry* segment_layer_get_entry(SegmentLayer* layer) {
  SegmentLayerData* data = layer_get_data(layer);
  return data->entry;
}

void segment_layer_update(SegmentLayer* layer) {
  SegmentLayerData* data = layer_get_data(layer);
  switch (data->type) {
    case SegmentTypeMessage:
      message_layer_update(data->message_layer);
      break;
    case SegmentTypeInfo:
      info_layer_update(data->info_layer);
      break;
  }
  GSize child_size = layer_get_frame(data->layer).size;
  GPoint origin = layer_get_frame(layer).origin;
  layer_set_frame(layer, GRect(origin.x, origin.y, child_size.w, child_size.h));
}

static SegmentType prv_get_segment_type(ConversationEntry* entry) {
  switch (conversation_entry_get_type(entry)) {
    case EntryTypePrompt:
    case EntryTypeResponse:
      return SegmentTypeMessage;
    case EntryTypeThought:
    case EntryTypeError:
    case EntryTypeAction:
      return SegmentTypeInfo;
  }
  APP_LOG(APP_LOG_LEVEL_ERROR, "Unknown entry type %d.", conversation_entry_get_type(entry));
  return SegmentTypeMessage;
}
