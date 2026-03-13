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

// This function will be serialised and sent into another context, so it cannot reference anything
// that is not textually inside it.
module.exports = function(minified) {
    var clayConfig = this;

    // API URLs - these need to be defined inline since we can't reference external modules
    var API_BASE = 'https://bobby-api.rebble.io';

    // Helper function to make HTTP requests
    function httpPost(url, data) {
        return new Promise(function(resolve, reject) {
            var xhr = new XMLHttpRequest();
            xhr.open('POST', url, true);
            xhr.setRequestHeader('Content-Type', 'application/json');
            xhr.onreadystatechange = function() {
                if (xhr.readyState === 4) {
                    if (xhr.status === 200) {
                        try {
                            resolve(JSON.parse(xhr.responseText));
                        } catch (e) {
                            reject(new Error('Invalid response: ' + xhr.responseText));
                        }
                    } else {
                        reject(new Error('Request failed: ' + xhr.status));
                    }
                }
            };
            xhr.send(JSON.stringify(data));
        });
    }

    function httpGet(url) {
        return new Promise(function(resolve, reject) {
            var xhr = new XMLHttpRequest();
            xhr.open('GET', url, true);
            xhr.onreadystatechange = function() {
                if (xhr.readyState === 4) {
                    if (xhr.status === 200) {
                        try {
                            resolve(JSON.parse(xhr.responseText));
                        } catch (e) {
                            reject(new Error('Invalid response: ' + xhr.responseText));
                        }
                    } else {
                        reject(new Error('Request failed: ' + xhr.status));
                    }
                }
            };
            xhr.send();
        });
    }

    // Telegram connection status
    var telegramStatusText = clayConfig.getItemByMessageKey('telegramStatus');
    var connectBtn = clayConfig.getItemByMessageKey('TELEGRAM_CONNECT');
    var botInput = clayConfig.getItemByMessageKey('OPENCLAW_BOT');
    var currentSessionId = null;
    var pollTimeout = null;

    // Update status text
    function setStatus(text, isError) {
        if (telegramStatusText) {
            telegramStatusText.set('value', text);
            if (isError) {
                telegramStatusText.element.style.color = 'red';
            } else {
                telegramStatusText.element.style.color = '';
            }
        }
    }

    // Get timeline token from Pebble
    function getTimelineToken() {
        return new Promise(function(resolve, reject) {
            if (typeof Pebble !== 'undefined' && Pebble.getTimelineToken) {
                Pebble.getTimelineToken(resolve, reject);
            } else {
                // In emulator or test mode, use a placeholder
                resolve('test-token');
            }
        });
    }

    // Check current Telegram connection status
    function checkTelegramStatus() {
        getTimelineToken().then(function(token) {
            return httpGet(API_BASE + '/telegram/auth/check?token=' + encodeURIComponent(token));
        }).then(function(response) {
            if (response.connected) {
                setStatus('Connected' + (response.bot_username ? ' (' + response.bot_username + ')' : ''));
                if (connectBtn) {
                    connectBtn.set('defaultValue', 'Disconnect from Telegram');
                }
            } else {
                setStatus('Not connected');
                if (connectBtn) {
                    connectBtn.set('defaultValue', 'Connect to Telegram');
                }
            }
        }).catch(function(error) {
            setStatus('Error checking status', true);
            console.log('Error checking Telegram status:', error);
        });
    }

    // Start Telegram login
    function startTelegramLogin() {
        var botUsername = botInput ? botInput.get('value') : '@OpenClawBot';
        var token;

        setStatus('Starting login...');
        getTimelineToken().then(function(t) {
            token = t;
            return httpPost(API_BASE + '/telegram/auth/start', { token: token });
        }).then(function(response) {
            currentSessionId = response.session_id;
            setStatus('Opening Telegram...');

            // Open the deep link in Telegram
            if (typeof Pebble !== 'undefined' && Pebble.openURL) {
                Pebble.openURL(response.deep_link);
            } else {
                window.open(response.deep_link, '_blank');
            }

            // Start polling for completion
            pollForCompletion(token, response.session_id, response.expires_in * 1000);

        }).catch(function(error) {
            setStatus('Error: ' + error.message, true);
            console.log('Error starting Telegram login:', error);
        });
    }

    // Poll for login completion
    function pollForCompletion(token, sessionId, timeout) {
        var startTime = Date.now();

        function poll() {
            httpGet(API_BASE + '/telegram/auth/status/' + sessionId + '?token=' + encodeURIComponent(token))
                .then(function(response) {
                    if (response.state === 'complete') {
                        setStatus('Connected!');

                        // Set the bot username
                        var botUsername = botInput ? botInput.get('value') : '@OpenClawBot';
                        return httpPost(API_BASE + '/telegram/auth/bot', {
                            token: token,
                            bot_username: botUsername
                        });
                    } else if (response.state === 'failed') {
                        setStatus('Error: ' + (response.error || 'Login failed'), true);
                    } else if (response.state === 'expired') {
                        setStatus('Login timed out', true);
                    } else if (Date.now() - startTime > timeout) {
                        setStatus('Connection timed out', true);
                    } else {
                        // Continue polling
                        pollTimeout = setTimeout(poll, 1000);
                    }
                }).catch(function(error) {
                    setStatus('Error polling status: ' + error.message, true);
                });
        }

        poll();
    }

    // Disconnect Telegram
    function disconnectTelegram() {
        setStatus('Disconnecting...');
        getTimelineToken().then(function(token) {
            return httpPost(API_BASE + '/telegram/auth/logout', { token: token });
        }).then(function() {
            setStatus('Not connected');
            if (connectBtn) {
                connectBtn.set('defaultValue', 'Connect to Telegram');
            }
        }).catch(function(error) {
            setStatus('Error disconnecting: ' + error.message, true);
            console.log('Error disconnecting:', error);
        });
    }

    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
        // Check Telegram connection status
        checkTelegramStatus();

        // Handle connect/disconnect button
        if (connectBtn) {
            connectBtn.on('click', function() {
                var currentValue = connectBtn.get('defaultValue');
                if (currentValue === 'Disconnect from Telegram') {
                    disconnectTelegram();
                } else {
                    startTelegramLogin();
                }
            });
        }
    });
}
