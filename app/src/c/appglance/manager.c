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

#include "manager.h"
#include "../alarms/manager.h"

#include <pebble.h>

#include "../util/time.h"

static void prv_app_glance_reload(AppGlanceReloadSession *session, size_t limit, void *context);

void app_glance_manager_refresh() {
  app_glance_reload(prv_app_glance_reload, NULL);
}

static void prv_app_glance_reload(AppGlanceReloadSession *session, size_t limit, void *context) {
  int alarm_count = alarm_manager_get_alarm_count();
  uint32_t glances = 0;
  const size_t template_size = 111;
  char *template = malloc(template_size);
  const size_t target_time_long_size = 25;
  char *target_time_long = malloc(target_time_long_size);
  for (int i = 0; i < alarm_count && glances < limit; i++) {
    Alarm *alarm = alarm_manager_get_alarm(i);
    AppGlanceSlice slice;
    time_t expiry = alarm_get_time(alarm);
    char *name = alarm_get_name(alarm);
    if (alarm_is_timer(alarm)) {
      if (!name) {
        name = "Timer";
      }
      snprintf(template, template_size, "{time_until(%ld)|format(>=1M:'%%T',>1S:'%%S seconds',>0S:'1 second','Now!')} - %.25s", expiry, name);
      slice = (AppGlanceSlice) {
      .layout = {
          .icon = APP_GLANCE_SLICE_DEFAULT_ICON,
          .subtitle_template_string = template,
        },
        .expiration_time = expiry + 2,
      };
    } else {
      if (!name) {
        name = "Alarm";
      }
      struct tm *expiry_tm = localtime(&expiry);
      format_datetime(target_time_long, target_time_long_size, expiry);
      char target_time_short[10];
      format_time_ampm(target_time_short, sizeof(target_time_short), expiry_tm);
      snprintf(template, template_size, "{time_until(%ld)|format(>=24H:'%s','%s')} - %.23s", expiry, target_time_long, target_time_short, name);
      slice = (AppGlanceSlice) {
        .layout = {
          .icon = APP_GLANCE_SLICE_DEFAULT_ICON,
          .subtitle_template_string = template,
        },
        .expiration_time = expiry + 2,
      };
    }
    app_glance_add_slice(session, slice);
    glances++;
  }
  free(template);
  free(target_time_long);
}