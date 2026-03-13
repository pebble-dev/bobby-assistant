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

var urls = require('./urls');

exports.getSettings = function() {
    return JSON.parse(localStorage.getItem('clay-settings')) || {};
}

exports.setSetting = function(key, value) {
    var settings = exports.getSettings();
    settings[key] = value;
    localStorage.setItem('clay-settings', JSON.stringify(settings));
}

exports.isLocationEnabled = function() {
    return !!exports.getSettings()['LOCATION_ENABLED'];
}

// Telegram authentication functions

/**
 * Start the Telegram login flow.
 * @param {string} token - Pebble timeline token for authentication
 * @returns {Promise<{deepLink: string, sessionId: string, expiresIn: number}>}
 */
exports.startTelegramLogin = function(token) {
    return new Promise(function(resolve, reject) {
        var xhr = new XMLHttpRequest();
        xhr.open('POST', urls.TELEGRAM_AUTH_START_URL, true);
        xhr.setRequestHeader('Content-Type', 'application/json');
        xhr.onreadystatechange = function() {
            if (xhr.readyState === 4) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText);
                        resolve({
                            deepLink: response.deep_link,
                            sessionId: response.session_id,
                            expiresIn: response.expires_in
                        });
                    } catch (e) {
                        reject(new Error('Invalid response: ' + xhr.responseText));
                    }
                } else {
                    reject(new Error('Failed to start login: ' + xhr.status + ' ' + xhr.responseText));
                }
            }
        };
        xhr.send(JSON.stringify({ token: token }));
    });
}

/**
 * Poll for Telegram login status.
 * @param {string} sessionId - Login session ID
 * @param {string} token - Pebble timeline token for authentication
 * @returns {Promise<{state: string, error?: string}>}
 */
exports.pollTelegramLogin = function(sessionId, token) {
    return new Promise(function(resolve, reject) {
        var xhr = new XMLHttpRequest();
        var url = urls.TELEGRAM_AUTH_STATUS_URL + '/' + sessionId + '?token=' + encodeURIComponent(token);
        xhr.open('GET', url, true);
        xhr.onreadystatechange = function() {
            if (xhr.readyState === 4) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText);
                        resolve({
                            state: response.state,
                            error: response.error
                        });
                    } catch (e) {
                        reject(new Error('Invalid response: ' + xhr.responseText));
                    }
                } else {
                    reject(new Error('Failed to poll status: ' + xhr.status));
                }
            }
        };
        xhr.send();
    });
}

/**
 * Check if user has an active Telegram session.
 * @param {string} token - Pebble timeline token for authentication
 * @returns {Promise<{connected: boolean, botUsername?: string}>}
 */
exports.checkTelegramConnection = function(token) {
    return new Promise(function(resolve, reject) {
        var xhr = new XMLHttpRequest();
        var url = urls.TELEGRAM_AUTH_CHECK_URL + '?token=' + encodeURIComponent(token);
        xhr.open('GET', url, true);
        xhr.onreadystatechange = function() {
            if (xhr.readyState === 4) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText);
                        resolve({
                            connected: response.connected,
                            botUsername: response.bot_username
                        });
                    } catch (e) {
                        reject(new Error('Invalid response: ' + xhr.responseText));
                    }
                } else {
                    reject(new Error('Failed to check connection: ' + xhr.status));
                }
            }
        };
        xhr.send();
    });
}

/**
 * Disconnect Telegram session.
 * @param {string} token - Pebble timeline token for authentication
 * @returns {Promise<{success: boolean}>}
 */
exports.disconnectTelegram = function(token) {
    return new Promise(function(resolve, reject) {
        var xhr = new XMLHttpRequest();
        xhr.open('POST', urls.TELEGRAM_AUTH_LOGOUT_URL, true);
        xhr.setRequestHeader('Content-Type', 'application/json');
        xhr.onreadystatechange = function() {
            if (xhr.readyState === 4) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText);
                        resolve({ success: response.success });
                    } catch (e) {
                        reject(new Error('Invalid response: ' + xhr.responseText));
                    }
                } else {
                    reject(new Error('Failed to disconnect: ' + xhr.status));
                }
            }
        };
        xhr.send(JSON.stringify({ token: token }));
    });
}

/**
 * Set the OpenClaw bot username.
 * @param {string} token - Pebble timeline token for authentication
 * @param {string} botUsername - OpenClaw bot username
 * @returns {Promise<{success: boolean}>}
 */
exports.setTelegramBotUsername = function(token, botUsername) {
    return new Promise(function(resolve, reject) {
        var xhr = new XMLHttpRequest();
        xhr.open('POST', urls.TELEGRAM_SET_BOT_URL, true);
        xhr.setRequestHeader('Content-Type', 'application/json');
        xhr.onreadystatechange = function() {
            if (xhr.readyState === 4) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText);
                        resolve({ success: response.success });
                    } catch (e) {
                        reject(new Error('Invalid response: ' + xhr.responseText));
                    }
                } else {
                    reject(new Error('Failed to set bot username: ' + xhr.status));
                }
            }
        };
        xhr.send(JSON.stringify({ token: token, bot_username: botUsername }));
    });
}

/**
 * Wait for Telegram login to complete, polling until done.
 * @param {string} sessionId - Login session ID
 * @param {string} token - Pebble timeline token for authentication
 * @param {number} timeout - Maximum time to wait in milliseconds
 * @param {function} onProgress - Callback for status updates
 * @returns {Promise<{success: boolean, error?: string}>}
 */
exports.waitForTelegramLogin = function(sessionId, token, timeout, onProgress) {
    var pollInterval = 1000; // Poll every second
    var startTime = Date.now();

    return new Promise(function(resolve, reject) {
        function poll() {
            exports.pollTelegramLogin(sessionId, token).then(function(result) {
                if (onProgress) {
                    onProgress(result.state);
                }

                if (result.state === 'complete') {
                    resolve({ success: true });
                } else if (result.state === 'failed') {
                    resolve({ success: false, error: result.error || 'Login failed' });
                } else if (result.state === 'expired') {
                    resolve({ success: false, error: 'Login timed out' });
                } else if (Date.now() - startTime > timeout) {
                    resolve({ success: false, error: 'Polling timed out' });
                } else {
                    // Continue polling
                    setTimeout(poll, pollInterval);
                }
            }).catch(function(error) {
                reject(error);
            });
        }

        poll();
    });
}
