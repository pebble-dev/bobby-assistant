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

#include "info_layer.h"
#include <pebble.h>

#define CONTENT_FONT FONT_KEY_GOTHIC_24_BOLD
#define NAME_HEIGHT 15

typedef struct {
  ConversationEntry* entry;
  TextLayer* content_layer;
  uint16_t content_height;
  char* content_text; // For ConversationEntries without their own text, we need somewhere to stash what we generate.
} InfoLayerData;

static char *prv_get_content_text(InfoLayer *layer);
static int prv_get_content_height(InfoLayer* layer);
static void prv_layer_render(Layer* layer, GContext* ctx);
static char* prv_generate_action_text(ConversationAction* action);

InfoLayer* info_layer_create(GRect rect, ConversationEntry* entry) {
    Layer* layer = layer_create_with_data(rect, sizeof(InfoLayerData));
    InfoLayerData* data = layer_get_data(layer);
    data->entry = entry;
    data->content_text = NULL;
    data->content_height = 24;
    data->content_height = prv_get_content_height(layer);
    data->content_layer = text_layer_create(GRect(5, 1, rect.size.w - 10, data->content_height));
    text_layer_set_text(data->content_layer, prv_get_content_text(layer));
    text_layer_set_font(data->content_layer, fonts_get_system_font(CONTENT_FONT));
    text_layer_set_text_alignment(data->content_layer, GTextAlignmentCenter);
    data->content_height = text_layer_get_content_size(data->content_layer).h;
    layer_add_child(layer, (Layer *)data->content_layer);
    info_layer_update(layer);
    layer_set_update_proc(layer, prv_layer_render);
    return layer;
}

void info_layer_destroy(InfoLayer* layer) {
  InfoLayerData* data = layer_get_data(layer);
  text_layer_destroy(data->content_layer);
  if (data->content_text) {
    free(data->content_text);
  }
}

ConversationEntry* info_layer_get_entry(InfoLayer* layer) {
  InfoLayerData* data = layer_get_data(layer);
  return data->entry;
}

void info_layer_update(InfoLayer* layer) {
  InfoLayerData* data = layer_get_data(layer);
  // The text pointer can change out underneath us.
  text_layer_set_text(data->content_layer, prv_get_content_text(layer));
  data->content_height = prv_get_content_height(layer);
  int width = layer_get_bounds(layer).size.w - 10;
  GRect frame = layer_get_frame(layer);
  frame.size.h = data->content_height + 11;
  text_layer_set_size(data->content_layer, GSize(width, data->content_height + 5));
  layer_set_frame(layer, frame);
}

static char *prv_get_content_text(InfoLayer *layer) {
  InfoLayerData* data = layer_get_data(layer);
  ConversationEntry* entry = data->entry;
  switch(conversation_entry_get_type(entry)) {
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
    default:
      return "(Bobby bug)";
  }
}

static int prv_get_content_height(InfoLayer* layer) {
  // Measuring the whole field every time we add text is way too expensive.
  // Instead, just try to figure out where we are line breaking. We don't get enough information
  // from the text layout engine to do this in any particularly clever way, so instead we try
  // to figure out what's on the current line, and when that spills over to the next line.
  // Then we figure out (guess) exactly what spilled over and repeat from there.
  char* text = prv_get_content_text(layer);
  const GFont font = fonts_get_system_font(CONTENT_FONT);
  const GRect rect = GRect(0, 0, layer_get_frame(layer).size.w - 10, 10000);
  // This algorithm is somewhat buggy (it can't cope with words getting broken; it assumes only one break per
  // fragment), so for content where speed is less important just actually measure the thing.
  GTextAlignment alignment = GTextAlignmentCenter;

  return graphics_text_layout_get_content_size(text, font, rect, GTextOverflowModeTrailingEllipsis, alignment).h;
}

static void prv_layer_render(Layer* layer, GContext* ctx) {
  // Thoughts, actions, and errors get a box drawn around them
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_round_rect(ctx, layer_get_bounds(layer), 4);
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