// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package functions

import (
	"context"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
	"google.golang.org/genai"
	"strings"
)

type UpdateSettingsInput struct {
	UnitSystem            string `json:"unit_system"`
	ResponseLanguage      string `json:"response_language"`
	AlarmVibrationPattern string `json:"alarm_vibration_pattern"`
	TimerVibrationPattern string `json:"timer_vibration_pattern"`
	QuickLaunchBehaviour  string `json:"quick_launch_behaviour"`
	ConfirmPrompts        *bool  `json:"confirm_prompts"`
}

func init() {
	t := true
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "update_settings",
			Description: "Update the user's settings, e.g. their preferred unit system. Call if and only if the user asks you to change something. Properties not specified won't be changed. No property is required. For security reasons, changing the location permission can't be done with this method - the user must go to the settings page.",
			Parameters: &genai.Schema{
				Type: genai.TypeObject,
				Properties: map[string]*genai.Schema{
					"unit_system": {
						Type:        genai.TypeString,
						Description: "Whether the user prefers metric, imperial, 'UK hybrid' (temperature in celsius, distance in miles), or both metric and imperial. Or, 'auto' to figure it out based on the user's location.",
						Enum:        []string{"auto", "imperial", "metric", "uk hybrid", "both"},
					},
					"response_language": {
						Type:        genai.TypeString,
						Description: "The user's preferred response language. This is the language in which the assistant will respond to the user, or 'automatic' to use the language of the user's last message.",
						Enum: []string{"auto", "af_ZA", "id_ID", "ms_MY", "cs_CZ", "da_DK", "de_DE",
							"en_US", "es_ES", "fil_PH", "fr_FR", "gl_ES", "hr_HR", "is_IS", "it_IT", "sw_TZ", "lv_LV",
							"lt_LT", "hu_HU", "nl_NL", "no_NO", "pl_PL", "pt_PT", "ro_RO", "ru_RU", "sk_SK", "sl_SI",
							"fi_FI", "sv_SE", "tr_TR", "zu_ZA"},
					},
					"alarm_vibration_pattern": {
						Type:        genai.TypeString,
						Description: "The user's preferred alarm vibration pattern, used when alarms go off.",
						Enum:        []string{"Reveille", "Mario", "Nudge Nudge", "Jackhammer", "Standard"},
					},
					"timer_vibration_pattern": {
						Type:        genai.TypeString,
						Description: "The user's preferred timer vibration pattern, used when timers go off.",
						Enum:        []string{"Reveille", "Mario", "Nudge Nudge", "Jackhammer", "Standard"},
					},
					"quick_launch_behaviour": {
						Type:        genai.TypeString,
						Description: "The user's preferred quick launch behaviour. The app can open the home screen (same as a non-quick launch), open the conversation but time out and quit after a minute, or open the conversation and stick around.",
						Enum:        []string{"open home screen", "start conversation and time out", "start conversation and stay open"},
					},
					"confirm_prompts": {
						Type:        genai.TypeBoolean,
						Description: "Whether the user wants to be asked to confirm their all of their queries before acting on them",
						Nullable:    &t,
					},
				},
				Required: []string{},
			},
		},
		Cb:         updateSettingsImpl,
		Thought:    updateSettingsThought,
		InputType:  UpdateSettingsInput{},
		Capability: "update_settings",
	})
}

func updateSettingsImpl(ctx context.Context, quotaTracker *quota.Tracker, i any, requestChan chan<- map[string]any, responseChan <-chan map[string]any) any {
	arg := i.(*UpdateSettingsInput)
	request := map[string]any{
		"action": "update_settings",
	}
	if arg.UnitSystem != "" {
		request["unitSystem"] = arg.UnitSystem
	}
	if arg.ResponseLanguage != "" {
		request["responseLanguage"] = arg.ResponseLanguage
	}
	if arg.AlarmVibrationPattern != "" {
		request["alarmVibrationPattern"] = arg.AlarmVibrationPattern
	}
	if arg.TimerVibrationPattern != "" {
		request["timerVibrationPattern"] = arg.TimerVibrationPattern
	}
	if arg.QuickLaunchBehaviour != "" {
		request["quickLaunchBehaviour"] = arg.QuickLaunchBehaviour
	}
	if arg.ConfirmPrompts != nil {
		request["confirmPrompts"] = *arg.ConfirmPrompts
	}
	requestChan <- request
	response := <-responseChan
	return response
}

func updateSettingsThought(args any) string {
	i := args.(*UpdateSettingsInput)
	var settingTypes []string
	if i.UnitSystem != "" {
		settingTypes = append(settingTypes, "unit system")
	}
	if i.ResponseLanguage != "" {
		settingTypes = append(settingTypes, "response language")
	}
	if i.AlarmVibrationPattern != "" {
		settingTypes = append(settingTypes, "alarm vibration")
	}
	if i.TimerVibrationPattern != "" {
		settingTypes = append(settingTypes, "timer vibration")
	}
	if i.QuickLaunchBehaviour != "" {
		settingTypes = append(settingTypes, "quick launch behaviour")
	}
	if i.ConfirmPrompts != nil {
		settingTypes = append(settingTypes, "prompt confirmation")
	}
	if len(settingTypes) == 0 {
		return "Updating no settings..."
	}
	var settingString string
	if len(settingTypes) == 1 {
		settingString = settingTypes[0]
	} else {
		lastEntry := settingTypes[len(settingTypes)-1]
		settingString = strings.Join(settingTypes[0:len(settingTypes)-1], ", ") + " and " + lastEntry
	}
	return "Updating " + settingString + " settings..."
}
