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

#ifndef VERSION_H
#define VERSION_H

#include <pebble.h>

typedef struct __attribute__((__packed__)) {
    uint8_t major;
    uint8_t minor;
} VersionInfo;

void version_store_current();
bool version_is_first_launch();
VersionInfo version_get_last_launch();
VersionInfo version_get_current();
int version_info_compare(VersionInfo a, VersionInfo b);
const char* version_git_tag();

#endif //VERSION_H
