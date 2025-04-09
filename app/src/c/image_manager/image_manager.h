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

#pragma once
#include <pebble.h>

typedef enum {
  ImageStatusCreated,
  ImageStatusCompleted,
  ImageStatusDestroyed,
} ImageStatus;

typedef void (*ImageManagerCallback)(int image_id, ImageStatus status, void *context);

void image_manager_init();
void image_manager_deinit();
void image_manager_register_callback(int image_id, ImageManagerCallback callback, void *context);
void image_manager_unregister_callback(int image_id);
GBitmap *image_manager_get_image(int image_id);
GSize image_manager_get_size(int image_id);
void image_manager_destroy_image(int image_id);
void image_manager_destroy_all_images();
