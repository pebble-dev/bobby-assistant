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

#include "manager.h"
#include "../converse/conversation_manager.h"
#include "../util/persist_keys.h"

#include "alarm_window.h"

#include <pebble-events/pebble-events.h>
#include <pebble.h>

struct Alarm {
  time_t scheduled_time;
  WakeupId wakeup_id;
  char *name;
  bool is_timer; // what's the difference between an alarm and a timer? the user's intention.
};

struct AlarmManager {
  Alarm *pending_alarms;
  uint8_t pending_alarm_count;
  EventHandle app_message_handle;
};

AlarmManager s_manager;

static void prv_load_alarms();
static void prv_save_alarms();
static void prv_remove_alarm(int to_remove);
static void prv_handle_app_message_inbox_received(DictionaryIterator *iterator, void *context);
static void prv_send_alarm_response(StatusCode response);
static void prv_wakeup_handler(WakeupId wakeup_id, int32_t cookie);

#define MAX_ALARMS 8
#define ALARM_NAME_SIZE 32

void alarm_manager_init() {
  wakeup_service_subscribe(prv_wakeup_handler);
  s_manager.pending_alarms = NULL;
  s_manager.pending_alarm_count = 0;
  s_manager.app_message_handle = events_app_message_register_inbox_received(prv_handle_app_message_inbox_received, NULL);
  prv_load_alarms();
}

int alarm_manager_add_alarm(time_t when, bool is_timer, char* name, bool conversational) {
  if (s_manager.pending_alarm_count >= MAX_ALARMS) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Not scheduling alarm because MAX_ALARMS (%d) was already reached.", MAX_ALARMS);
    return E_OUT_OF_RESOURCES;
  }
  WakeupId id = wakeup_schedule(when, when, true);
  APP_LOG(APP_LOG_LEVEL_INFO, "wakeup_schedule(%d, %d, true) -> %d", when, when, id);
  if (id == E_RANGE) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Scheduling alarm failed: E_RANGE (there's another event already scheduled then)");
    return id;
  }
  if (id == E_INVALID_ARGUMENT) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Scheduling alarm failed: E_INVALID_ARGUMENT (the time is in the past)");
    return id;
  }
  if (id == E_OUT_OF_RESOURCES) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Scheduling alarm failed: E_OUT_OF_RESOURCES (already eight alarms scheduled)");
    return id;
  }
  if (id < 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Scheduling alarm failed: %d (Pebble internal error)");
    return id;
  }
  Alarm* alarm = malloc(sizeof(Alarm));
  alarm->scheduled_time = when;
  alarm->is_timer = is_timer;
  alarm->wakeup_id = id;
  alarm->name = NULL;
  size_t name_len = 0;
  if (name) {
    name_len = strlen(name);
    if (name_len > 0) {
      alarm->name = malloc(name_len + 1);
      strncpy(alarm->name, name, name_len + 1);
    }
  }

  if (conversational && conversation_manager_get_current()) {
    ConversationManager *conversation_manager = conversation_manager_get_current();
    if (alarm->is_timer) {
      // For timers, instead of the standard action item, we add a countdown widget.
      ConversationWidget widget = {
        .type = ConversationWidgetTypeTimer,
        .locally_created = true,
        .widget = {
          .timer = {
            .target_time = alarm->scheduled_time,
          }
        }
      };
      if (alarm->name) {
        widget.widget.timer.name = malloc(name_len + 1);
        strncpy(widget.widget.timer.name, alarm->name, name_len + 1);
      }
      conversation_manager_add_widget(conversation_manager, &widget);
    } else {
      ConversationAction action = {
        .type = ConversationActionTypeSetAlarm,
        .action = {
          .set_alarm = {
            .time = alarm->scheduled_time,
            .is_timer = alarm->is_timer,
            .deleted = false,
            .name = NULL,
          }
        }
      };
      if (name) {
        action.action.set_alarm.name = malloc(name_len + 1);
        strncpy(action.action.set_alarm.name, name, name_len + 1);
      }
      conversation_manager_add_action(conversation_manager, &action);
    }
  }

  ++s_manager.pending_alarm_count;
  if (s_manager.pending_alarm_count == 1) {
    s_manager.pending_alarms = alarm;
  } else {
    s_manager.pending_alarms = realloc(s_manager.pending_alarms, sizeof(Alarm) * s_manager.pending_alarm_count);
    s_manager.pending_alarms[s_manager.pending_alarm_count-1] = *alarm;
    free(alarm);
  }
  prv_save_alarms();
  return 0;
}

int alarm_manager_cancel_alarm(time_t when, bool is_timer) {
  for (int i = 0; i < s_manager.pending_alarm_count; ++i) {
    Alarm* alarm = &s_manager.pending_alarms[i];
    if (alarm->scheduled_time == when) {
      prv_remove_alarm(i);
      prv_save_alarms();
      return 0;
    }
  }
  return E_INVALID_ARGUMENT;
}

Alarm* alarm_manager_get_alarm(int index) {
  return &s_manager.pending_alarms[index];
}

int alarm_manager_get_alarm_count() {
  return s_manager.pending_alarm_count;
}

static void prv_load_alarms() {
  int alarm_count_one = persist_read_int(PERSIST_KEY_ALARM_COUNT_ONE);
  int alarm_count_two = persist_read_int(PERSIST_KEY_ALARM_COUNT_TWO);
  int alarm_count = alarm_count_one < alarm_count_two ? alarm_count_one : alarm_count_two;
  
  if (alarm_count == 0) {
    APP_LOG(APP_LOG_LEVEL_INFO, "No alarms known. Deleting all alarms to ensure consistency.");
    wakeup_cancel_all();
    return;
  }
  
  time_t times[MAX_ALARMS];
  WakeupId wakeup_ids[MAX_ALARMS];
  bool is_timers[MAX_ALARMS];
  char names[MAX_ALARMS][ALARM_NAME_SIZE];
  
  persist_read_data(PERSIST_KEY_ALARM_TIMES, &times, sizeof(times));
  persist_read_data(PERSIST_KEY_ALARM_WAKEUP_IDS, &wakeup_ids, sizeof(wakeup_ids));
  persist_read_data(PERSIST_KEY_ALARM_IS_TIMERS, &is_timers, sizeof(is_timers));
  persist_read_data(PERSIST_KEY_ALARM_NAMES, &names, sizeof(names));
  
  s_manager.pending_alarms = malloc(sizeof(Alarm) * alarm_count);
  s_manager.pending_alarm_count = alarm_count;
  
  bool did_drop_entries = false;
  
  WakeupId launch_id = 0;
  int32_t launch_cookie = 0;
  bool launched_for_wakeup = wakeup_get_launch_event(&launch_id, &launch_cookie);
  
  int j = 0;
  for (int i = 0; i < alarm_count; ++i) {
    if (!wakeup_query(wakeup_ids[i], NULL) && (!launched_for_wakeup || launch_id != wakeup_ids[i])) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Alarm %d (scheduled for %d) no longer exists; dropping.", wakeup_ids[i], times[i]);
      did_drop_entries = true;
      continue;
    }
    Alarm* alarm = &s_manager.pending_alarms[j++];
    alarm->scheduled_time = times[i];
    alarm->wakeup_id = wakeup_ids[i];
    alarm->is_timer = is_timers[i];
    size_t name_len = strlen(names[i]);
    if (name_len >= ALARM_NAME_SIZE) {
      name_len = ALARM_NAME_SIZE - 1;
    }
    if (name_len == 0) {
      alarm->name = NULL;
    } else {
      alarm->name = malloc(name_len + 1);
      strncpy(alarm->name, names[i], name_len + 1);
    }
  }
  s_manager.pending_alarm_count = j;
  
  if (did_drop_entries) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Updating saved data after dropping entries.");
    prv_save_alarms();
  }
}

static void prv_save_alarms() {
  if (s_manager.pending_alarm_count == 0) {
    APP_LOG(APP_LOG_LEVEL_INFO, "No alarms to save. Deleting everything.");
    persist_delete(PERSIST_KEY_ALARM_COUNT_ONE);
    persist_delete(PERSIST_KEY_ALARM_TIMES);
    persist_delete(PERSIST_KEY_ALARM_WAKEUP_IDS);
    persist_delete(PERSIST_KEY_ALARM_IS_TIMERS);
    persist_delete(PERSIST_KEY_ALARM_NAMES);
    persist_delete(PERSIST_KEY_ALARM_COUNT_TWO);
    wakeup_cancel_all();
  }
  time_t times[MAX_ALARMS];
  WakeupId wakeup_ids[MAX_ALARMS];
  bool is_timers[MAX_ALARMS];
  char names[MAX_ALARMS][ALARM_NAME_SIZE];
  memset(names, 0, sizeof(names));
  for (int i = 0; i < s_manager.pending_alarm_count; ++i) {
    Alarm* alarm = &s_manager.pending_alarms[i];
    times[i] = alarm->scheduled_time;
    wakeup_ids[i] = alarm->wakeup_id;
    is_timers[i] = alarm->is_timer;
    if (alarm->name) {
      strncpy(names[i], alarm->name, ALARM_NAME_SIZE);
      names[i][ALARM_NAME_SIZE - 1] = '\0';
    } else {
      names[i][0] = '\0';
    }
  }
  
  persist_write_int(PERSIST_KEY_ALARM_COUNT_ONE, s_manager.pending_alarm_count);
  persist_write_data(PERSIST_KEY_ALARM_TIMES, &times, sizeof(times));
  persist_write_data(PERSIST_KEY_ALARM_WAKEUP_IDS, &wakeup_ids, sizeof(wakeup_ids));
  persist_write_data(PERSIST_KEY_ALARM_IS_TIMERS, &is_timers, sizeof(is_timers));
  persist_write_data(PERSIST_KEY_ALARM_NAMES, &names, sizeof(names));
  persist_write_int(PERSIST_KEY_ALARM_COUNT_TWO, s_manager.pending_alarm_count);
  APP_LOG(APP_LOG_LEVEL_INFO, "Wrote %d alarms.", s_manager.pending_alarm_count);
}

static void prv_remove_alarm(int to_remove) {
  Alarm* alarm = &s_manager.pending_alarms[to_remove];
  wakeup_cancel(alarm->wakeup_id);

  // We don't want to add an entry for deleting something that is in the present or past.
  // Practically, this prevents us from adding entries when an alarm is dismissed during an active conversation.
  if (conversation_manager_get_current() && alarm->scheduled_time > time(NULL)) {
    ConversationManager *conversation_manager = conversation_manager_get_current();
    ConversationAction action = {
      .type = ConversationActionTypeSetAlarm,
      .action = {
        .set_alarm = {
          .time = alarm->scheduled_time,
          .is_timer = alarm->is_timer,
          .deleted = true,
          .name = NULL,
        }
      }
    };
    if (alarm->name) {
      size_t name_len = strlen(alarm->name);
      action.action.set_alarm.name = malloc(name_len + 1);
      strncpy(action.action.set_alarm.name, alarm->name, name_len + 1);
    }
    conversation_manager_add_action(conversation_manager, &action);
  }

  if (alarm->name) {
    free(alarm->name);
  }

  if (s_manager.pending_alarm_count == 1) {
    free(s_manager.pending_alarms);
    s_manager.pending_alarms = NULL;
    s_manager.pending_alarm_count = 0;
    return;
  }
  Alarm* new_alarms = malloc(sizeof(Alarm) * (s_manager.pending_alarm_count - 1));
  for (int i = 0, j = 0; i < s_manager.pending_alarm_count; ++i) {
    if (i == to_remove) {
      continue;
    }
    memcpy(&new_alarms[j], &s_manager.pending_alarms[i], sizeof(Alarm));
    ++j;
  }
  s_manager.pending_alarm_count--;
  free(s_manager.pending_alarms);
  s_manager.pending_alarms = new_alarms;
}

bool alarm_manager_maybe_alarm() {
  if (launch_reason() != APP_LAUNCH_WAKEUP) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Not launched by APP_LAUNCH_WAKEUP");
    return false;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Launched by APP_LAUNCH_WAKEUP");
  WakeupId id;
  int32_t cookie;
  if (!wakeup_get_launch_event(&id, &cookie)) {
    return false;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "WakeupId: %d, cookie: %d", id, cookie);
  for (int i = 0; i < s_manager.pending_alarm_count; ++i) {
    Alarm* alarm = &s_manager.pending_alarms[i];
    APP_LOG(APP_LOG_LEVEL_INFO, "comparing %d == %d", alarm->wakeup_id, id);
    if (alarm->wakeup_id == id) {
      APP_LOG(APP_LOG_LEVEL_INFO, "alarm found! alarming...");
      alarm_window_push(alarm->scheduled_time, alarm->is_timer, alarm->name);
      prv_remove_alarm(i);
      return true;
    }
  }
  return false;
}

time_t alarm_get_time(Alarm* alarm) {
  return alarm->scheduled_time;
}

bool alarm_is_timer(Alarm* alarm) {
  return alarm->is_timer;
}

char* alarm_get_name(Alarm* alarm) {
  return alarm->name;
}

static void prv_handle_set_alarm_request(DictionaryIterator *iterator, void *context) {
  Tuple* tuple = dict_find(iterator, MESSAGE_KEY_SET_ALARM_TIME);
  if (tuple == NULL) {
    return;
  }
  time_t alarm_time = tuple->value->int32;
  tuple = dict_find(iterator, MESSAGE_KEY_SET_ALARM_IS_TIMER);
  if (tuple == NULL) {
    prv_send_alarm_response(E_DOES_NOT_EXIST); // this is mismatched, but E_INVALID_ARGUMENT is taken.
    return;
  }
  bool is_timer = tuple->value->int16;
  if (is_timer) {
    alarm_time += time(NULL);
  }
  tuple = dict_find(iterator, MESSAGE_KEY_SET_ALARM_NAME);
  char* name = NULL;
  if (tuple != NULL && strlen(tuple->value->cstring) > 0) {
    name = tuple->value->cstring;
  }
  StatusCode result = alarm_manager_add_alarm(alarm_time, is_timer, name, true);
  prv_send_alarm_response(result);
  if (result == S_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Set alarm for %d (is timer: %d)", alarm_time, is_timer);
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Setting alarm for %d failed: %d", alarm_time, result);
  }
}

static void prv_handle_get_alarm_request(int16_t is_timer, void* context) {
  DictionaryIterator *iter;
  APP_LOG(APP_LOG_LEVEL_INFO, "Retrieving alarms or possibly timers (%d).", is_timer);
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Opening dict to respond failed: %d.", result);
    return;
  }
  int write_index = 0;
  for (int i = 0; i < s_manager.pending_alarm_count; ++i) {
    Alarm* alarm = &s_manager.pending_alarms[i];
    if (alarm->is_timer == is_timer) {
      ++write_index;
      dict_write_int32(iter, MESSAGE_KEY_GET_ALARM_RESULT + write_index, alarm->scheduled_time);
      dict_write_cstring(iter, MESSAGE_KEY_GET_ALARM_NAME + write_index, alarm->name);
    }
  }
  dict_write_int16(iter, MESSAGE_KEY_GET_ALARM_RESULT, write_index);
  dict_write_int32(iter, MESSAGE_KEY_CURRENT_TIME, time(NULL));
  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Sending alarm list to phone failed: %d.", result);
    return;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Sent alarm list.");
}

static void prv_handle_cancel_alarm_request(DictionaryIterator* iterator, void* context) {
  Tuple* tuple = dict_find(iterator, MESSAGE_KEY_CANCEL_ALARM_TIME);
  time_t target_time = tuple->value->int32;
  tuple = dict_find(iterator, MESSAGE_KEY_CANCEL_ALARM_IS_TIMER);
  int16_t is_timer = 0;
  if (tuple != NULL) {
    is_timer = tuple->value->int16;
  }
  if (target_time != 0) {
    int result = alarm_manager_cancel_alarm(target_time, is_timer);
    prv_send_alarm_response(result);
    return;
  }
  // if we don't have a target time we'll just delete the first one of whatever we find.
  for (int i = 0; i < s_manager.pending_alarm_count; ++i) {
    Alarm* alarm = &s_manager.pending_alarms[i];
    if (alarm->is_timer == is_timer) {
      prv_remove_alarm(i);
      // TODO: we should probably tell it which alarm we removed...
      prv_send_alarm_response(S_SUCCESS);
      return;
    }
  }
  // we've got nothing. return a failure.
  prv_send_alarm_response(E_INVALID_ARGUMENT);
}

static void prv_handle_app_message_inbox_received(DictionaryIterator *iterator, void *context) {
  Tuple* tuple = dict_find(iterator, MESSAGE_KEY_SET_ALARM_TIME);
  if (tuple != NULL) {
    prv_handle_set_alarm_request(iterator, context);
    return;
  }
  tuple = dict_find(iterator, MESSAGE_KEY_GET_ALARM_OR_TIMER);
  if (tuple != NULL) {
    prv_handle_get_alarm_request(tuple->value->int16, context);
  }
  tuple = dict_find(iterator, MESSAGE_KEY_CANCEL_ALARM_TIME);
  if (tuple != NULL) {
    prv_handle_cancel_alarm_request(iterator, context);
  }
}

static void prv_send_alarm_response(StatusCode response) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Returning status code %d to phone failed in open: %d.", response, result);
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_SET_ALARM_RESULT, response);
  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Returning status code %d to phone failed in send: %d.", response, result);
    return;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Sent alarm response %d", response);
}

static void prv_wakeup_handler(WakeupId wakeup_id, int32_t cookie) {
  APP_LOG(APP_LOG_LEVEL_INFO, "it's the wakeup handler! (%d, %d)", wakeup_id, cookie);
  for (int i = 0; i < s_manager.pending_alarm_count; ++i) {
    Alarm* alarm = &s_manager.pending_alarms[i];
    APP_LOG(APP_LOG_LEVEL_INFO, "comparing %d == %d", alarm->wakeup_id, wakeup_id);
    if (alarm->wakeup_id == wakeup_id) {
      APP_LOG(APP_LOG_LEVEL_INFO, "alarm found! alarming...");
      alarm_window_push(alarm->scheduled_time, alarm->is_timer, alarm->name);
      prv_remove_alarm(i);
      break;
    }
  }
}
