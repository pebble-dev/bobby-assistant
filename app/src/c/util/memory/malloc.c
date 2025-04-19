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

#include "malloc.h"
#include "pressure.h"
#include "../logging.h"

#include <pebble.h>

void *bmalloc(size_t size) {
  register uintptr_t lr __asm("lr");
  const uintptr_t saved_lr = lr;
  int heap_size = heap_bytes_free();
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "malloc request: %d; free: %d", size, heap_size);
  while (true) {
    heap_size = heap_bytes_free();
    if (heap_bytes_free() > 750) {
      void *ptr = malloc(size);
      if (ptr) {
        BOBBY_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "malloc returned %p for caller %p", ptr, saved_lr);
        return ptr;
      }
      BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Out of memory! Need to allocate %d bytes; %d bytes free.", size, heap_size);
    } else {
      BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Low memory (%d byte free); trying to free some before allocating %d bytes.", heap_size, size);
    }
    if (!memory_pressure_try_free()) {
      BOBBY_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocate memory: couldn't free enough heap.");
      void *tried = malloc(size);
      if (tried) {
        BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "malloc returned %p for caller %p", tried, saved_lr);
      }
      return tried;
    }
    int new_heap_size = heap_bytes_free();
    BOBBY_LOG(APP_LOG_LEVEL_INFO, "Freed %d bytes, heap size is now %d. Retrying allocation of %d bytes.", heap_size - new_heap_size, new_heap_size, size);
  }
  return NULL;
}
