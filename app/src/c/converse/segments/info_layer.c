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

#define CONTENT_FONT FONT_KEY_GOTHIC_24
#define STRIPE_WIDTH 24
#define TEXT_PADDING_LEFT 2
#define TEXT_PADDING_RIGHT 5
#define UNAVAILABLE_WIDTH (STRIPE_WIDTH + TEXT_PADDING_LEFT + TEXT_PADDING_RIGHT)

typedef struct {
  ConversationEntry* entry;
  GDrawCommandImage* icon;
  uint32_t icon_resource;
  TextLayer* content_layer;
  uint16_t content_height;
  char* content_text; // For ConversationEntries without their own text, we need somewhere to stash what we generate.
} InfoLayerData;

static char *prv_get_content_text(InfoLayer *layer);
static int prv_get_content_height(InfoLayer* layer);
static void prv_layer_render(Layer* layer, GContext* ctx);
static char* prv_generate_action_text(ConversationAction* action);
static uint32_t prv_get_icon_resource(ConversationEntry* entry);
static GColor prv_get_stripe_color(ConversationEntry* entry);

InfoLayer* info_layer_create(GRect rect, ConversationEntry* entry) {
    Layer* layer = layer_create_with_data(rect, sizeof(InfoLayerData));
    InfoLayerData* data = layer_get_data(layer);
    data->entry = entry;
    data->icon = NULL;
    data->content_text = NULL;
    data->content_height = 24;
    data->content_height = prv_get_content_height(layer);
    data->content_layer = text_layer_create(GRect(STRIPE_WIDTH + TEXT_PADDING_LEFT, 1, rect.size.w - UNAVAILABLE_WIDTH, data->content_height));
    text_layer_set_text(data->content_layer, prv_get_content_text(layer));
    text_layer_set_font(data->content_layer, fonts_get_system_font(CONTENT_FONT));
    text_layer_set_background_color(data->content_layer, GColorClear);
    text_layer_set_text_alignment(data->content_layer, GTextAlignmentLeft);
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
  if (data->icon) {
    gdraw_command_image_destroy(data->icon);
  }
  layer_destroy(layer);
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
  int width = layer_get_bounds((Layer *)data->content_layer).size.w;
  GRect frame = layer_get_frame(layer);
  frame.size.h = data->content_height + 11;
  text_layer_set_size(data->content_layer, GSize(width, data->content_height + 5));
  layer_set_frame(layer, frame);
  uint32_t new_resource = prv_get_icon_resource(data->entry);
  if (new_resource != data->icon_resource) {
    if (data->icon) {
      gdraw_command_image_destroy(data->icon);
    }
    data->icon = gdraw_command_image_create_with_resource(new_resource);
    data->icon_resource = new_resource;
  }
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
  char* text = prv_get_content_text(layer);
  const GFont font = fonts_get_system_font(CONTENT_FONT);
  const GRect rect = GRect(0, 0, layer_get_frame(layer).size.w - UNAVAILABLE_WIDTH, 10000);
  GTextAlignment alignment = GTextAlignmentCenter;
  return graphics_text_layout_get_content_size(text, font, rect, GTextOverflowModeTrailingEllipsis, alignment).h;
}

static void prv_layer_render(Layer* layer, GContext* ctx) {
  InfoLayerData* data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  GColor stripe_color = prv_get_stripe_color(data->entry);
  graphics_context_set_fill_color(ctx, stripe_color);
  graphics_fill_rect(ctx, GRect(0, 0, STRIPE_WIDTH, bounds.size.h), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(bounds.size.w, 0));
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
  if (data->icon) {
    gdraw_command_image_draw(ctx, data->icon, GPoint(3, 10));
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
          snprintf(buffer, 50, deleting ? "Timer canceled with %d:%02d:%02d remaining." : "Timer set for %d:%02d:%02d.", hours, minutes, seconds);
          return buffer;
        } else {
          snprintf(buffer, 50, deleting ? "Timer canceled with %d:%02d remaining." : "Timer set for %d:%02d.", minutes, seconds);
          return buffer;
        }
      } else {
        const char *verb = action->action.set_alarm.deleted ? "canceled" : "set";
        char time_str[30];
        prv_format_time(action->action.set_alarm.time, time_str, sizeof(time_str));
        snprintf(buffer, 50, "Alarm %s for %s.", verb, time_str);
        return buffer;
      }
      break;
    }
    case ConversationActionTypeSetReminder: {
      char time_str[30];
      prv_format_time(action->action.set_reminder.time, time_str, sizeof(time_str));
      snprintf(buffer, 50, "Reminder set for %s.", time_str);
      break;
    }
    case ConversationActionTypeDeleteReminder:
      strncpy(buffer, "Reminder deleted.", 50);
      break;
    case ConversationActionTypeUpdateChecklist:
      strncpy(buffer, "Checklist updated.", 50);
      break;
    case ConversationActionTypeGenericSentence:
      strncpy(buffer, action->action.generic_sentence.sentence, 50);
      break;
  }
  return buffer;
}

static uint32_t prv_get_icon_resource(ConversationEntry* entry) {
  switch(conversation_entry_get_type(entry)) {
    case EntryTypeThought:
      return RESOURCE_ID_LIGHTBULB_ICON;
    case EntryTypeError:
      return RESOURCE_ID_SKULL_ICON;
    case EntryTypeAction: {
      ConversationAction* action = conversation_entry_get_action(entry);
      switch (action->type) {
        case ConversationActionTypeSetAlarm:
          if (action->action.set_alarm.is_timer) {
            return RESOURCE_ID_TIMER_ICON;
          }
        return RESOURCE_ID_CLOCK_ICON;
        case ConversationActionTypeSetReminder:
        case ConversationActionTypeDeleteReminder:
          return RESOURCE_ID_REMINDER_ICON;
        default:
          return RESOURCE_ID_COG_ICON;
      }
    }
    default:
      return RESOURCE_ID_COG_ICON;
  }
}

static GColor prv_get_stripe_color(ConversationEntry* entry) {
#if defined(PBL_COLOR)
  switch (conversation_entry_get_type(entry)) {
    case EntryTypeThought:
      return GColorYellow;
    case EntryTypeError:
      return GColorRed;
    case EntryTypeAction:
      return GColorShockingPink;
    default:
      return GColorOrange;
  }
#else
  return GColorDarkGray;
#endif
}