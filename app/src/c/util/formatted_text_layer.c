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


#include "formatted_text_layer.h"

#include <pebble.h>

typedef enum {
  FragmentTypeTitle,
  FragmentTypeSubtitle,
  FragmentTypeBody,
} FragmentType;

typedef struct {
  FragmentType type;
  const char *text;
  size_t text_length;
  int16_t vertical_offset;
} TextFragment;

typedef struct {
  const char* text;
  GFont title_font;
  GFont subtitle_font;
  GFont body_font;
  TextFragment* fragments;
  size_t fragment_count;
  size_t largest_fragment_length;
  GTextAlignment alignment;
  int16_t total_height;
} FormattedTextLayerData;

static void prv_layer_update(Layer *layer, GContext *ctx);
static int prv_segment_upper_bound(const char* text);
static void prv_recalculate(FormattedTextLayer* layer);
static void prv_fragment(FormattedTextLayerData* data);
static void prv_layout(FormattedTextLayer* layer);

FormattedTextLayer* formatted_text_layer_create(GRect frame) {
  Layer *layer = layer_create_with_data(frame, sizeof(FormattedTextLayerData));
  FormattedTextLayerData *data = layer_get_data(layer);
  memset(data, 0, sizeof(FormattedTextLayerData));
  data->title_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  data->subtitle_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  data->body_font = fonts_get_system_font(FONT_KEY_GOTHIC_24);
  data->alignment = GTextAlignmentLeft;
  layer_set_update_proc(layer, prv_layer_update);
  return layer;
}

Layer* formatted_text_layer_get_layer(FormattedTextLayer* layer) {
  return layer;
}

void formatted_text_layer_destroy(FormattedTextLayer* layer) {
  FormattedTextLayerData *data = layer_get_data(layer);
  free(data->fragments);
  layer_destroy(layer);
}

void formatted_text_layer_set_text(FormattedTextLayer* layer, const char* text) {
  FormattedTextLayerData *data = layer_get_data(layer);
  data->text = text;
  prv_recalculate(layer);
}

void formatted_text_layer_set_title_font(FormattedTextLayer* layer, GFont font) {
  FormattedTextLayerData *data = layer_get_data(layer);
  data->title_font = font;
  prv_recalculate(layer);
}

void formatted_text_layer_set_subtitle_font(FormattedTextLayer* layer, GFont font) {
  FormattedTextLayerData *data = layer_get_data(layer);
  data->subtitle_font = font;
  prv_recalculate(layer);
}

void formatted_text_layer_set_body_font(FormattedTextLayer* layer, GFont font) {
  FormattedTextLayerData *data = layer_get_data(layer);
  data->body_font = font;
  prv_recalculate(layer);
}

void formatted_text_layer_set_text_alignment(FormattedTextLayer* layer, GTextAlignment alignment) {
  FormattedTextLayerData *data = layer_get_data(layer);
  data->alignment = alignment;
  prv_recalculate(layer);
}

static void prv_layer_update(Layer *layer, GContext *ctx) {
  FormattedTextLayerData *data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  GFont font_lookup[3] = {
    data->title_font,
    data->subtitle_font,
    data->body_font,
  };
  graphics_context_set_text_color(ctx, GColorBlack);
  char *buffer = malloc(data->largest_fragment_length + 1);
  for (size_t i = 0; i < data->fragment_count; ++i) {
    TextFragment *fragment = &data->fragments[i];
    if (fragment->text_length == 0) {
      continue;
    }
    // If we're off the bottom of the screen, stop - there's no more work to do.
    if (layer_convert_point_to_screen(layer, GPoint(0, fragment->vertical_offset)).y > 168) {
      break;
    }
    // If the next fragment starts before the beginning of the screen, skip ahead.
    if (i < data->fragment_count - 1) {
      TextFragment *next_fragment = &data->fragments[i+1];
      if (layer_convert_point_to_screen(layer, GPoint(0, next_fragment->vertical_offset)).y < -10) {
        continue;
      }
    }
    // We're at least partially on screen, get to it
    GRect frame = GRect(0, fragment->vertical_offset, bounds.size.w, 10000);
    strncpy(buffer, fragment->text, fragment->text_length);
    buffer[fragment->text_length] = '\0';
    graphics_draw_text(ctx, buffer, font_lookup[fragment->type], frame, GTextOverflowModeWordWrap, data->alignment, NULL);
  }
  free(buffer);
}

static void prv_fragment(FormattedTextLayerData *data) {
  if (data->fragments) {
    free(data->fragments);
  }
  int max_fragment_count = prv_segment_upper_bound(data->text);
  data->largest_fragment_length = 0;
  data->fragments = malloc(max_fragment_count * sizeof(TextFragment));
  size_t text_length = strlen(data->text);
  const char* ptr = data->text;
  TextFragment* last_fragment = &data->fragments[data->fragment_count++];
  last_fragment->text = ptr;
  last_fragment->type = FragmentTypeBody;
  while ((ptr = strchr(ptr, '#')) != NULL) {
    last_fragment->text_length = ptr - last_fragment->text;
    if (last_fragment->text_length > data->largest_fragment_length) {
      data->largest_fragment_length = last_fragment->text_length;
    }

    TextFragment *fragment = &data->fragments[data->fragment_count++];
    fragment->text = ptr;
    fragment->text_length = 0;
    int hash_count = 0;
    bool hashing = true;
    while (fragment->text[fragment->text_length] != '\n') {
      if (hashing && fragment->text[fragment->text_length] == '#') {
        ++hash_count;
        ++fragment->text;
      } else if (hashing && fragment->text[fragment->text_length] == ' ') {
        ++fragment->text;
      } else {
        hashing = false;
        ++fragment->text_length;
      }
    }
    switch (hash_count) {
      case 1:
        fragment->type = FragmentTypeTitle;
        break;
      case 2:
      default:
        fragment->type = FragmentTypeSubtitle;
        break;
    }
    ptr = fragment->text + fragment->text_length;
    if (*ptr == '\n') {
      ++ptr;
    }
    last_fragment = &data->fragments[data->fragment_count++];
    last_fragment->text = ptr;
    last_fragment->type = FragmentTypeBody;
  }
  if (last_fragment) {
    last_fragment->text_length = text_length - (last_fragment->text - data->text);
    if (last_fragment->text_length > data->largest_fragment_length) {
      data->largest_fragment_length = last_fragment->text_length;
    }
  }
}

static void prv_layout(FormattedTextLayer* layer) {
  FormattedTextLayerData *data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  GRect sizing_frame = GRect(0, 0, bounds.size.w, 10000);

  char *buffer = malloc(data->largest_fragment_length + 1);

  GFont font_lookup[3] = {
    data->title_font,
    data->subtitle_font,
    data->body_font,
  };

  int16_t y = 0;

  for (size_t i = 0; i < data->fragment_count; ++i) {
    // We need to copy the text out of the fragment, because it's not null-terminated, and Pebble doesn't take lengths.
    TextFragment *fragment = &data->fragments[i];
    if (fragment->text_length == 0) {
      continue;
    }
    strncpy(buffer, fragment->text, fragment->text_length);
    buffer[fragment->text_length] = '\0';
    GSize size = graphics_text_layout_get_content_size(buffer, font_lookup[fragment->type], sizing_frame, GTextOverflowModeWordWrap, data->alignment);
    fragment->vertical_offset = y;
    y += size.h;
  }
  data->total_height = y;
  free(buffer);
}

static void prv_recalculate(FormattedTextLayer* layer) {
  FormattedTextLayerData *data = layer_get_data(layer);
  if (!data->text) {
    return;
  }
  prv_fragment(data);
  prv_layout(layer);
  layer_mark_dirty(layer);
}

static int prv_segment_upper_bound(const char* text) {
  int count = 1;
  const char* ptr = text;
  while ((ptr = strchr(ptr, '#')) != NULL) {
    count += 2;
    // '##' starts a new segment, so we don't want to start a segment for each '#' when that appears.
    if (*++ptr == '#') {
      ++ptr;
    }
  }
  return count;
}

GSize formatted_text_layer_get_content_size(FormattedTextLayer* layer) {
  GRect bounds = layer_get_bounds(layer);
  FormattedTextLayerData *data = layer_get_data(layer);
  return GSize(bounds.size.w, data->total_height);
}
