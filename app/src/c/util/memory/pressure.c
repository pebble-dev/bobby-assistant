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

#include "pressure.h"
#include <pebble.h>

#include <@smallstoneapps/linked-list/linked-list.h>

static LinkedRoot *s_callback_list = NULL;
int s_max_priority = 0;

typedef struct {
  MemoryPressureHandler handler;
  int priority;
  void *context;
} MemoryPressureCallbackEntry;

static bool prv_entry_compare(void *object1, void *object2);
static void prv_update_max_priority();
static bool prv_max_priority_callback(void *object, void *context);

void memory_pressure_init() {
  s_callback_list = linked_list_create_root();
}

void memory_pressure_deinit() {
  linked_list_clear(s_callback_list);
}

void memory_pressure_register_callback(MemoryPressureHandler handler, int priority, void *context) {
  MemoryPressureCallbackEntry *entry = malloc(sizeof(MemoryPressureCallbackEntry));
  entry->handler = handler;
  entry->priority = priority;
  entry->context = context;
  linked_list_append(s_callback_list, entry);
  if (entry->priority > s_max_priority) {
    s_max_priority = entry->priority;
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "memory_pressure_register_callback: %p, priority %d", handler, priority);
}

void memory_pressure_unregister_callback(MemoryPressureHandler handler) {
  int idx = linked_list_find_compare(s_callback_list, handler, prv_entry_compare);
  if (idx == -1) {
    return;
  }
  MemoryPressureCallbackEntry *entry = linked_list_get(s_callback_list, idx);
  linked_list_remove(s_callback_list, idx);
  if (entry->priority == s_max_priority) {
    prv_update_max_priority();
  }
  free(entry);
}

bool memory_pressure_try_free() {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Memory emergency! Trying to free memory.");
  int priority = 0;
  int count = linked_list_count(s_callback_list);
  if (count == 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "No memory freeing callbacks registered");
    return false;
  }
  for (int p = 0; p < s_max_priority; ++p) {
    for (int i = 0; i < count; ++i) {
      MemoryPressureCallbackEntry *entry = linked_list_get(s_callback_list, i);
      if (entry->priority != priority) {
        continue;
      }
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Calling memory pressure callback %p with priority %d", entry->handler, entry->priority);
      if (entry->handler(entry->context)) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed some memory!");
        return true;
      }
      APP_LOG(APP_LOG_LEVEL_DEBUG, "No joy.");
    }
  }
  return false;
}

static bool prv_entry_compare(void *object1, void *object2) {
  MemoryPressureCallbackEntry *entry = object1;
  return entry->handler == object2;
}

static void prv_update_max_priority() {
  s_max_priority = 0;
  linked_list_foreach(s_callback_list, prv_max_priority_callback, NULL);
}

static bool prv_max_priority_callback(void *object, void *context) {
  MemoryPressureCallbackEntry *entry = object;
  if (entry->priority > s_max_priority) {
    s_max_priority = entry->priority;
  }
  return true;
}