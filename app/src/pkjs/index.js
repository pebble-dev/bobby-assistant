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
var quota = require('./quota');
var Clay = require('pebble-clay');
var clayConfig = require('./config.json');
var customConfigFunction = require('./custom_config');
var config = require('./config');

var clay = new Clay(clayConfig, customConfigFunction);

function main() {
    Pebble.addEventListener('appmessage', handleAppMessage);
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
    if (data.QUOTA_REQUEST) {
        console.log("Requesting quota...");
        quota.fetchQuota();
    }
}

Pebble.addEventListener("ready",
    function(e) {
        Pebble.getTimelineToken(function(token) {
            session.userToken = token;
            main();
        }, function(e) {
            console.log("Get timeline token failed???", e);
        })
    }
);
