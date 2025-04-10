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
static void prv_destroy_image();
static void prv_handle_new_image(int image_id, size_t size, DictionaryIterator *iterator);
static void prv_handle_image_chunk(int image_id, size_t offset, DictionaryIterator *iterator);
static void prv_handle_image_complete(int image_id);


static ManagedImage s_managed_image = {0};
static uint8_t s_image_storage[PBL_IF_COLOR_ELSE(3800, 1900)] = {0};
static EventHandle *s_appmessage_handle;

void image_manager_init() {
  events_app_message_request_inbox_size(1024);
  s_appmessage_handle = events_app_message_register_inbox_received(prv_inbox_received, NULL);
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
  if (s_managed_image.image_id != image_id) {
    return;
  }
  prv_destroy_image();
}

void image_manager_destroy_all_images() {
  prv_destroy_image();
}

static ManagedImage *prv_find_image(int image_id) {
  if (s_managed_image.image_id != image_id) {
    return NULL;
  }
  return &s_managed_image;
}

static void prv_destroy_image() {
  s_managed_image.status = ImageStatusDestroyed;
  if (s_managed_image.callback) {
    s_managed_image.callback(s_managed_image.image_id, ImageStatusDestroyed, s_managed_image.context);
  }
  if (s_managed_image.bitmap) {
    gbitmap_destroy(s_managed_image.bitmap);
    s_managed_image.bitmap = NULL;
  }
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
  if (s_managed_image.image_id != 0) {
    prv_destroy_image();
  }
  s_managed_image.image_id = image_id;
  s_managed_image.status = ImageStatusCreated;
  s_managed_image.callback = NULL;
  s_managed_image.data = s_image_storage;
  s_managed_image.size = size;
  s_managed_image.bitmap = NULL;
  s_managed_image.image_size = GSize(width, height);
}

static void prv_handle_image_chunk(int image_id, size_t offset, DictionaryIterator *iterator) {
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Handling image chunk for image_id: %d, offset: %d", image_id, offset);
  ManagedImage *image = prv_find_image(image_id);
  if (!image) {
    BOBBY_LOG(APP_LOG_LEVEL_INFO, "Got data for unknown image id %d", image_id);
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
