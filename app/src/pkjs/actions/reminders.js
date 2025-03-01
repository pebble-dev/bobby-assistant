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

var timeline = require('./timeline');

exports.setReminder = function(message, callback) {
    var when = message['time'];
    var what = message['what'];
    var date = (new Date(when)).toISOString();
    console.log("Setting a reminder: \"" + what + "\" at " + date);
    var pin = {
        "id": "bobby-reminder-" + Math.random(),
        "time": date,
        "layout": {
            "type": "genericPin",
            "title": what,
            "tinyIcon": "system://images/NOTIFICATION_REMINDER",
        },
        "reminders": [{
            "time": date,
            "layout": {
                "type": "genericReminder",
                "title": what,
                "tinyIcon": "system://images/NOTIFICATION_REMINDER",
            },
        }],
    };
    timeline.insertUserPin(pin, function() {
        var unixTime = (new Date(when)).getTime() / 1000;
        Pebble.sendAppMessage({ACTION_REMINDER_WAS_SET: unixTime});
        if (unixTime < (new Date()).getTime() / 1000 + 3600) {
            callback({"warning": "Your reminder was set. It is **critical** you warn the user: Due to timeline delays, reminders set in the near future may not appear on time."});
        } else {
            callback({"status": "ok"});
        }
        console.log("Done.");
    });
}
