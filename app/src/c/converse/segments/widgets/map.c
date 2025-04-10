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
#include "../../../image_manager/image_manager.h"
#include "../../../util/memory/sdk.h"
#include "../../../util/thinking_layer.h"

#include "map.h"

static void prv_image_updated(int image_id, ImageStatus status, void *context);
static void prv_layer_update(Layer *layer, GContext *ctx);

typedef struct {
  ConversationEntry* entry;
  GBitmap* bitmap;
  ThinkingLayer* loading_layer;
  GDrawCommandImage *skull_image;
} MapWidgetData;

static inline int prv_get_image_id(MapWidgetData *data) {
  return conversation_entry_get_widget(data->entry)->widget.map.image_id;
}

MapWidget* map_widget_create(GRect rect, ConversationEntry* entry) {
  int image_id = conversation_entry_get_widget(entry)->widget.map.image_id;
  GSize image_size = image_manager_get_size(image_id);
  Layer *layer = blayer_create_with_data(GRect(rect.origin.x, rect.origin.y, rect.size.w, image_size.h + 2), sizeof(MapWidgetData));
  MapWidgetData* data = layer_get_data(layer);
  data->entry = entry;
  data->bitmap = NULL;
  data->loading_layer = thinking_layer_create(GRect(rect.size.w / 2 - THINKING_LAYER_WIDTH / 2, image_size.h / 2 - THINKING_LAYER_HEIGHT / 2, THINKING_LAYER_WIDTH, THINKING_LAYER_HEIGHT));
  layer_add_child(layer, data->loading_layer);
  image_manager_register_callback(image_id, prv_image_updated, layer);
  layer_set_update_proc(layer, prv_layer_update);
  return layer;
}

ConversationEntry* map_widget_get_entry(MapWidget* layer) {
  MapWidgetData* data = layer_get_data(layer);
  return data->entry;
}

void map_widget_destroy(MapWidget* layer) {
  MapWidgetData* data = layer_get_data(layer);
  if (data->bitmap) {
    gbitmap_destroy(data->bitmap);
  }
  if (data->loading_layer) {
    thinking_layer_destroy(data->loading_layer);
  }
  if (data->skull_image) {
    gdraw_command_image_destroy(data->skull_image);
  }
  int image_id = prv_get_image_id(data);
  image_manager_unregister_callback(image_id);
  layer_destroy(layer);
}

void map_widget_update(MapWidget* layer) {
  // nothing to do here.
}

static void prv_image_updated(int image_id, ImageStatus status, void *context) {
  Layer *layer = context;
  MapWidgetData* data = layer_get_data(layer);
  if (status == ImageStatusCompleted) {
    if (data->loading_layer) {
      layer_remove_from_parent(data->loading_layer);
      thinking_layer_destroy(data->loading_layer);
      data->loading_layer = NULL;
    }
    if (data->bitmap) {
      gbitmap_destroy(data->bitmap);
    }
    data->bitmap = image_manager_get_image(image_id);
    layer_mark_dirty(layer);
  } else if (status == ImageStatusDestroyed) {
    if (data->loading_layer) {
      layer_remove_from_parent(data->loading_layer);
      thinking_layer_destroy(data->loading_layer);
      data->loading_layer = NULL;
    }
    data->bitmap = NULL;
    if (!data->skull_image) {
      data->skull_image = bgdraw_command_image_create_with_resource(RESOURCE_ID_IMAGE_SKULL);
    }
    layer_mark_dirty(layer);
  }
}

static void prv_layer_update(Layer *layer, GContext *ctx) {
  MapWidgetData *data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  GPoint user_location = conversation_entry_get_widget(data->entry)->widget.map.user_location;
  GRect image_rect = grect_inset(bounds, GEdgeInsets(1, 0));
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(bounds.size.w, 0));
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
  if (data->bitmap) {
    graphics_draw_bitmap_in_rect(ctx, data->bitmap, image_rect);
    if (user_location.x > 0 && user_location.y > 0) {
      GPoint center = GPoint(user_location.x + image_rect.origin.x, user_location.y + image_rect.origin.y);
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_circle(ctx, center, 6);
      graphics_context_set_stroke_color(ctx, COLOR_FALLBACK(GColorDarkGray, GColorBlack));
      graphics_draw_circle(ctx, center, 6);
      graphics_context_set_fill_color(ctx, COLOR_FALLBACK(GColorBlue, GColorBlack));
      graphics_fill_circle(ctx, center, 4);
    }
  } else {
    graphics_context_set_fill_color(ctx, COLOR_FALLBACK(GColorLightGray, GColorWhite));
    graphics_fill_rect(ctx, image_rect, 0, GCornerNone);
    if (data->skull_image) {
      GSize skull_size = gdraw_command_image_get_bounds_size(data->skull_image);
      gdraw_command_image_draw(ctx, data->skull_image, GPoint(image_rect.origin.x + image_rect.size.w / 2 - skull_size.w / 2, image_rect.origin.y + image_rect.size.h / 2 - skull_size.h / 2));
    }
  }
}
