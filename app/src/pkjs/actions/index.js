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

var reminders = require('./reminders');
var alarms = require('./alarms');
var config = require('../config.js');

var actionMap = {
    'set_reminder': reminders.setReminder,
    'get_reminders': reminders.getReminders,
    'delete_reminder': reminders.deleteReminder,
    'set_alarm': alarms.setAlarm,
    'get_alarm': alarms.getAlarm,
};

var extraActions = ['named_alarms'];

exports.handleAction = function(session, ws, actionParamString) {
    var params = JSON.parse(actionParamString);
    var name = params['action'];
    console.log("got an action: ", actionParamString);
    if (name in actionMap) {
        actionMap[name](session, params, function(result) {
            console.log("Sending websocket response...");
            ws.send(JSON.stringify(result));
            console.log("Send");
        })
    } else {
        ws.send(JSON.stringify({"error": "Unknown action '" + name + "'."}));
    }
}

exports.getSupportedActions = function() {
    var results = [];
    for (var name in actionMap) {
        results.push(name);
    }
    return results.concat(extraActions);
}
