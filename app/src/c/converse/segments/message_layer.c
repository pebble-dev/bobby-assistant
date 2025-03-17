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

#include "message_layer.h"

#include <pebble.h>


#define CONTENT_FONT FONT_KEY_GOTHIC_24_BOLD
#define NAME_HEIGHT 15

typedef struct {
  ConversationEntry* entry;
  TextLayer* speaker_layer;
  TextLayer* content_layer;
  uint16_t content_height;
  size_t last_newline_offset;
} MessageLayerData;

static char *prv_get_content_text(MessageLayer *layer);
static int prv_get_content_height(MessageLayer* layer);

MessageLayer* message_layer_create(GRect rect, ConversationEntry* entry) {
    Layer* layer = layer_create_with_data(rect, sizeof(MessageLayerData));
    MessageLayerData* data = layer_get_data(layer);
    data->entry = entry;
    data->speaker_layer = text_layer_create(GRect(5, 0, rect.size.w, NAME_HEIGHT));
    size_t content_origin_y = -5;
    EntryType type = conversation_entry_get_type(entry);
    if (type == EntryTypePrompt) {
      text_layer_set_text(data->speaker_layer, "You");
      content_origin_y = NAME_HEIGHT;
    }
    layer_add_child(layer, (Layer *)data->speaker_layer);
    data->last_newline_offset = 0;
    data->content_height = 24;
    data->content_height = prv_get_content_height(layer);
    data->content_layer = text_layer_create(GRect(5, content_origin_y, rect.size.w - 10, data->content_height));
    text_layer_set_text(data->content_layer, prv_get_content_text(layer));
    text_layer_set_font(data->content_layer, fonts_get_system_font(CONTENT_FONT));
    data->content_height = text_layer_get_content_size(data->content_layer).h;
    layer_add_child(layer, (Layer *)data->content_layer);
    message_layer_update(layer);
    return layer;
}

void message_layer_destroy(MessageLayer* layer) {
  MessageLayerData* data = layer_get_data(layer);
  if (data->speaker_layer) {
    text_layer_destroy(data->speaker_layer);
  }
  text_layer_destroy(data->content_layer);
  layer_destroy(layer);
}

void message_layer_update(MessageLayer* layer) {
  MessageLayerData* data = layer_get_data(layer);
  // The text pointer can change out underneath us.
  text_layer_set_text(data->content_layer, prv_get_content_text(layer));
  data->content_height = prv_get_content_height(layer);
  int width = layer_get_bounds(layer).size.w;
  GRect frame = layer_get_frame(layer);
  frame.size.h = data->content_height + 5;
  if (conversation_entry_get_type(data->entry) == EntryTypePrompt) {
    frame.size.h += NAME_HEIGHT;
  }
  text_layer_set_size(data->content_layer, GSize(width - 10, data->content_height + 5));
  layer_set_frame(layer, frame);
}

static char *prv_get_content_text(MessageLayer *layer) {
  MessageLayerData* data = layer_get_data(layer);
  ConversationEntry* entry = data->entry;
  switch(conversation_entry_get_type(entry)) {
    case EntryTypePrompt:
      return conversation_entry_get_prompt(entry)->prompt;
    case EntryTypeResponse:
      return conversation_entry_get_response(entry)->response;
    default:
      return "(Bobby bug)";
  }
}

static int prv_get_content_height(MessageLayer* layer) {
  MessageLayerData* data = layer_get_data(layer);
  // Measuring the whole field every time we add text is way too expensive.
  // Instead, just try to figure out where we are line breaking. We don't get enough information
  // from the text layout engine to do this in any particularly clever way, so instead we try
  // to figure out what's on the current line, and when that spills over to the next line.
  // Then we figure out (guess) exactly what spilled over and repeat from there.
  size_t offset = data->last_newline_offset;
  char* text = prv_get_content_text(layer);
  const GFont font = fonts_get_system_font(CONTENT_FONT);
  const GRect rect = GRect(0, 0, layer_get_frame(layer).size.w - 10, 10000);
  // This algorithm is somewhat buggy (it can't cope with words getting broken; it assumes only one break per
  // fragment), so for content where speed is less important just actually measure the thing.
  GTextAlignment alignment = GTextAlignmentLeft;
  if (conversation_entry_get_type(data->entry) != EntryTypeResponse || conversation_entry_get_response(data->entry)->complete) {
    return graphics_text_layout_get_content_size(text, font, rect, GTextOverflowModeTrailingEllipsis, alignment).h;
  }
  int height = graphics_text_layout_get_content_size(text + offset, font, rect, GTextOverflowModeTrailingEllipsis, alignment).h;
  int content_height = data->content_height;

  if (height > 35) {
    int h2 = 0;
    // we broke to a new line. see if we can figure out where.
    size_t len = strlen(text + offset);
    for (int i = len; i > 0; --i) {
      // TODO: maybe we should just copy it? This could cause us trouble with literals...
      char c = (text+offset)[i];
      (text+offset)[i] = 0;
      h2 = graphics_text_layout_get_content_size(text + offset, font, rect, GTextOverflowModeTrailingEllipsis, alignment).h;
      (text+offset)[i] = c;
      if (h2 < height) {
        // now try to backtrack to where in the text we caused this.
        // I can't think of any way to infer this from the sizing, so just go back to a word break.
        for (int j = i - 1; j >= 0; --j) {
          if ((text+offset)[j] == ' ' || (text+offset)[j] == '-' || (text+offset)[j] == '\n') {
            i = j+1;
//            APP_LOG(APP_LOG_LEVEL_DEBUG, "New line starts \"%s\".", text+offset+i);
            break;
          }
        }
        offset += i;
        break;
      }
    }
    data->last_newline_offset = offset;
    content_height += height - h2;
  }

  return content_height;
}
