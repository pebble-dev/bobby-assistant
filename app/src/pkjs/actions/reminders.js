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

// Store reminders in localStorage
var REMINDERS_STORAGE_KEY = 'bobby_reminders';
// Keep expired reminders for 24 hours before cleanup
var EXPIRED_REMINDER_TTL = 24 * 60 * 60 * 1000; // 24 hours in milliseconds

function loadReminders() {
    var stored = localStorage.getItem(REMINDERS_STORAGE_KEY);
    return stored ? JSON.parse(stored) : [];
}

function saveReminders(reminders) {
    localStorage.setItem(REMINDERS_STORAGE_KEY, JSON.stringify(reminders));
}

function cleanupExpiredReminders() {
    var reminders = loadReminders();
    var now = new Date();
    var cutoffTime = now.getTime() - EXPIRED_REMINDER_TTL;
    
    // Filter out reminders that are older than the cutoff time
    var activeReminders = reminders.filter(function(reminder) {
        var reminderTime = new Date(reminder.time).getTime();
        return reminderTime > cutoffTime;
    });
    
    // If we removed any reminders, save the updated list
    if (activeReminders.length < reminders.length) {
        console.log("Cleaned up " + (reminders.length - activeReminders.length) + " expired reminders");
        saveReminders(activeReminders);
    }
    
    return activeReminders;
}

exports.setReminder = function(session, message, callback) {
    // Clean up expired reminders first
    cleanupExpiredReminders();
    
    var when = message['time'];
    var what = message['what'];
    var date = (new Date(when)).toISOString();
    console.log("Setting a reminder: \"" + what + "\" at " + date);
    var reminderId = "bobby-reminder-" + Math.random();
    var pin = {
        "id": reminderId,
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
        // Store reminder locally
        var reminders = loadReminders();
        reminders.push({
            id: reminderId,
            time: date,
            what: what
        });
        saveReminders(reminders);
        
        var unixTime = (new Date(when)).getTime() / 1000;
        session.enqueue({ACTION_REMINDER_WAS_SET: unixTime});
        if (unixTime < (new Date()).getTime() / 1000 + 3600) {
            callback({"warning": "Your reminder was set. It is **critical** you warn the user: Due to timeline delays, reminders set in the near future may not appear on time."});
        } else {
            callback({"status": "ok"});
        }
        console.log("Done.");
    });
}

exports.getReminders = function(session, message, callback) {
    // Clean up expired reminders before returning the list
    var reminders = cleanupExpiredReminders();
    
    // Sort reminders by time
    reminders.sort((a, b) => new Date(a.time) - new Date(b.time));
    
    // Only return future reminders for display
    var now = new Date();
    var futureReminders = reminders.filter(r => new Date(r.time) > now);
    
    callback({
        "status": "ok",
        "reminders": futureReminders
    });
}

exports.deleteReminder = function(session, message, callback) {
    // Clean up expired reminders first
    cleanupExpiredReminders();
    
    var reminderId = message['id'];
    if (!reminderId) {
        callback({"error": "No reminder ID provided"});
        return;
    }
    
    var reminders = loadReminders();
    var reminderIndex = reminders.findIndex(r => r.id === reminderId);
    
    if (reminderIndex === -1) {
        callback({"error": "Reminder not found"});
        return;
    }
    
    // Remove from timeline
    timeline.deleteUserPin(reminderId, function() {
        // Remove from local storage
        reminders.splice(reminderIndex, 1);
        saveReminders(reminders);
        callback({"status": "ok"});
    });
}
