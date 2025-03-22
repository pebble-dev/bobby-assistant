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

#include "conversation.h"


struct ConversationEntry {
  EntryType type;
  union {
    ConversationAction *action;
    ConversationPrompt *prompt;
    ConversationResponse *response;
    ConversationThought *thought;
    ConversationWidget *widget;
    ConversationError *error;
  } content;
};

struct Conversation {
  ConversationEntry* entries;
  int entry_count;
  int entry_allocated;
  char thread_id[37];
};

static ConversationEntry* prv_create_entry(Conversation* conversation);

Conversation* conversation_create() {
  Conversation *conversation = malloc(sizeof(Conversation));
  conversation->entry_count = 0;
  conversation->entry_allocated = 3;
  conversation->entries = malloc(sizeof(ConversationEntry) * conversation->entry_allocated);
  conversation->thread_id[0] = 0;
  return conversation;
}

void conversation_destroy(Conversation* conversation) {
  for (int i = 0; i < conversation->entry_count; ++i) {
    ConversationEntry* entry = &conversation->entries[i];
    switch (entry->type) {
      case EntryTypePrompt:
        free(entry->content.prompt->prompt);
        free(entry->content.prompt);
        break;
      case EntryTypeResponse:
        free(entry->content.response->response);
        free(entry->content.response);
        break;
      case EntryTypeThought:
        free(entry->content.thought->thought);
        free(entry->content.thought);
        break;
      case EntryTypeError:
        free(entry->content.error->message);
        free(entry->content.error);
        break;
      case EntryTypeAction:
        // Only alarms and generic sentence actions need further cleanup.
        switch (entry->content.action->type) {
          case ConversationActionTypeSetAlarm:
            if (entry->content.action->action.set_alarm.name) {
              free(entry->content.action->action.set_alarm.name);
            }
            break;
          case ConversationActionTypeGenericSentence:
            free(entry->content.action->action.generic_sentence.sentence);
            break;
          case ConversationActionTypeSetReminder:
          case ConversationActionTypeDeleteReminder:
          case ConversationActionTypeSendFeedback:
          case ConversationActionTypeUpdateChecklist:
            // These have nothing to clean up.
            break;
        }
        free(entry->content.action);
        break;
      case EntryTypeWidget:
        switch (entry->content.widget->type) {
          case ConversationWidgetTypeWeatherSingleDay:
            free(entry->content.widget->widget.weather_single_day.location);
            free(entry->content.widget->widget.weather_single_day.summary);
            free(entry->content.widget->widget.weather_single_day.temp_unit);
            free(entry->content.widget->widget.weather_single_day.day);
            break;
          case ConversationWidgetTypeWeatherCurrent:
            free(entry->content.widget->widget.weather_current.location);
            free(entry->content.widget->widget.weather_current.summary);
            free(entry->content.widget->widget.weather_current.wind_speed_unit);
            break;
          case ConversationWidgetTypeWeatherMultiDay:
            free(entry->content.widget->widget.weather_multi_day.location);
            break;
          case ConversationWidgetTypeTimer:
            if (entry->content.widget->widget.timer.name) {
              free(entry->content.widget->widget.timer.name);
            }
            break;
          case ConversationWidgetTypeNumber:
            free(entry->content.widget->widget.number.number);
            if (entry->content.widget->widget.number.unit) {
              free(entry->content.widget->widget.number.unit);
            }
            break;
        }
        free(entry->content.widget);
        break;
    }
  }
  free(conversation->entries);
  free(conversation);
}

static ConversationEntry* prv_create_entry(Conversation* conversation) {
  if (conversation->entry_count >= conversation->entry_allocated) {
    ConversationEntry *new_entries = malloc(sizeof(ConversationEntry) * (conversation->entry_allocated + 1));
    memcpy(new_entries, conversation->entries, sizeof(ConversationEntry) * conversation->entry_allocated);
    ++conversation->entry_allocated;
    conversation->entries = new_entries;
  }
  return &conversation->entries[conversation->entry_count++];
}

void conversation_add_prompt(Conversation* conversation, const char* prompt_text) {
  ConversationEntry *entry = prv_create_entry(conversation);
  entry->type = EntryTypePrompt;
  ConversationPrompt *prompt = malloc(sizeof(ConversationPrompt));
  prompt->prompt = malloc(strlen(prompt_text) + 1);
  strcpy(prompt->prompt, prompt_text);
  entry->content.prompt = prompt;
}

void conversation_add_response(Conversation* conversation, const char* response_text) {
  ConversationEntry *entry = prv_create_entry(conversation);
  entry->type = EntryTypeResponse;
  ConversationResponse *response = malloc(sizeof(ConversationResponse));
  response->complete = true;
  response->len = strlen(response_text);
  response->allocated = response->len + 1;
  response->response = malloc(response->allocated);
  strcpy(response->response, response_text);
  entry->content.response = response;
}

void conversation_start_response(Conversation *conversation) {
  ConversationEntry *entry = prv_create_entry(conversation);
  entry->type = EntryTypeResponse;
  ConversationResponse *response = malloc(sizeof(ConversationResponse));
  response->complete = false;
  response->len = 0;
  response->allocated = 8;
  response->response = malloc(response->allocated);
  response->response[0] = 0;
  entry->content.response = response;
}

static void prv_append_to_response(ConversationResponse *response, const char* fragment) {
  size_t len = strlen(fragment);
  while (response->len + len >= response->allocated) {
    response->allocated *= 2;
    char* new_resp = malloc(response->allocated);
    APP_LOG(APP_LOG_LEVEL_INFO, "Expanding buffer to %d bytes. New buffer: %p. Old buffer: %p.", response->allocated, new_resp, response->response);
    strcpy(new_resp, response->response);
    APP_LOG(APP_LOG_LEVEL_INFO, "Copied %d bytes.", strlen(response->response) + 1);
    free(response->response);
    response->response = new_resp;
  }
  response->len += len;
  strcat(response->response, fragment);
}

static ConversationResponse* prv_find_last_open_response(Conversation* conversation) {
  for (int i = conversation->entry_count - 1; i >= 0; --i) {
    ConversationEntry* entry = &conversation->entries[i];
    if (entry->type != EntryTypeResponse) {
      continue;
    }
    if (!entry->content.response->complete) {
      return entry->content.response;
    }
  }
  return NULL;
}

bool conversation_add_response_fragment(Conversation* conversation, const char* fragment) {
  ConversationResponse* response = prv_find_last_open_response(conversation);
  bool added_entry = false;
  if (response == NULL) {
    added_entry = true;
    conversation_start_response(conversation);
    response = prv_find_last_open_response(conversation);
  }
  prv_append_to_response(response, fragment);
  return added_entry;
}

void conversation_complete_response(Conversation *conversation) {
  ConversationResponse* response = prv_find_last_open_response(conversation);
  if (response == NULL) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Trying to complete a response, but couldn't find any.");
    return;
  }
  response->complete = true;
}

void conversation_add_thought(Conversation* conversation, char* thought_text) {
  ConversationEntry* entry = prv_create_entry(conversation);
  ConversationThought* thought = malloc(sizeof(ConversationThought));
  thought->thought = malloc(strlen(thought_text) + 1);
  strcpy(thought->thought, thought_text);
  entry->type = EntryTypeThought;
  entry->content.thought = thought;
}

void conversation_add_action(Conversation* conversation, ConversationAction* action) {
  ConversationAction* new_action = malloc(sizeof(ConversationAction));
  memcpy(new_action, action, sizeof(ConversationAction));
  ConversationEntry* entry = prv_create_entry(conversation);
  entry->type = EntryTypeAction;
  entry->content.action = new_action;
}

void conversation_add_error(Conversation* conversation, const char* error_text) {
  ConversationEntry* entry = prv_create_entry(conversation);
  ConversationError* error = malloc(sizeof(ConversationError));
  error->message = malloc(strlen(error_text) + 1);
  strcpy(error->message, error_text);
  entry->type = EntryTypeError;
  entry->content.error = error;
}

void conversation_add_widget(Conversation* conversation, ConversationWidget* widget) {
  ConversationWidget* new_widget = malloc(sizeof(ConversationWidget));
  memcpy(new_widget, widget, sizeof(ConversationWidget));
  ConversationEntry* entry = prv_create_entry(conversation);
  entry->type = EntryTypeWidget;
  entry->content.widget = new_widget;
}

ConversationEntry* conversation_entry_at_index(Conversation* conversation, int index) {
  if (index >= conversation->entry_count) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Caller asked for entry %d, but only %d exist.", index, conversation->entry_count);
    return NULL;
  }
  return &conversation->entries[index];
}

ConversationEntry* conversation_peek(Conversation* conversation) {
  if (conversation->entry_count == 0) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Tried to peek at conversation, but no entries yet.");
    return NULL;
  }
  return &conversation->entries[conversation->entry_count-1];
}

ConversationEntry* conversation_get_last_of_type(Conversation* conversation, EntryType type) {
  for (int i = conversation->entry_count - 1; i >= 0; --i) {
    ConversationEntry* entry = &conversation->entries[i];
    if (entry->type == type) {
      return entry;
    }
  }
  return NULL;
}

const char* prv_type_to_string(EntryType type) {
  switch(type) {
    case EntryTypePrompt:
      return "EntryTypePrompt";
    case EntryTypeResponse:
      return "EntryTypeResponse";  
    case EntryTypeThought:
      return "EntryTypeThought";
    case EntryTypeAction:
      return "EntryTypeAction";
    case EntryTypeError:
      return "EntryTypeError";
    case EntryTypeWidget:
      return "EntryTypeWidget";
  }
  return "Unknown";
}

ConversationPrompt* conversation_entry_get_prompt(ConversationEntry* entry) {
  if (entry->type != EntryTypePrompt) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Asked for prompt %p, but it's actually a %s.", entry, prv_type_to_string(entry->type));
    return NULL;
  }
  return entry->content.prompt;
}

ConversationResponse* conversation_entry_get_response(ConversationEntry* entry) {
  if (entry->type != EntryTypeResponse) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Asked for response %p, but it's actually a %s.", entry, prv_type_to_string(entry->type));
    return NULL;
  }
  return entry->content.response;
}

ConversationThought* conversation_entry_get_thought(ConversationEntry* entry) {
  if (entry->type != EntryTypeThought) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Asked for thought %p, but it's actually a %s.", entry, prv_type_to_string(entry->type));
    return NULL;
  }
  return entry->content.thought;
}

ConversationError* conversation_entry_get_error(ConversationEntry* entry) {
  if (entry->type != EntryTypeError) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Asked for error %p, but it's actually a %s.", entry, prv_type_to_string(entry->type));
    return NULL;
  }
  return entry->content.error;
}

ConversationAction* conversation_entry_get_action(ConversationEntry* action) {
  if (action->type != EntryTypeAction) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Asked for action %p, but it's actually a %s.", action, prv_type_to_string(action->type));
    return NULL;
  }
  return action->content.action;
}

ConversationWidget* conversation_entry_get_widget(ConversationEntry* widget) {
  if (widget->type != EntryTypeWidget) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Asked for widget %p, but it's actually a %s.", widget, prv_type_to_string(widget->type));
    return NULL;
  }
  return widget->content.widget;
}

EntryType conversation_entry_get_type(ConversationEntry* entry) {
  return entry->type;
}

void conversation_set_thread_id(Conversation* conversation, const char* thread_id) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Thread ID updated: %s", thread_id);
  strncpy(conversation->thread_id, thread_id, 37);
}

const char* conversation_get_thread_id(Conversation* conversation) {
  return conversation->thread_id;
}

bool conversation_is_idle(Conversation* conversation) {
  ConversationEntry* entry = conversation_peek(conversation);
  if (entry == NULL) {
    return true;
  }
  if (entry->type == EntryTypeError) {
    return true;
  }
  if (entry->type == EntryTypeResponse) {
    return entry->content.response->complete;
  }
  if (entry->type == EntryTypeWidget) {
    return true;
  }
  return false;
}

static bool prv_entry_type_is_assistant(ConversationEntry* entry) {
  switch (entry->type) {
    case EntryTypePrompt:
    case EntryTypeError:
    case EntryTypeThought:
    case EntryTypeAction:
      return false;
    case EntryTypeResponse:
      return true;
    case EntryTypeWidget:
      return !entry->content.widget->locally_created;
  }
  return false;
}

bool conversation_assistant_just_started(Conversation* conversation) {
  ConversationEntry *entry = conversation_peek(conversation);
  if (entry == NULL) {
    return false;
  }
  if (!prv_entry_type_is_assistant(entry)) {
    return false;
  }
  if (conversation->entry_count == 1) {
    return true;
  }
  ConversationEntry *previous = &conversation->entries[conversation->entry_count - 2];
  return !prv_entry_type_is_assistant(previous);
}
