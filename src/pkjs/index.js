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

// Telegram/GramJS bundle - creates global TelegramClient, StringSession, NewMessage
// Set USE_MOCK_TELEGRAM to true to use mock implementation for testing
var USE_MOCK_TELEGRAM = true;
if (USE_MOCK_TELEGRAM) {
    require('./lib/telegram-bundle-mock.js');
} else {
    require('./lib/telegram-bundle.js');
}

var location = require('./location');
var session = require('./session');
var telegram = require('./telegram');
var Clay = require('@rebble/clay');
var clayConfig = require('./config.json');
var customConfigFunction = require('./custom_config');
var config = require('./config');
var reminders = require('./reminders');
var package_json = require('package.json');


var clay = new Clay(clayConfig, customConfigFunction);

function main() {
    location.update();
    sendTelegramStatus();
    Pebble.addEventListener('appmessage', handleAppMessage);
}

function sendTelegramStatus() {
    var isConnected = telegram.hasSession();
    console.log('Telegram connected: ' + isConnected);
    Pebble.sendAppMessage({
        TELEGRAM_CONNECTED: isConnected ? 1 : 0
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

    if ('LOCATION_ENABLED' in data) {
        config.setSetting("LOCATION_ENABLED", !!data.LOCATION_ENABLED);
        console.log("Location enabled: " + config.isLocationEnabled());
        // We need to confirm that we received this for the watch to proceed.
        Pebble.sendAppMessage({
            LOCATION_ENABLED: data.LOCATION_ENABLED,
        });
    }
}

function doCobbleWarning() {
    if (window.cobble) {
        console.log("WARNING: Running Clawd on Cobble is not supported, and has multiple known issues.");
        Pebble.sendAppMessage({COBBLE_WARNING: 1});
    }
}

Pebble.addEventListener("ready",
    function(e) {
        // This happens before anything else because I don't trust Cobble to get through the normal flow,
        // given how many things bizarrely don't work.
        doCobbleWarning();
        console.log("Clawd " + package_json['version']);

        // Timeline token only available on real devices, not emulator
        if (Pebble.platform !== 'pypkjs' && Pebble.getTimelineToken) {
            Pebble.getTimelineToken(function(token) {
                session.userToken = token;
                main();
            }, function(e) {
                console.log("Get timeline token failed???", e);
                main(); // Continue anyway
            });
        } else {
            console.log("Entering emulator mode.");
            main();
        }
    }
);

// Export function to notify watch of Telegram status changes
exports.updateTelegramStatus = function() {
    sendTelegramStatus();
};