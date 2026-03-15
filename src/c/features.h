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

// If true, instead of using dictation input, the app will just use a fixed prompt.
#define ENABLE_FEATURE_FIXED_PROMPT 0

// IFTTT: if you change this, you need to update the corresponding feature in src/pkjs/features.js.
// If true, maps will be available.
#define ENABLE_FEATURE_MAPS 1

// If true, the image manager will be available (required for maps to function)
#define ENABLE_FEATURE_IMAGE_MANAGER 1

#if ENABLE_FEATURE_MAPS && !ENABLE_FEATURE_IMAGE_MANAGER
#error "ENABLE_FEATURE_MAPS requires ENABLE_FEATURE_IMAGE_MANAGER to be enabled."
#endif
