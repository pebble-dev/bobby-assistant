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

#include <pebble.h>
#include <pebble-events/pebble-events.h>
#include <@smallstoneapps/linked-list/linked-list.h>

#include "../util/memory/malloc.h"
#include "../util/memory/pressure.h"
#include "../util/logging.h"
#include "image_manager.h"

typedef struct {
  int image_id;
  ImageStatus status;
  GSize image_size;
  size_t size;
  ImageManagerCallback callback;
  void *context;
  uint8_t* data;
  GBitmap* bitmap;
} ManagedImage;

static void prv_inbox_received(DictionaryIterator *iterator, void *context);
static ManagedImage *prv_find_image(int image_id);
static int16_t prv_find_image_index(int image_id);
static bool prv_image_id_compare(void *image_id, void *object);
static void prv_destroy_image(ManagedImage *image);
static bool prv_foreach_destroy(void *object, void *context);
static void prv_handle_new_image(int image_id, size_t size, DictionaryIterator *iterator);
static void prv_handle_image_chunk(int image_id, size_t offset, DictionaryIterator *iterator);
static void prv_handle_image_complete(int image_id);
static bool prv_handle_memory_pressure(void *context);

static EventHandle *s_appmessage_handle;
static LinkedRoot *s_image_list;
static ManagedImage *s_cached_image_ref = NULL;

void image_manager_init() {
  s_image_list = linked_list_create_root();
  events_app_message_request_inbox_size(1024);
  s_appmessage_handle = events_app_message_register_inbox_received(prv_inbox_received, NULL);
  memory_pressure_register_callback(prv_handle_memory_pressure, 0, NULL);
}

void image_manager_deinit() {
  events_app_message_unsubscribe(s_appmessage_handle);
}

void image_manager_register_callback(int image_id, ImageManagerCallback callback, void *context) {
  ManagedImage *image = prv_find_image(image_id);
  if (!image) {
    return;
  }
  image->callback = callback;
  image->context = context;
}

void image_manager_unregister_callback(int image_id) {
  ManagedImage *image = prv_find_image(image_id);
  if (!image) {
    return;
  }
  image->callback = NULL;
}

GBitmap *image_manager_get_image(int image_id) {
  ManagedImage *image = prv_find_image(image_id);
  if (!image) {
    return NULL;
  }
  return image->bitmap;
}

GSize image_manager_get_size(int image_id) {
  ManagedImage *image = prv_find_image(image_id);
  if (!image) {
    return GSizeZero;
  }
  return image->image_size;
}

void image_manager_destroy_image(int image_id) {
  int16_t image_idx = prv_find_image_index(image_id);
  if (image_idx < 0) {
    return;
  }
  ManagedImage *image = linked_list_get(s_image_list, image_idx);
  prv_destroy_image(image);
  linked_list_remove(s_image_list, image_idx);
}

void image_manager_destroy_all_images() {
  linked_list_foreach(s_image_list, prv_foreach_destroy, NULL);
  linked_list_clear(s_image_list);
}

static ManagedImage *prv_find_image(int image_id) {
  if (s_cached_image_ref != NULL && s_cached_image_ref->image_id == image_id) {
    return s_cached_image_ref;
  }
  int16_t idx = prv_find_image_index(image_id);
  if (idx >= 0) {
    return linked_list_get(s_image_list, idx);
  }
  return NULL;
}

static int16_t prv_find_image_index(int image_id) {
  return linked_list_find_compare(s_image_list, &image_id, prv_image_id_compare);
}

static bool prv_image_id_compare(void *image_id, void *object) {
  ManagedImage *image = object;
  return image->image_id == *(int *)image_id;
}

static void prv_destroy_image(ManagedImage *image) {
  image->status = ImageStatusDestroyed;
  if (image->callback) {
    image->callback(image->image_id, ImageStatusDestroyed, image->context);
  }
  if (image->bitmap) {
    gbitmap_destroy(image->bitmap);
    image->bitmap = NULL;
  }
  if (image->data) {
    free(image->data);
    image->data = NULL;
  }
  if (s_cached_image_ref == image) {
    s_cached_image_ref = NULL;
  }
  free(image);
}

static bool prv_foreach_destroy(void *object, void *context) {
  ManagedImage *image = object;
  prv_destroy_image(image);
  return true;
}

static void prv_inbox_received(DictionaryIterator *iterator, void *context) {
  Tuple *tuple = dict_find(iterator, MESSAGE_KEY_IMAGE_ID);
  if (!tuple) {
    return;
  }
  int image_id = tuple->value->int32;
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "handling something for image_id: %d", image_id);
  tuple = dict_find(iterator, MESSAGE_KEY_IMAGE_START_BYTE_SIZE);
  if (tuple) {
    prv_handle_new_image(image_id, tuple->value->int32, iterator);
    return;
  }
  tuple = dict_find(iterator, MESSAGE_KEY_IMAGE_CHUNK_OFFSET);
  if (tuple) {
    prv_handle_image_chunk(image_id, tuple->value->int32, iterator);
    return;
  }
  tuple = dict_find(iterator, MESSAGE_KEY_IMAGE_COMPLETE);
  if (tuple) {
    prv_handle_image_complete(image_id);
    return;
  }
}

static void prv_handle_new_image(int image_id, size_t size, DictionaryIterator *iterator) {
  Tuple *tuple = dict_find(iterator, MESSAGE_KEY_IMAGE_WIDTH);
  int16_t width = tuple->value->int32;
  tuple = dict_find(iterator, MESSAGE_KEY_IMAGE_HEIGHT);
  int16_t height = tuple->value->int32;
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "New image: %d, size: %d, width: %d, height: %d", image_id, size, width, height);
  ManagedImage *image = bmalloc(sizeof(ManagedImage));
  if (!image) {
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Failed to allocate memory for image metadata");
    return;
  }
  image->data = bmalloc(size);
  if (!image->data) {
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Failed to allocate memory for image data");
  }
  image->image_id = image_id;
  image->status = ImageStatusDestroyed;
  image->callback = NULL;
  image->size = size;
  image->bitmap = NULL;
  image->image_size = GSize(width, height);
  linked_list_append(s_image_list, image);
}

static void prv_handle_image_chunk(int image_id, size_t offset, DictionaryIterator *iterator) {
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Handling image chunk for image_id: %d, offset: %d", image_id, offset);
  ManagedImage *image = prv_find_image(image_id);
  if (!image) {
    BOBBY_LOG(APP_LOG_LEVEL_INFO, "Got data for unknown image id %d", image_id);
    return;
  }
  if (!image->data) {
    BOBBY_LOG(APP_LOG_LEVEL_INFO, "Got data for image we couldn't allocate; discarding.");
    return;
  }
  Tuple *tuple = dict_find(iterator, MESSAGE_KEY_IMAGE_CHUNK_DATA);
  if (!tuple) {
    BOBBY_LOG(APP_LOG_LEVEL_INFO, "Got data for image id %d with no chunk data!", image_id);
    return;
  }
  if (offset + tuple->length > image->size) {
    BOBBY_LOG(APP_LOG_LEVEL_INFO, "Image data chunk too large: %d + %d > %d", offset, tuple->length, image->size);
    return;
  }
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Got %d bytes for image id %d; copying to %p", tuple->length, image_id, image->data + offset);
  memcpy(image->data + offset, tuple->value->data, tuple->length);
}

static void prv_handle_image_complete(int image_id) {
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Handling image complete for image_id: %d", image_id);
  ManagedImage *image = prv_find_image(image_id);
  if (!image) {
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Got complete for unknown image id %d", image_id);
    return;
  }
  if (!image->data) {
    BOBBY_LOG(APP_LOG_LEVEL_INFO, "Got complete for image we couldn't allocate; destroying.");
    prv_destroy_image(image);
    linked_list_remove(s_image_list, prv_find_image_index(image_id));
    return;
  }
  if (!image->bitmap) {
    image->bitmap = gbitmap_create_with_data(image->data);
    if (!image->bitmap) {
      BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Failed to create bitmap from data");
      return;
    }
    GRect bounds = gbitmap_get_bounds(image->bitmap);
    GBitmapFormat format = gbitmap_get_format(image->bitmap);
    int bytes_per_row = gbitmap_get_bytes_per_row(image->bitmap);
    GColor *palette = gbitmap_get_palette(image->bitmap);
    BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Bitmap created: %d x %d, format: %d, bytes_per_row: %d", bounds.size.w, bounds.size.h, format, bytes_per_row);
    if (format == GBitmapFormat2BitPalette) {
      BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Palette: %d, %d, %d, %d", palette[0], palette[1], palette[2], palette[3]);
    }
    image->status = ImageStatusCompleted;
  }
  if (image->callback) {
    image->callback(image->image_id, ImageStatusCompleted, image->context);
  }
}


static bool prv_handle_memory_pressure(void *context) {
  if (linked_list_count(s_image_list) == 0) {
    return false;
  }
  BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Memory pressure! Destroying the oldest image.");
  ManagedImage *image = linked_list_get(s_image_list, 0);
  prv_destroy_image(image);
  linked_list_remove(s_image_list, 0);
  return true;
}