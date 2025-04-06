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

#include "version.h"
#include "../util/persist_keys.h"

#include <pebble.h>
#include "pebble_process_info.h"

// The standard pebble app build process injects this structure.
extern const PebbleProcessInfo __pbl_app_info;

static bool s_is_first_launch = false;
static bool s_is_update = false;
static VersionInfo s_last_launch;

VersionInfo prv_read_last_launch();

void version_init() {
    VersionInfo version_info = version_get_current();
    s_last_launch = prv_read_last_launch();
    s_is_first_launch = s_last_launch.major == 0 && s_last_launch.minor == 0;
    if (version_info_compare(version_info, s_last_launch) != 0) {
        s_is_update = true;
        int status = persist_write_data(PERSIST_KEY_VERSION, &version_info, sizeof(VersionInfo));
        if (status < 0) {
            APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to write version info: %d", status);
        } else {
            APP_LOG(APP_LOG_LEVEL_INFO, "Current version (v%d.%d) stored (previous: v%d.%d)", version_info.major, version_info.minor, s_last_launch.major, s_last_launch.minor);
        }
    } else {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Version (v%d.%d) unchanged since last launch.", version_info.major, version_info.minor);
    }
}

bool version_is_first_launch() {
    return s_is_first_launch;
}

bool version_is_updated() {
    return s_is_update;
}

VersionInfo version_get_last_launch() {
    return s_last_launch;
}

VersionInfo version_get_current() {
    return (VersionInfo) {
        .major = __pbl_app_info.process_version.major,
        .minor = __pbl_app_info.process_version.minor,
    };
}

int version_info_compare(VersionInfo a, VersionInfo b) {
    if (a.major < b.major) {
        return -1;
    }
    if (a.major > b.major) {
        return 1;
    }
    if (a.minor < b.minor) {
        return -1;
    }
    if (a.minor > b.minor) {
        return 1;
    }
    return 0;
}

VersionInfo prv_read_last_launch() {
    VersionInfo version_info;
    int status = persist_read_data(PERSIST_KEY_VERSION, &version_info, sizeof(VersionInfo));
    if (status < 0) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to read version info: %d", status);
        return (VersionInfo) {
            .major = 0,
            .minor = 0,
        };
    }
    return version_info;
}
