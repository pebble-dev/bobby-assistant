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

#include "root_window.h"
#include "release_notes.h"
#include "consent/consent.h"
#include "converse/session_window.h"
#include "converse/conversation_manager.h"
#include "image_manager/image_manager.h"
#include "alarms/manager.h"
#include "version/version.h"
#include "settings/settings.h"

#include <pebble.h>
#include <pebble-events/pebble-events.h>

#include "util/fonts.h"
#include "util/logging.h"
#include "util/memory/pressure.h"


#define QUICK_LAUNCH_TIMEOUT_MS 60000

static RootWindow* s_root_window = NULL;

static void prv_init(void) {
  memory_pressure_init();
  version_init();
  consent_migrate();
  consent_init();
  settings_init();
  conversation_manager_init();
#if ENABLE_FEATURE_IMAGE_MANAGER
  image_manager_init();
#endif
  events_app_message_open();
  alarm_manager_init();
  fonts_load();
}

static void prv_deinit(void) {
  if (s_root_window) {
    root_window_destroy(s_root_window);
  }
#ifdef ENABLE_FEATURE_IMAGE_MANAGER
  image_manager_deinit();
#endif
  fonts_unload();
}

int main(void) {
  VersionInfo version_info = version_get_current();
  BOBBY_LOG(APP_LOG_LEVEL_INFO, "Bobby %d.%d", version_info.major, version_info.minor);
  prv_init();
  
  if (alarm_manager_maybe_alarm()) {
    // don't actually have anything to do here - the alarm manager already did it.
  } else {
    QuickLaunchBehaviour quick_launch_behaviour = settings_get_quick_launch_behaviour();
    bool quick_launch_to_session = launch_reason() == APP_LAUNCH_QUICK_LAUNCH &&
                                   quick_launch_behaviour != QuickLaunchBehaviourHomeScreen;
    // If the user has accepted an older consent version, quick launch still goes straight to dictation.
    // The conversation manager asks them to review the new terms before anything is sent.
    if (consent_accepted_version() == 0 || (must_present_consent() && !quick_launch_to_session)) {
      consent_window_push_for_launch();
    } else {
      if (launch_reason() == APP_LAUNCH_QUICK_LAUNCH) {
        if (quick_launch_to_session) {
          session_window_push(quick_launch_behaviour == QuickLaunchBehaviourConverseWithTimeout ? QUICK_LAUNCH_TIMEOUT_MS : 0, NULL);
        } else {
          s_root_window = root_window_create();
          root_window_push(s_root_window);
        }
      } else {
        s_root_window = root_window_create();
        root_window_push(s_root_window);
      }
      release_notes_maybe_push();
    }
  }


  app_event_loop();
  prv_deinit();
}
