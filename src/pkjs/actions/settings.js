/**
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

var config = require('../config');

function settingUpdater(session, key, value) {
    var updaters = {
        'unitSystem': updateUnitSystem,
        'responseLanguage': updateResponseLanguage,
        'alarmVibrationPattern': updateAlarmVibrationPattern,
        'timerVibrationPattern': updateTimerVibrationPattern,
        'quickLaunchBehaviour': updateQuickLaunchBehaviour,
        'confirmPrompts': updateConfirmPrompts,
    };
    if (key in updaters) {
        return updaters[key](session, value);
    } else {
        console.log("Unknown setting: " + key);
    }
    return null;
}

function updateUnitSystem(session, value) {
    if (value === 'imperial') {
        config.setSetting('UNIT_PREFERENCE', 'imperial');
        return 'units to imperial';
    } else if (value === 'metric') {
        config.setSetting('UNIT_PREFERENCE', 'metric');
        return 'units to metric';
    } else if (value === 'uk hybrid') {
        config.setSetting('UNIT_PREFERENCE', 'uk');
        return 'units to UK hybrid';
    } else if (value === 'both') {
        config.setSetting('UNIT_PREFERENCE', 'both');
        return 'units to both imperial and metric';
    } else if (value === 'auto') {
        config.setSetting('UNIT_PREFERENCE', '');
        return 'units to automatic';
    } else {
        console.log("Unknown unit system: " + value);
        return null;
    }
}

function updateResponseLanguage(session, value) {
    var knownLanguageMap = {
        "af_ZA": "Afrikaans",
        "id_ID": "Bahasa Indonesia",
        "ms_MY": "Bahasa Melayu",
        "cs_CZ": "Čeština",
        "da_DK": "Dansk",
        "de_DE": "Deutsch",
        "en_US": "English",
        "es_ES": "Español",
        "fil_PH": "Filipino",
        "fr_FR": "Français",
        "gl_ES": "Galego",
        "hr_HR": "Hrvatski",
        "is_IS": "Íslenska",
        "it_IT": "Italiano",
        "sw_TZ": "Kiswahili",
        "lv_LV": "Latviešu",
        "lt_LT": "Lietuvių",
        "hu_HU": "Magyar",
        "nl_NL": "Nederlands",
        "no_NO": "Norsk",
        "pl_PL": "Polski",
        "pt_PT": "Português",
        "ro_RO": "Română",
        "ru_RU": "Русский",
        "sk_SK": "Slovenčina",
        "sl_SI": "Slovenščina",
        "fi_FI": "Suomi",
        "sv_SE": "Svenska",
        "tr_TR": "Türkçe",
        "zu_ZA": "IsiZulu"
    }
    if (value === 'auto') {
        config.setSetting("LANGUAGE_CODE", "");
        return 'response language to automatic';
    } else if (value in knownLanguageMap) {
        config.setSetting("LANGUAGE_CODE", value);
        return 'response language to ' + knownLanguageMap[value];
    } else {
        console.log("Unknown response language: " + value);
        return null;
    }
}

var vibePatternMap = {
    'Reveille': "1",
    'Mario': "2",
    'Nudge Nudge': "3",
    'Jackhammer': "4",
    'Standard': "5",
}

function updateAlarmVibrationPattern(session, value) {
    if (value in vibePatternMap) {
        config.setSetting('ALARM_VIBE_PATTERN', vibePatternMap[value]);
        session.enqueue({
            'ALARM_VIBE_PATTERN': vibePatternMap[value]
        });
        return 'alarm vibration pattern to ' + value;
    } else {
        console.log("Unknown alarm vibration pattern: " + value);
    }
}

function updateTimerVibrationPattern(session, value) {
    if (value in vibePatternMap) {
        config.setSetting('TIMER_VIBE_PATTERN', vibePatternMap[value]);
        session.enqueue({
            'TIMER_VIBE_PATTERN': vibePatternMap[value]
        });
        return 'timer vibration pattern to ' + value;
    } else {
        console.log("Unknown timer vibration pattern: " + value);
    }
}

function updateQuickLaunchBehaviour(session, value) {
    var behaviourMap = {
        'start conversation and time out': "1",
        'start conversation and stay open': "2",
        'open home screen': "3",
    };
    if (value in behaviourMap) {
        config.setSetting('QUICK_LAUNCH_BEHAVIOUR', behaviourMap[value]);
        session.enqueue({
            'QUICK_LAUNCH_BEHAVIOUR': behaviourMap[value]
        });
        return 'quick launch behaviour to ' + value;
    } else {
        console.log("Unknown quick launch behaviour: " + value);
    }
}

function updateConfirmPrompts(session, value) {
    config.setSetting('CONFIRM_TRANSCRIPTS', value);
    session.enqueue({
        'CONFIRM_TRANSCRIPTS': value
    });
    if (value) {
        return 'prompt confirmation to on';
    } else {
        return 'prompt confirmation to off';
    }
}

exports.updateSettings = function(session, message, callback) {
    console.log("Updating settings...");
    var summaries = [];
    for (var key in message) {
        summaries.push(settingUpdater(session, key, message[key]));
    }
    var summary = summaries.filter(function(s) { return !!s; }).join(", ");
    if (summary === "") {
        summary = "No settings changed.";
    } else {
        summary = "Set " + summary + ".";
    }
    session.enqueue({
        ACTION_SETTINGS_UPDATED: summary,
    });
    callback({"status": "ok"});
}
