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

var message_keys = require('message_keys');

function setAlarm(session, message, callback) {
    var time = message['time'];
    var name = message['name'] || null;
    var isTimer = !!message['isTimer'];
    var cancelling = !!message['cancel'];
    var date = new Date(time);
    var unixTime = date.getTime() / 1000;
    if (isTimer && !cancelling) {
        var duration = message['duration'];
        if (!duration) {
            callback({"error": "Need to specify a duration for a timer."});
            return;
        }
        // the real unix time will be calculated by the watch to avoid time sync errors.
        unixTime = duration;
    }
    if (isNaN(unixTime) && (!cancelling)) {
        callback({"error": "Invalid time string '" + time + "'."});
        return;
    }
    if (isNaN(unixTime)) {
        unixTime = 0;
    }
    console.log("Trying to set " + (isTimer ? "timer" : "alarm") + " for " + unixTime + " (" + time + ")");
    
    var timeout = setTimeout(2000, function() {
        console.log("Timed out, returning error message.");
        callback({"error": "Timed out waiting for a response from the watch."});
        cleanup();
    });
    
    var cleanup = function() {
        console.log("Cleaning up alarm handling.");
        Pebble.removeEventListener('appmessage', handleMessage);
        clearTimeout(timeout);
    }
    
    // We need to temporarily hook up to the appmessage system so we can pick up our response.
    var handleMessage = function(event) {
        var data = event.payload;
        if ('SET_ALARM_RESULT' in data) {
            var result = data['SET_ALARM_RESULT'];
            console.log("Got an alarm response: " + result);
            switch (result) {
            case 0: // S_SUCCESS
                callback({"status": "ok"});
                break;
            case -3: // E_INTERNAL
                callback({"error": "An internal error occurred."});
                break;
            case -4: // E_INVALID_ARGUMENT
                if (cancelling) {
                    if (!time) {
                        callback({"error": "No " + (isTimer ? "timers" : "alarms") + " were set."});
                        break;
                    }
                    callback({"error": "There is no alarm set for that time."});
                    break;
                }
                callback({"error": "The alarm time given was in the past."});
                break;
            case -7: // E_OUT_OF_RESOURCES
                callback({"error": "The limit of eight alarms was already reached."});
                break;
            case -8: // E_RANGE
                callback({"error": "Something is already scheduled on your Pebble for that time."});
                break;
            default:
                callback({"error": "Something unexpected went wrong."});
                break;
            }
            cleanup();
        }
    };
    Pebble.addEventListener('appmessage', handleMessage);
    if (!cancelling) {
        var request = {
            SET_ALARM_TIME: unixTime,
            SET_ALARM_IS_TIMER: isTimer,
        };
        if (name) {
            request.SET_ALARM_NAME = name;
        }
        session.enqueue(request);
    } else {
        session.enqueue({
            CANCEL_ALARM_TIME: unixTime,
            CANCEL_ALARM_IS_TIMER: isTimer,
        });
    }
    console.log("Sent set_alarm request to Pebble.");
}

function getAlarm(session, message, callback) {
    var isTimer = message['isTimer'];
    console.log("Getting " + (isTimer ? "timers" : "alarms") + "...");


    var timeout = setTimeout(2000, function() {
        console.log("Timed out, returning error message.");
        callback({"error": "Timed out waiting for a response from the watch."});
        cleanup();
    });

    var cleanup = function() {
        console.log("Cleaning up alarm handling.");
        Pebble.removeEventListener('appmessage', handleMessage);
        clearTimeout(timeout);
    }

    var leftPad2 = function(number) {
        if (number == 0) {
            return "00";
        }
        if (number < 10) {
            return "0" + number;
        }
        return "" + number;
    }

    var handleMessage = function(event) {
        var data = event.payload;
        if (!(message_keys.GET_ALARM_RESULT in data)) {
            return;
        }
        console.log("got response from watch");
        var obj = {"status": "ok"};
        var resp = [];
        obj[isTimer ? "timers": "alarms"] = resp;
        var watchTime = data.CURRENT_TIME;
        var count = data[message_keys.GET_ALARM_RESULT];
        for (var i = 0; i < count; ++i) {
            var alarmTime = data[message_keys.GET_ALARM_RESULT + i + 1];
            var alarmName = data[message_keys.GET_ALARM_NAME + i + 1] || null;
            if (isTimer) {
                var secondsLeft = alarmTime - watchTime;
                var formattedTimeLeft = "";
                var hours = Math.floor(secondsLeft / 3600);
                var minutes = Math.floor((secondsLeft % 3600) / 60);
                var seconds = secondsLeft % 60;
                formattedTimeLeft = hours + ":" + leftPad2(minutes) + ":" + leftPad2(seconds);
                var expirationDate = new Date(alarmTime * 1000);
                var r = {"secondsLeft": secondsLeft, "formattedTimeLeft": formattedTimeLeft, "expirationTimeForDeletingAndWidgets": expirationDate.toISOString()};
                if (alarmName) {
                    r.name = alarmName;
                }
                resp.push(r);
            } else {
                var date = new Date(alarmTime * 1000);
                var formatted = (1900 + date.getYear()) + "-" + leftPad2(date.getMonth() + 1) + "-" + leftPad2(date.getDate()) + "T" + leftPad2(date.getHours()) + ":" + leftPad2(date.getMinutes()) + ":" + leftPad2(date.getSeconds());
                var tzOffset = -date.getTimezoneOffset();
                formatted += (tzOffset < 0) ? "-" : "+";
                if (tzOffset < 0) {
                    tzOffset = -tzOffset;
                }
                formatted += leftPad2(Math.floor(tzOffset / 60)) + ":" + leftPad2(tzOffset % 60);
                var r = {"time": formatted};
                if (alarmName) {
                    r.name = alarmName;
                }
                resp.push(r);
            }
        }
        callback(obj);
        cleanup();
    }
    Pebble.addEventListener('appmessage', handleMessage);
    session.enqueue({
        GET_ALARM_OR_TIMER: isTimer,
    });
}

exports.setAlarm = setAlarm;
exports.getAlarm = getAlarm;
