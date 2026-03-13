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

/**
 * Configuration module for Clawd.
 * Client-side configuration - no backend needed.
 */

exports.getSettings = function() {
    return JSON.parse(localStorage.getItem('clay-settings')) || {};
};

exports.setSetting = function(key, value) {
    var settings = exports.getSettings();
    settings[key] = value;
    localStorage.setItem('clay-settings', JSON.stringify(settings));
};

exports.isLocationEnabled = function() {
    return !!exports.getSettings()['LOCATION_ENABLED'];
};

/**
 * Get the OpenClaw bot username.
 * @returns {string}
 */
exports.getBotUsername = function() {
    var username = localStorage.getItem('openclaw_bot_username');
    if (!username) {
        var settings = exports.getSettings();
        username = settings['OPENCLAW_BOT'] || '@OpenClawBot';
    }
    if (username && !username.startsWith('@')) {
        username = '@' + username;
    }
    return username || '@OpenClawBot';
};

/**
 * Set the OpenClaw bot username.
 * @param {string} username
 */
exports.setBotUsername = function(username) {
    if (username && !username.startsWith('@')) {
        username = '@' + username;
    }
    localStorage.setItem('openclaw_bot_username', username);
};

/**
 * Check if Telegram is connected.
 * @returns {boolean}
 */
exports.isTelegramConnected = function() {
    return !!localStorage.getItem('telegram_session');
};