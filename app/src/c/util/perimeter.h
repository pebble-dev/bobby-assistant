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

// These definitions are all copied from the firmware. They aren't exposed in the public SDK.

#ifndef PERIMETER_H
#define PERIMETER_H

#include <pebble.h>

typedef struct GPerimeter GPerimeter;

typedef struct GRangeHorizontal {
  int16_t origin_x;
  int16_t size_w;
} GRangeHorizontal;

typedef struct GRangeVertical {
  int16_t origin_y;
  int16_t size_h;
} GRangeVertical;

typedef GRangeHorizontal (*GPerimeterCallback)(const GPerimeter *perimeter, const GSize *ctx_size,
                                               GRangeVertical vertical_range, uint16_t inset);

typedef struct GPerimeter {
  GPerimeterCallback callback;
} GPerimeter;

typedef struct {
  const GPerimeter *impl;
  uint8_t inset;
} TextLayoutFlowDataPerimeter;

typedef struct {
  GPoint origin_on_screen;
  GRangeVertical page_on_screen;
} TextLayoutFlowDataPaging;

typedef struct {
  TextLayoutFlowDataPerimeter perimeter;
  TextLayoutFlowDataPaging paging;
} TextLayoutFlowData;

struct GTextAttributes {
  //! Invalidate the cache if these parameters have changed
  uint32_t hash;
  GRect box;
  GFont font;
  GTextOverflowMode overflow_mode;
  GTextAlignment alignment;
  //! Cached parameters
  GSize max_used_size; //<! Max area occupied by text in px

  //! Vertical padding in px to add to the font line height when rendering
  int16_t line_spacing_delta;

  //! Layout restriction callback shrinking text box to fit within perimeter
  TextLayoutFlowData flow_data;
};

#endif //PERIMETER_H
