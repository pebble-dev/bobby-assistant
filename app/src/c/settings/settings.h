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

typedef enum {
  QuickLaunchBehaviourConverseWithTimeout = 1,
  QuickLaunchBehaviourConverseForever = 2,
  QuickLaunchBehaviourHomeScreen = 3,
} QuickLaunchBehaviour;

typedef enum {
  VibePatternSettingReveille = 1,
  VibePatternSettingMario = 2,
  VibePatternSettingNudgeNudge = 3,
  VibePatternSettingJackhammer = 4,
  VibePatternSettingStandard = 5,
} VibePatternSetting;

void settings_init();
void settings_deinit();
QuickLaunchBehaviour settings_get_quick_launch_behaviour();
VibePatternSetting settings_get_alarm_vibe_pattern();
VibePatternSetting settings_get_timer_vibe_pattern();
