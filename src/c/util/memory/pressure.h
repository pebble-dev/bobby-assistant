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

typedef bool (*MemoryPressureHandler)(void *context);

void memory_pressure_init();
void memory_pressure_deinit();
void memory_pressure_register_callback(MemoryPressureHandler handler, int priority, void *context);
void memory_pressure_unregister_callback(MemoryPressureHandler handler);
bool memory_pressure_try_free();
