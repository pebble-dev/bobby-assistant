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

var feedback = require('../lib/feedback');

exports.sendFeedback = function(session, message, callback) {
  var feedbackText = message['feedback'];
  var threadId = message['thread_id'];
  if (!feedbackText && !threadId) {
    callback({"error": "No feedback provided"});
  }
  console.log("Sending feedback: '" + feedbackText + "' threadId: " + threadId);

  feedback.sendFeedback(feedbackText, threadId, function(success, status) {
    if (success) {
      session.enqueue({ACTION_FEEDBACK_SENT: 1});
      callback({"status": "ok"});
    } else {
      session.enqueue({WARNING: "Sending feedback failed"});
      callback({"error": "Failed to send feedback: HTTP error code" + status});
    }
  });
};
