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

var session = require('./session');

var QUOTA_URL = require('./urls').QUOTA_URL;

exports.fetchQuota = function(callback) {
    var url = QUOTA_URL;
    url += '?token=' + session.userToken;
    console.log("Fetching quota from " + url);
    var req = new XMLHttpRequest();
    req.open('GET', url, true);
    req.onload = function(e) {
        if (req.readyState === 4) {
            if (req.status === 200) {
                console.log("Got quota response: " + req.responseText);
                var response = JSON.parse(req.responseText);
                callback(response);
            } else {
                console.log("Request returned error code " + req.status.toString());
            }
        }
    }
    req.send();
}

exports.handleQuotaRequest = function() {
    console.log("Requesting quota...");
    exports.fetchQuota(function(response) {
        Pebble.sendAppMessage({
            QUOTA_RESPONSE_USED: response.used,
            QUOTA_RESPONSE_REMAINING: response.remaining,
            QUOTA_HAS_SUBSCRIPTION: response.hasSubscription,
        });
    });
}
