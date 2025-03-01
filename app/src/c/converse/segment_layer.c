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
#include "conversation.h"
#include "../util/thinking_layer.h"

#include <pebble.h>

#define CONTENT_FONT FONT_KEY_GOTHIC_24
#define NAME_HEIGHT 15

typedef struct {
  ConversationEntry* entry;
  TextLayer* speaker_layer;
  TextLayer* content_layer;
  ThinkingLayer* thinking_layer;
  uint16_t content_height;
  size_t last_newline_offset;
  char* content_text; // For ConversationEntries without their own text, we need somewhere to stash what we generate.
} SegmentLayerData;

static char *prv_get_content_text(SegmentLayer *layer);
static int prv_get_content_height(SegmentLayer* layer);
static void prv_layer_render(Layer* layer, GContext* ctx);
static bool prv_has_border(SegmentLayer* layer);
static bool prv_has_thinking(SegmentLayer* layer);
static char* prv_generate_action_text(ConversationAction* action);

SegmentLayer* segment_layer_create(GRect rect, ConversationEntry* entry) {
    Layer* layer = layer_create_with_data(rect, sizeof(SegmentLayerData));
    SegmentLayerData* data = layer_get_data(layer);
    data->entry = entry;
    bool has_border = prv_has_border(layer);
    if (!has_border) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Adding speaker layer.");
      data->speaker_layer = text_layer_create(GRect(0, 0, rect.size.w, NAME_HEIGHT));
      EntryType type = conversation_entry_get_type(entry);
      if (type == EntryTypePrompt) {
        text_layer_set_text(data->speaker_layer, "You");
      } else if (type == EntryTypeResponse) {
        text_layer_set_text(data->speaker_layer, "Bobby");
      }
      layer_add_child(layer, (Layer *)data->speaker_layer);
    } else {
      data->speaker_layer = NULL;
    }
    data->thinking_layer = NULL;
    data->content_text = NULL;
    data->last_newline_offset = 0;
    data->content_height = 24;
    data->content_height = prv_get_content_height(layer);
    data->content_layer = text_layer_create(GRect(has_border ? 5 : 0, has_border ? 1 : NAME_HEIGHT, rect.size.w - has_border ? 10 : 0, data->content_height));
    text_layer_set_text(data->content_layer, prv_get_content_text(layer));
    text_layer_set_font(data->content_layer, fonts_get_system_font(CONTENT_FONT));
    if (has_border) {
      text_layer_set_text_alignment(data->content_layer, GTextAlignmentCenter);
    }
    data->content_height = text_layer_get_content_size(data->content_layer).h;
    layer_add_child(layer, (Layer *)data->content_layer);
    segment_layer_update(layer);
    layer_set_update_proc(layer, prv_layer_render);
    return layer;
}

void segment_layer_destroy(SegmentLayer* layer) {
  SegmentLayerData* data = layer_get_data(layer);
  if (data->speaker_layer) {
    text_layer_destroy(data->speaker_layer);
  }
  if (data->thinking_layer) {
    thinking_layer_destroy(data->thinking_layer);
  }
  text_layer_destroy(data->content_layer);
  if (data->content_text) {
    free(data->content_text);
  }
}

ConversationEntry* segment_layer_get_entry(SegmentLayer* layer) {
  SegmentLayerData* data = layer_get_data(layer);
  return data->entry;
}

void segment_layer_update(SegmentLayer* layer) {
  SegmentLayerData* data = layer_get_data(layer);
  // The text pointer can change out underneath us.
  text_layer_set_text(data->content_layer, prv_get_content_text(layer));
  data->content_height = prv_get_content_height(layer);
  int width = layer_get_bounds(layer).size.w;
  bool has_border = prv_has_border(layer);
  if (has_border) {
    width -= 10;
  }
  GRect frame = layer_get_frame(layer);
  frame.size.h = data->content_height + (has_border ? 6 : NAME_HEIGHT) + 5;
  if (prv_has_thinking(layer)) {
    if (!data->thinking_layer) {
      data->thinking_layer = thinking_layer_create(GRect((width - THINKING_LAYER_WIDTH) / 2, frame.size.h, THINKING_LAYER_WIDTH, THINKING_LAYER_HEIGHT));
      layer_add_child(layer, data->thinking_layer);
    } else {
      GRect thinking_frame = layer_get_frame(data->thinking_layer);
      thinking_frame.origin.y = frame.size.h;
      layer_set_frame(data->thinking_layer, thinking_frame);
    }
    frame.size.h += THINKING_LAYER_HEIGHT + 5;
  } else if (data->thinking_layer) {
    layer_remove_from_parent(data->thinking_layer);
    thinking_layer_destroy(data->thinking_layer);
    data->thinking_layer = 0;
  }
  text_layer_set_size(data->content_layer, GSize(width, data->content_height + 5));
  layer_set_frame(layer, frame);
}

static char *prv_get_content_text(SegmentLayer *layer) {
  SegmentLayerData* data = layer_get_data(layer);
  ConversationEntry* entry = data->entry;
  switch(conversation_entry_get_type(entry)) {
    case EntryTypePrompt:
      return conversation_entry_get_prompt(entry)->prompt;
    case EntryTypeResponse:
      return conversation_entry_get_response(entry)->response;
    case EntryTypeThought:
      return conversation_entry_get_thought(entry)->thought;
    case EntryTypeError:
      return conversation_entry_get_error(entry)->message;
    case EntryTypeAction:
      // Assumption: the action text doesn't ever change.
      if (!data->content_text) {
        data->content_text = prv_generate_action_text(conversation_entry_get_action(entry));
      }
      return data->content_text;
  }
  return "(Bobby bug)";
}

static int prv_get_content_height(SegmentLayer* layer) {
  SegmentLayerData* data = layer_get_data(layer);
  // Measuring the whole field every time we add text is way too expensive.
  // Instead, just try to figure out where we are line breaking. We don't get enough information
  // from the text layout engine to do this in any particularly clever way, so instead we try
  // to figure out what's on the current line, and when that spills over to the next line.
  // Then we figure out (guess) exactly what spilled over and repeat from there.
  size_t offset = data->last_newline_offset;
  char* text = prv_get_content_text(layer);
  const GFont font = fonts_get_system_font(CONTENT_FONT);
  const GRect rect = GRect(0, 0, layer_get_frame(layer).size.w - (prv_has_border(layer) ? 10 : 0), 10000);
  // This algorithm is somewhat buggy (it can't cope with words getting broken; it assumes only one break per
  // fragment), so for content where speed is less important just actually measure the thing.
  GTextAlignment alignment = prv_has_border(layer) ? GTextAlignmentCenter : GTextAlignmentLeft;
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

static bool prv_has_border(SegmentLayer* layer) {
  SegmentLayerData* data = layer_get_data(layer);
  EntryType entryType = conversation_entry_get_type(data->entry);
  return (entryType == EntryTypeThought || entryType == EntryTypeAction || entryType == EntryTypeError);
}

static bool prv_has_thinking(SegmentLayer* layer) {
  SegmentLayerData* data = layer_get_data(layer);
  EntryType entryType = conversation_entry_get_type(data->entry);
  if (entryType == EntryTypeThought) {
    return true;
  }
  if (entryType == EntryTypeResponse) {
    return !conversation_entry_get_response(data->entry)->complete;
  }
  return false;
}

static void prv_layer_render(Layer* layer, GContext* ctx) {
  if (prv_has_border(layer)) {
    // Thoughts, actions, and errors get a box drawn around them
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_round_rect(ctx, layer_get_bounds(layer), 4);
  }
}

static void prv_format_time(time_t when, char* buffer, size_t size) {
  time_t midnight = time_start_of_today();

  char time_str[10];
  if (clock_is_24h_style()) {
    strftime(time_str, sizeof(time_str), "%H:%M", localtime(&when));
  } else {
    struct tm* ts = localtime(&when);
    int hour = ts->tm_hour % 12;
    if (hour == 0) {
      hour = 12;
    }
    snprintf(time_str, sizeof(time_str), "%d:%02d %s", hour, ts->tm_min, ts->tm_hour < 12 ? "AM" : "PM");
  }

  if (when < midnight + 86400) {
    snprintf(buffer, size, "today at %s", time_str);
  } else if (when < midnight + 86400 * 2) {
    snprintf(buffer, size, "tomorrow at %s", time_str);
  } else {
    char date_str[15];
    strftime(date_str, sizeof(date_str), "%a, %b %d", localtime(&when));
    snprintf(buffer, size, "%s at %s", date_str, time_str);
  }
}

static char* prv_generate_action_text(ConversationAction* action) {
  char* buffer = malloc(50);
  switch (action->type) {
    case ConversationActionTypeSetAlarm: {
      if (action->action.set_alarm.is_timer) {
        bool deleting = action->action.set_alarm.deleted;
        time_t now = time(NULL);
        int duration = action->action.set_alarm.time - now;
        int hours = duration / 3600;
        int minutes = (duration % 3600) / 60;
        int seconds = duration % 60;
        if (hours > 0) {
          snprintf(buffer, 50, deleting ? "Canceled a timer with %d:%02d:%02d remaining." : "Set a timer for %d:%02d:%02d.", hours, minutes, seconds);
          return buffer;
        } else {
          snprintf(buffer, 50, deleting ? "Canceled a timer with %d:%02d remaining." : "Set a timer for %d:%02d.", minutes, seconds);
          return buffer;
        }
      } else {
        const char *verb = action->action.set_alarm.deleted ? "Canceled" : "Set";
        char time_str[30];
        prv_format_time(action->action.set_alarm.time, time_str, sizeof(time_str));
        snprintf(buffer, 50, "%s an alarm for %s.", verb, time_str);
        return buffer;
      }
      break;
    }
    case ConversationActionTypeSetReminder: {
      char time_str[30];
      prv_format_time(action->action.set_reminder.time, time_str, sizeof(time_str));
      snprintf(buffer, 50, "Set a reminder for %s.", time_str);
      break;
    }
    case ConversationActionTypeUpdateChecklist:
      strncpy(buffer, "Updated your checklist.", 50);
      break;
    case ConversationActionTypeGenericSentence:
      strncpy(buffer, action->action.generic_sentence.sentence, 50);
      break;
  }
  return buffer;
}
