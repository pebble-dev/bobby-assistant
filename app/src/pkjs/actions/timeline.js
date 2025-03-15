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

// The timeline public URL root
var API_URL_ROOT = 'https://timeline-api.rebble.io/';

function timelineRequest(pin, type, topics, apiKey, callback) {
    // User or shared?
    var url = API_URL_ROOT + 'v1/' + ((topics != null) ? 'shared/' : 'user/') + 'pins/' + pin.id;

    // Create XHR
    var xhr = new XMLHttpRequest();
    xhr.onload = function () {
        console.log('timeline: response received: ' + this.responseText);
        if (callback) {
            callback(this.responseText);
        }
    };
    xhr.open(type, url);

    // Set headers
    xhr.setRequestHeader('Content-Type', 'application/json');
    if(topics != null) {
        xhr.setRequestHeader('X-Pin-Topics', '' + topics.join(','));
        xhr.setRequestHeader('X-API-Key', '' + apiKey);
    }

    // Get token
    Pebble.getTimelineToken(function(token) {
        // Add headers
        xhr.setRequestHeader('X-User-Token', '' + token);

        // Send
        xhr.send(JSON.stringify(pin));
        console.log('timeline: request sent.');
    }, function(error) { console.log('timeline: error getting timeline token: ' + error); });
}

// Insert a pin into the timeline
exports.insertUserPin = function(pin, callback) {
    timelineRequest(pin, 'PUT', null, null, callback);
};

// Delete a pin from the timeline
exports.deleteUserPin = function(pinId, callback) {
    var pin = { "id": pinId };
    timelineRequest(pin, 'DELETE', null, null, callback);
};
