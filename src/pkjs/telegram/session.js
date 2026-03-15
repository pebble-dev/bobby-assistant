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
 * Session storage for Telegram authentication.
 * Stores the Telegram session in localStorage, optionally encrypted.
 */

var SESSION_KEY = 'telegram_session';
var BOT_USERNAME_KEY = 'openclaw_bot_username';

/**
 * Save Telegram session to localStorage.
 * @param {string} sessionData - The session string from GramJS
 */
function saveSession(sessionData) {
    try {
        // For security, we could encrypt this, but for simplicity we store it directly
        // In a production app, consider using the Web Crypto API for encryption
        localStorage.setItem(SESSION_KEY, sessionData);
        console.log('Telegram session saved');
    } catch (err) {
        console.error('Failed to save session:', err);
    }
}

/**
 * Load Telegram session from localStorage.
 * @returns {string|null} The session string or null if not found
 */
function loadSession() {
    try {
        var sessionData = localStorage.getItem(SESSION_KEY);
        if (sessionData) {
            console.log('Telegram session loaded');
            return sessionData;
        }
    } catch (err) {
        console.error('Failed to load session:', err);
    }
    return null;
}

/**
 * Clear the Telegram session from localStorage.
 */
function clearSession() {
    try {
        localStorage.removeItem(SESSION_KEY);
        console.log('Telegram session cleared');
    } catch (err) {
        console.error('Failed to clear session:', err);
    }
}

/**
 * Check if a session exists.
 * @returns {boolean}
 */
function hasSession() {
    return localStorage.getItem(SESSION_KEY) !== null;
}

/**
 * Save the OpenClaw bot username.
 * @param {string} username - Bot username (e.g., "@OpenClawBot")
 */
function saveBotUsername(username) {
    try {
        // Normalize username
        if (username && !username.startsWith('@')) {
            username = '@' + username;
        }
        localStorage.setItem(BOT_USERNAME_KEY, username);
        console.log('Bot username saved:', username);
    } catch (err) {
        console.error('Failed to save bot username:', err);
    }
}

/**
 * Get the OpenClaw bot username.
 * @returns {string} The bot username or default
 */
function getBotUsername() {
    var username = localStorage.getItem(BOT_USERNAME_KEY);
    if (!username) {
        // Get from clay-settings as fallback
        var settings = JSON.parse(localStorage.getItem('clay-settings')) || {};
        username = settings.OPENCLAW_BOT || '@OpenClawBot';
    }
    // Normalize
    if (username && !username.startsWith('@')) {
        username = '@' + username;
    }
    return username || '@OpenClawBot';
}

/**
 * Get all session info.
 * @returns {object}
 */
function getSessionInfo() {
    return {
        hasSession: hasSession(),
        botUsername: getBotUsername()
    };
}

exports.saveSession = saveSession;
exports.loadSession = loadSession;
exports.clearSession = clearSession;
exports.hasSession = hasSession;
exports.saveBotUsername = saveBotUsername;
exports.getBotUsername = getBotUsername;
exports.getSessionInfo = getSessionInfo;