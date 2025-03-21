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

var location = require('./location');
var session = require('./session');
var quota = require('./quota');
var Clay = require('pebble-clay');
var clayConfig = require('./config.json');
var customConfigFunction = require('./custom_config');
var config = require('./config');
var reminders = require('./reminders');
var feedback = require('./feedback');
var package_json = require('package.json');


var clay = new Clay(clayConfig, customConfigFunction);

function main() {
    doQuotaWarning();
    location.update();
    Pebble.addEventListener('appmessage', handleAppMessage);
}

function doQuotaWarning() {
    quota.fetchQuota(function(response) {
        if (!response.hasSubscription) {
            Pebble.showSimpleNotificationOnPebble(
                "Subscription Needed",
                "In order to use Bobby, you need a Rebble subscription. You can sign up for a subscription at auth.rebble.io."
            );
        }
    });
}

function handleAppMessage(e) {
    console.log("Inbound app message!");
    console.log(JSON.stringify(e));
    var data = e.payload;
    if (data.PROMPT) {
        console.log("Starting a new Session...");
        var s = new session.Session(data.PROMPT, data.THREAD_ID);
        s.run();
        return;
    }

    if (reminders.handleReminderMessage(data)) {
        return;
    }

    if (data.QUOTA_REQUEST) {
        console.log("Requesting quota...");
        quota.handleQuotaRequest();
    }
    if ('LOCATION_ENABLED' in data) {
        config.setSetting("LOCATION_ENABLED", !!data.LOCATION_ENABLED);
        console.log("Location enabled: " + config.isLocationEnabled());
        // We need to confirm that we received this for the watch to proceed.
        Pebble.sendAppMessage({
            LOCATION_ENABLED: data.LOCATION_ENABLED,
        });
    }
    if ('FEEDBACK_TEXT' in data) {
        console.log("Handling feedback...");
        feedback.handleFeedbackRequest(data);
    }
    if ('REPORT_THREAD_UUID' in data) {
        console.log("Handling report...");
        feedback.handleReportRequest(data);
    }
}

Pebble.addEventListener("ready",
    function(e) {
        console.log("Bobby " + package_json['version']);
        Pebble.getTimelineToken(function(token) {
            session.userToken = token;
            main();
        }, function(e) {
            console.log("Get timeline token failed???", e);
        })
    }
);
