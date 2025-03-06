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

#ifndef COLOURS_H
#define COLOURS_H

#include <pebble.h>

#define ACCENT_COLOUR GColorShockingPink
#define SELECTION_HIGHLIGHT_COLOUR ACCENT_COLOUR

void bobby_status_bar_config(StatusBarLayer *status_bar);
void bobby_status_bar_result_pane_config(StatusBarLayer *status_bar);

#endif //COLOURS_H
