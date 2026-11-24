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

#ifndef APP_CONSENT_H
#define APP_CONSENT_H

#include <pebble.h>

// Close code the service uses when the consent version sent with a query is out of date.
#define CONSENT_CLOSE_CODE_REQUIRED 4001

typedef void (*ConsentCompleteCallback)(bool accepted, void *context);

void consent_init();
void consent_migrate();

// The consent version the user last accepted, or 0 if they never have.
int consent_accepted_version();
// True if the user must (re-)consent before Bobby can send anything to the service.
bool must_present_consent();
// Marks the accepted consent as outdated, e.g. because the service rejected it.
void consent_mark_outdated();

// Shows the consent flow on launch, then the root window once the user accepts.
void consent_window_push_for_launch();
// Shows a "Hold up!" notice and the consent flow on top of the current window,
// then calls callback once the window closes. callback may be NULL.
void consent_window_push_for_prompt(ConsentCompleteCallback callback, void *context);

#endif //APP_CONSENT_H
