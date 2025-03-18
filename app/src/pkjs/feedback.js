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

var config = require('./config');
var location = require('./location');
var reminders = require('./lib/reminders');
var package_json = require('package.json');
var urls = require('./urls');
var session = require('./session');

exports.handleFeedbackRequest = function(request) {
    var text = request['FEEDBACK_TEXT'];
    var appVersion = '' + request['FEEDBACK_APP_MAJOR'] + '.' + request['FEEDBACK_APP_MINOR'];
    var alarmCount = request['FEEDBACK_ALARM_COUNT'];
    var locationEnabled = config.isLocationEnabled();
    var locationReady = location.isReady();
    var settings = config.getSettings();
    var unitPreference = settings['UNIT_PREFERENCE'] || '';
    var languagePreference = settings['LANGUAGE_CODE'] || '';
    var reminderCount = reminders.getAllReminders().length;
    var jsVersion = package_json['version'];
    var timezone = (-(new Date()).getTimezoneOffset());
    var platform = 'unknown';
    if (window.cobble) {
        platform = 'Cobble';
    } else if (window.navigator) {
        var userAgent = navigator.userAgent;
        var androidVersionRegex = /Android (\d+(?:\.\d+)?)/;
        var androidVersion = androidVersionRegex.exec(userAgent);
        if (androidVersion) {
            platform = 'Android ' + androidVersion[1];
        } else {
            platform = 'iOS';
        }
    } else {
        platform = 'iOS';
    }
    var feedback = {
        'text': text,
        'appVersion': appVersion,
        'alarmCount': alarmCount,
        'locationEnabled': locationEnabled,
        'locationReady': locationReady,
        'unitPreference': unitPreference,
        'languagePreference': languagePreference,
        'reminderCount': reminderCount,
        'jsVersion': jsVersion,
        'timezone': timezone,
        'platform': platform,
        'timelineToken': session.userToken
    }
    console.log("Feedback request: " + JSON.stringify(feedback));

    var req = new XMLHttpRequest();
    req.open('POST', urls.FEEDBACK_URL, true);
    req.setRequestHeader('Content-Type', 'application/json');
    req.onload = function(e) {
        if (req.readyState === 4) {
            if (req.status === 200) {
                console.log("Feedback sent successfully");
                Pebble.sendAppMessage({FEEDBACK_SEND_RESULT: 0});
            } else {
                console.log("Feedback request returned error code " + req.status.toString());
                Pebble.sendAppMessage({FEEDBACK_SEND_RESULT: 1});
            }
        }
    }
    req.send(JSON.stringify(feedback));
}
