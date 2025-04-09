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

#include "map.h"

static void prv_image_updated(int image_id, ImageStatus status, void *context);
static void prv_layer_update(Layer *layer, GContext *ctx);

typedef struct {
  ConversationEntry* entry;
  GBitmap* bitmap;
} MapWidgetData;

static inline int prv_get_image_id(MapWidgetData *data) {
  return conversation_entry_get_widget(data->entry)->widget.map.image_id;
}

MapWidget* map_widget_create(GRect rect, ConversationEntry* entry) {
  int image_id = conversation_entry_get_widget(entry)->widget.map.image_id;
  GSize image_size = image_manager_get_size(image_id);
  Layer *layer = layer_create_with_data(GRect(rect.origin.x, rect.origin.y, rect.size.w, image_size.h + 2), sizeof(MapWidgetData));
  MapWidgetData* data = layer_get_data(layer);
  data->entry = entry;
  data->bitmap = NULL;
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
    if (data->bitmap) {
      gbitmap_destroy(data->bitmap);
    }
    data->bitmap = image_manager_get_image(image_id);
    layer_mark_dirty(layer);
  } else if (status == ImageStatusDestroyed) {
    data->bitmap = NULL;
    layer_mark_dirty(layer);
  }
}

static void prv_layer_update(Layer *layer, GContext *ctx) {
  MapWidgetData *data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  // bounds = grect_inset(bounds, (GEdgeInsets){.right = 5});
  GSize image_size = image_manager_get_size(prv_get_image_id(data));
  // GRect border_rect = GRect((bounds.size.w - image_size.w) / 2, 0, image_size.w + 2, image_size.h + 2);
  // GRect image_rect = grect_inset(border_rect, GEdgeInsets(1));
  GRect image_rect = grect_inset(bounds, GEdgeInsets(1, 0));
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(bounds.size.w, 0));
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
  if (data->bitmap) {
    graphics_draw_bitmap_in_rect(ctx, data->bitmap, image_rect);
  } else {
    graphics_context_set_fill_color(ctx, COLOR_FALLBACK(GColorLightGray, GColorWhite));
    graphics_fill_rect(ctx, image_rect, 0, GCornerNone);
  }
}
