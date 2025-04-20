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

#include "conversation_manager.h"

#include "conversation.h"
#include "../util/memory/malloc.h"
#include "../util/memory/pressure.h"
#include "../util/logging.h"
#include "../util/strings.h"

#include <pebble-events/pebble-events.h>
#include <pebble.h>


struct ConversationManager {
  Conversation* conversation;
  EventHandle app_message_handle;
  void* context;
  ConversationManagerUpdateHandler handler;
  ConversationManagerEntryDeletedHandler deletion_handler;
};

static void prv_conversation_updated(ConversationManager* manager, bool new_entry);
static void prv_handle_app_message_outbox_sent(DictionaryIterator *iterator, void *context);
static void prv_handle_app_message_outbox_failed(DictionaryIterator *iterator, AppMessageResult reason, void *context);
static void prv_handle_app_message_inbox_received(DictionaryIterator *iterator, void *context);
static void prv_handle_app_message_inbox_dropped(AppMessageResult result, void *context);
static void prv_process_weather_widget(int widget_type, DictionaryIterator *iter, ConversationManager *manager);
static void prv_process_timer_widget(int widget_type, DictionaryIterator *iter, ConversationManager *manager);
static void prv_process_highlight_widget(int widget_type, DictionaryIterator *iter, ConversationManager *manager);
#if ENABLE_FEATURE_MAPS
static void prv_process_map_widget(int widget_type, DictionaryIterator *iter, ConversationManager *manager);
#endif
static bool prv_handle_memory_pressure(void *context);

static ConversationManager* s_conversation_manager;

void conversation_manager_init() {
  events_app_message_request_outbox_size(1024);
  events_app_message_request_inbox_size(1024);
}

ConversationManager* conversation_manager_create() {
  ConversationManager* manager = bmalloc(sizeof(ConversationManager));
  manager->conversation = conversation_create();
  manager->handler = NULL;
  manager->app_message_handle = events_app_message_subscribe_handlers((EventAppMessageHandlers){
      .sent = prv_handle_app_message_outbox_sent,
      .failed = prv_handle_app_message_outbox_failed,
      .received = prv_handle_app_message_inbox_received,
      // We don't handle this elegantly enough for it to make sense here.
      // .dropped = prv_handle_app_message_inbox_dropped,
  }, manager);
  s_conversation_manager = manager;
  memory_pressure_register_callback(prv_handle_memory_pressure, 1, manager);
  return manager;
}

void conversation_manager_destroy(ConversationManager* manager) {
  conversation_destroy(manager->conversation);
  events_app_message_unsubscribe(manager->app_message_handle);
  if (s_conversation_manager == manager) {
    s_conversation_manager = NULL;
  }
  free(manager);
}

ConversationManager* conversation_manager_get_current() {
  return s_conversation_manager;
}

Conversation* conversation_manager_get_conversation(ConversationManager* manager) {
  return manager->conversation;
}

void conversation_manager_set_handler(ConversationManager* manager, ConversationManagerUpdateHandler handler, void* context) {
  manager->handler = handler;
  manager->context = context;
}

void conversation_manager_set_deletion_handler(ConversationManager* manager, ConversationManagerEntryDeletedHandler handler) {
  manager->deletion_handler = handler;
}

void conversation_manager_add_input(ConversationManager* manager, const char* input) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  conversation_add_prompt(manager->conversation, input);
  prv_conversation_updated(manager, true);
  if (result != APP_MSG_OK) {
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Preparing outbox failed: %d.", result);
    conversation_add_error(manager->conversation, "Sending to service failed.");
    prv_conversation_updated(manager, true);
    return;
  }

  // The Android Pebble app has a fun bug where any double-quotes in a
  // message will cause it to be dropped, this is a bodge workaround.
  char* bridge_bodge = bmalloc(strlen(input) + 1);
  strcpy(bridge_bodge, input);
  strings_fix_android_bridge_bodge(bridge_bodge);
  dict_write_cstring(iter, MESSAGE_KEY_PROMPT, bridge_bodge);
  free(bridge_bodge);

  const char* thread_id = conversation_get_thread_id(manager->conversation);
  if (thread_id[0] != 0) {
    BOBBY_LOG(APP_LOG_LEVEL_INFO, "Continuing previous conversation %s.", thread_id);
    dict_write_cstring(iter, MESSAGE_KEY_THREAD_ID, thread_id);
  }
  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Sending message failed: %d.", result);
    conversation_add_error(manager->conversation, "Sending to service failed.");
    prv_conversation_updated(manager, true);
    return;
  }
}

void conversation_manager_add_action(ConversationManager* manager, ConversationAction* action) {
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Adding action to conversation.");
  conversation_add_action(manager->conversation, action);
  prv_conversation_updated(manager, true);
}

void conversation_manager_add_widget(ConversationManager* manager, ConversationWidget* widget) {
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Adding widget to conversation.");
  conversation_add_widget(manager->conversation, widget);
  prv_conversation_updated(manager, true);
}

static void prv_handle_app_message_outbox_sent(DictionaryIterator *iterator, void *context) {
  BOBBY_LOG(APP_LOG_LEVEL_INFO, "Sent message successfully.");
}

static void prv_handle_app_message_outbox_failed(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Sending message failed: %d", reason);
  ConversationManager* manager = context;
  conversation_add_error(manager->conversation, "Sending to service failed.");
  prv_conversation_updated(manager, true);
}

static void prv_handle_app_message_inbox_received(DictionaryIterator *iter, void *context) {
  ConversationManager* manager = context;
  for (Tuple *tuple = dict_read_first(iter); tuple; tuple = dict_read_next(iter)) {
    if (tuple->key == MESSAGE_KEY_CHAT) {
      bool added_entry = conversation_add_response_fragment(manager->conversation, tuple->value->cstring);
      prv_conversation_updated(manager, added_entry);
    } else if (tuple->key == MESSAGE_KEY_FUNCTION) {
      BOBBY_LOG(APP_LOG_LEVEL_INFO, "Received function: \"%s\".", tuple->value->cstring);
      conversation_complete_response(manager->conversation);
      prv_conversation_updated(manager, false);
      conversation_add_thought(manager->conversation, tuple->value->cstring);
      prv_conversation_updated(manager, true);
    } else if (tuple->key == MESSAGE_KEY_CHAT_DONE) {
      conversation_complete_response(manager->conversation);
      prv_conversation_updated(manager, false);
    } else if (tuple->key == MESSAGE_KEY_THREAD_ID) {
      conversation_set_thread_id(manager->conversation, tuple->value->cstring);
    } else if (tuple->key == MESSAGE_KEY_CLOSE_WAS_CLEAN) {
      if (!tuple->value->int16) {
        conversation_complete_response(manager->conversation);
        conversation_add_error(manager->conversation, "Lost connection to server.");
        prv_conversation_updated(manager, true);
      }
    } else if (tuple->key == MESSAGE_KEY_CLOSE_REASON) {
      if (tuple->value->cstring[0] != 0) {
        conversation_complete_response(manager->conversation);
        conversation_add_error(manager->conversation, tuple->value->cstring);
        prv_conversation_updated(manager, true);
      }
    } else if (tuple->key == MESSAGE_KEY_ACTION_REMINDER_WAS_SET) {
      // Setting reminders is handled by the phone, so we don't have any logic here for it.
      // We pick this up here so we can add a note about it to the session view.
      ConversationAction action = {
        .type = ConversationActionTypeSetReminder,
        .action = {
          .set_reminder = {
            .time = tuple->value->int32,
          },
        },
      };
      conversation_manager_add_action(manager, &action);
    } else if (tuple->key == MESSAGE_KEY_ACTION_REMINDER_DELETED) {
      ConversationAction action = {
        .type = ConversationActionTypeDeleteReminder,
        .action = {},
      };
      conversation_manager_add_action(manager, &action);
    } else if (tuple->key == MESSAGE_KEY_ACTION_FEEDBACK_SENT) {
      ConversationAction action = {
        .type = ConversationActionTypeSendFeedback,
        .action = {}
      };
      conversation_manager_add_action(manager, &action);
    } else if (tuple->key == MESSAGE_KEY_WARNING) {
      conversation_complete_response(manager->conversation);
      prv_conversation_updated(manager, false);
      conversation_add_error(manager->conversation, tuple->value->cstring);
      prv_conversation_updated(manager, true);
    } else if (tuple->key == MESSAGE_KEY_WEATHER_WIDGET) {
      conversation_complete_response(manager->conversation);
      prv_conversation_updated(manager, false);
      prv_process_weather_widget(tuple->value->int32, iter, manager);
    } else if (tuple->key == MESSAGE_KEY_TIMER_WIDGET) {
      conversation_complete_response(manager->conversation);
      prv_conversation_updated(manager, false);
      prv_process_timer_widget(tuple->value->int32, iter, manager);
    } else if (tuple->key == MESSAGE_KEY_HIGHLIGHT_WIDGET) {
      conversation_complete_response(manager->conversation);
      prv_conversation_updated(manager, false);
      prv_process_highlight_widget(tuple->value->int32, iter, manager);
#if ENABLE_FEATURE_MAPS
    } else if (tuple->key == MESSAGE_KEY_MAP_WIDGET) {
      conversation_complete_response(manager->conversation);
      prv_conversation_updated(manager, false);
      prv_process_map_widget(tuple->value->int32, iter, manager);
#endif
    }
  }
}

static void prv_process_weather_widget(int widget_type, DictionaryIterator *iter, ConversationManager *manager) {
  switch (widget_type) {
    case 1: {
      int high = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_DAY_HIGH)->value->int32;
      int low = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_DAY_LOW)->value->int32;
      int icon = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_DAY_ICON)->value->int32;
      const char* summary = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_DAY_SUMMARY)->value->cstring;
      const char* location = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_LOCATION)->value->cstring;
      const char* temp_unit = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_TEMP_UNIT)->value->cstring;
      const char* day = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_DAY_OF_WEEK)->value->cstring;
      char* summary_stored = bmalloc(strlen(summary) + 1);
      strcpy(summary_stored, summary);
      char* location_stored = bmalloc(strlen(location) + 1);
      strcpy(location_stored, location);
      char* temp_unit_stored = bmalloc(strlen(temp_unit) + 1);
      strcpy(temp_unit_stored, temp_unit);
      char* day_stored = bmalloc(strlen(day) + 1);
      strcpy(day_stored, day);
      ConversationWidget widget = {
        .type = ConversationWidgetTypeWeatherSingleDay,
        .widget = {
          .weather_single_day = {
            .high = high,
            .low = low,
            .condition = icon,
            .location = location_stored,
            .summary = summary_stored,
            .temp_unit = temp_unit_stored,
            .day = day_stored,
          }
        }
      };
      conversation_add_widget(manager->conversation, &widget);
      prv_conversation_updated(manager, true);
      break;
    }
    case 2: {
      int temp = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_CURRENT_TEMP)->value->int32;
      int feels_like = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_FEELS_LIKE)->value->int32;
      int icon = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_DAY_ICON)->value->int32;
      int wind_speed = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_WIND_SPEED)->value->int32;
      const char* location = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_LOCATION)->value->cstring;
      const char* summary = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_DAY_SUMMARY)->value->cstring;
      const char* wind_speed_unit = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_WIND_SPEED_UNIT)->value->cstring;
      char* location_stored = bmalloc(strlen(location) + 1);
      strcpy(location_stored, location);
      char* summary_stored = bmalloc(strlen(summary) + 1);
      strcpy(summary_stored, summary);
      char* wind_speed_unit_stored = bmalloc(strlen(wind_speed_unit) + 1);
      strcpy(wind_speed_unit_stored, wind_speed_unit);
      ConversationWidget widget = {
        .type = ConversationWidgetTypeWeatherCurrent,
        .widget = {
          .weather_current = {
            .temperature = temp,
            .feels_like = feels_like,
            .condition = icon,
            .wind_speed = wind_speed,
            .location = location_stored,
            .summary = summary_stored,
            .wind_speed_unit = wind_speed_unit_stored,
          }
        }
      };
      conversation_add_widget(manager->conversation, &widget);
      prv_conversation_updated(manager, true);
      break;
    }
    case 3: {
      const char* location = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_LOCATION)->value->cstring;
      char *location_stored = bmalloc(strlen(location) + 1);
      strcpy(location_stored, location);
      ConversationWidget widget = {
        .type = ConversationWidgetTypeWeatherMultiDay,
        .widget = {
          .weather_multi_day = {
            .location = location_stored,
          }
        }
      };
      for (int i = 0; i < 3; ++i) {
        ConversationWidgetWeatherMultiDaySegment *s = &widget.widget.weather_multi_day.days[i];
        s->high = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_MULTI_HIGH + i)->value->int32;
        s->low = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_MULTI_LOW + i)->value->int32;
        s->condition = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_MULTI_ICON + i)->value->int32;
        const char* day = dict_find(iter, MESSAGE_KEY_WEATHER_WIDGET_MULTI_DAY + i)->value->cstring;
        strncpy(s->day, day, sizeof(s->day));
        s->day[sizeof(s->day) - 1] = '\0';
      }
      conversation_add_widget(manager->conversation, &widget);
      prv_conversation_updated(manager, true);
      break;
    }
  }
}

static void prv_process_timer_widget(int widget_type, DictionaryIterator *iter, ConversationManager *manager) {
  time_t target_time = dict_find(iter, MESSAGE_KEY_TIMER_WIDGET_TARGET_TIME)->value->int32;
  char *name_stored = NULL;
  Tuple *tuple = dict_find(iter, MESSAGE_KEY_TIMER_WIDGET_NAME);
  if (tuple) {
    const char *name = tuple->value->cstring;
    name_stored = bmalloc(strlen(name) + 1);
    strcpy(name_stored, name);
  }
  ConversationWidget widget = {
    .type = ConversationWidgetTypeTimer,
    .widget = {
      .timer = {
        .target_time = target_time,
        .name = name_stored,
      }
    }
  };
  conversation_add_widget(manager->conversation, &widget);
  prv_conversation_updated(manager, true);
}

static void prv_process_highlight_widget(int widget_type, DictionaryIterator *iter, ConversationManager *manager) {
  if (widget_type != 1) {
    return;
  }
  char *number = dict_find(iter, MESSAGE_KEY_HIGHLIGHT_WIDGET_PRIMARY)->value->cstring;
  char *number_stored = bmalloc(strlen(number) + 1);
  strcpy(number_stored, number);
  char *units_stored = NULL;
  Tuple *tuple = dict_find(iter, MESSAGE_KEY_HIGHLIGHT_WIDGET_SECONDARY);
  if (tuple) {
    const char *units = tuple->value->cstring;
    units_stored = bmalloc(strlen(units) + 1);
    strcpy(units_stored, units);
  }
  ConversationWidget widget = {
    .type = ConversationWidgetTypeNumber,
    .widget = {
      .number = {
        .number = number_stored,
        .unit = units_stored,
      }
    }
  };
  conversation_add_widget(manager->conversation, &widget);
  prv_conversation_updated(manager, true);
}

#if ENABLE_FEATURE_MAPS
static void prv_process_map_widget(int widget_type, DictionaryIterator *iter, ConversationManager *manager) {
  if (widget_type != 1) {
    return;
  }
  int image_id = dict_find(iter, MESSAGE_KEY_MAP_WIDGET_IMAGE_ID)->value->int32;
  int user_location = dict_find(iter, MESSAGE_KEY_MAP_WIDGET_USER_LOCATION)->value->int32;
  ConversationWidget widget = {
    .type = ConversationWidgetTypeMap,
    .widget = {
      .map = {
        .image_id = image_id,
        .user_location = GPoint(user_location >> 16, user_location & 0xFFFF),
      }
    }
  };
  conversation_add_widget(manager->conversation, &widget);
  prv_conversation_updated(manager, true);
}
#endif

static void prv_handle_app_message_inbox_dropped(AppMessageResult reason, void *context) {
  BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Received message dropped: %d", reason);
  ConversationManager* manager = context;
  conversation_add_error(manager->conversation, "Response from service lost.");
  prv_conversation_updated(manager, true);
}

static void prv_conversation_updated(ConversationManager* manager, bool new_entry) {
  if (manager->handler) {
    manager->handler(new_entry, manager->context);
  }
}

static bool prv_handle_memory_pressure(void *context) {
  BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Memory pressure detected.");
  ConversationManager* manager = context;
  if (!manager->conversation) {
    return false;
  }
  if (conversation_length(manager->conversation) <= 2) {
    return false;
  }
  BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Deleting oldest entry from conversation.");
  if (manager->deletion_handler) {
    manager->deletion_handler(0, manager->context);
  }
  conversation_delete_first_entry(manager->conversation);
  return true;
}