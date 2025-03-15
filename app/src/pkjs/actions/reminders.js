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

var reminders = require('../lib/reminders');

exports.setReminder = function(session, message, callback) {
  var when = message['time'];
  var what = message['what'];
  
  try {
    reminders.addReminder(what, when);
    var unixTime = (new Date(when)).getTime() / 1000;
    session.enqueue({ACTION_REMINDER_WAS_SET: unixTime});
    
    if (unixTime < (new Date()).getTime() / 1000 + 3600) {
      callback({"warning": "Your reminder was set. It is **critical** you warn the user: Due to timeline delays, reminders set in the near future may not appear on time."});
    } else {
      callback({"status": "ok"});
    }
  } catch (err) {
    callback({"error": "Failed to set reminder: " + err.message});
  }
};

exports.getReminders = function(session, message, callback) {
  try {
    var allReminders = reminders.getAllReminders();
    for (var i = 0; i < allReminders.length; i++) {
      var reminder = allReminders[i];
      reminder.time = new Date(reminder.time).toString();
    }
    callback({
      "status": "ok",
      "reminders": allReminders
    });
  } catch (err) {
    callback({"error": "Failed to get reminders: " + err.message});
  }
};

exports.deleteReminder = function(session, message, callback) {
  var reminderId = message['id'];
  if (!reminderId) {
    callback({"error": "No reminder ID provided"});
  }
  
  try {
    var success = reminders.deleteReminder(reminderId);
    if (!success) {
      callback({"error": "Reminder not found"});
    }
    session.enqueue({ACTION_REMINDER_DELETED: 1});
    callback({"status": "ok"});
  } catch (err) {
    callback({"error": "Failed to delete reminder: " + err.message});
  }
};
