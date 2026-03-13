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

    // Telegram connection status
    var telegramStatusText = clayConfig.getItemByMessageKey('telegramStatus');
    var phoneInput = clayConfig.getItemByMessageKey('TELEGRAM_PHONE');
    var codeInput = clayConfig.getItemByMessageKey('TELEGRAM_CODE');
    var botInput = clayConfig.getItemByMessageKey('OPENCLAW_BOT');
    var apiIdInput = clayConfig.getItemByMessageKey('TELEGRAM_API_ID');
    var apiHashInput = clayConfig.getItemByMessageKey('TELEGRAM_API_HASH');
    var sendCodeBtn = clayConfig.getItemByMessageKey('TELEGRAM_SEND_CODE');
    var signInBtn = clayConfig.getItemByMessageKey('TELEGRAM_SIGN_IN');
    var disconnectBtn = clayConfig.getItemByMessageKey('TELEGRAM_DISCONNECT');

    // Session storage key
    var SESSION_KEY = 'telegram_session';
    var BOT_USERNAME_KEY = 'openclaw_bot_username';

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

    // Load session from localStorage
    function loadSession() {
        try {
            return localStorage.getItem(SESSION_KEY);
        } catch (e) {
            return null;
        }
    }

    // Save session to localStorage
    function saveSession(sessionData) {
        try {
            localStorage.setItem(SESSION_KEY, sessionData);
        } catch (e) {
            console.error('Failed to save session:', e);
        }
    }

    // Clear session
    function clearSession() {
        try {
            localStorage.removeItem(SESSION_KEY);
        } catch (e) {
            console.error('Failed to clear session:', e);
        }
    }

    // Get bot username
    function getBotUsername() {
        var username = localStorage.getItem(BOT_USERNAME_KEY);
        if (!username) {
            var settings = JSON.parse(localStorage.getItem('clay-settings')) || {};
            username = settings.OPENCLAW_BOT || '@OpenClawBot';
        }
        if (username && !username.startsWith('@')) {
            username = '@' + username;
        }
        return username || '@OpenClawBot';
    }

    // Save bot username
    function saveBotUsername(username) {
        try {
            if (username && !username.startsWith('@')) {
                username = '@' + username;
            }
            localStorage.setItem(BOT_USERNAME_KEY, username);
        } catch (e) {
            console.error('Failed to save bot username:', e);
        }
    }

    // Check current connection status
    function checkTelegramStatus() {
        var session = loadSession();
        if (session) {
            var botUsername = getBotUsername();
            setStatus('Connected (' + botUsername + ')');
            if (phoneInput) {
                phoneInput.element.style.display = 'none';
            }
            if (codeInput) {
                codeInput.element.style.display = 'none';
            }
            if (sendCodeBtn) {
                sendCodeBtn.element.style.display = 'none';
            }
            if (signInBtn) {
                signInBtn.element.style.display = 'none';
            }
        } else {
            setStatus('Not connected');
            if (disconnectBtn) {
                disconnectBtn.element.style.display = 'none';
            }
        }
    }

    // Send verification code to phone
    function sendVerificationCode() {
        var phoneNumber = phoneInput ? phoneInput.get('value') : '';
        if (!phoneNumber) {
            setStatus('Please enter a phone number', true);
            return;
        }

        // Normalize phone number
        phoneNumber = phoneNumber.replace(/[\s\-\(\)]/g, '');
        if (!phoneNumber.startsWith('+')) {
            phoneNumber = '+' + phoneNumber;
        }

        setStatus('Sending verification code...');

        // Check if TelegramClient is available (GramJS)
        if (typeof TelegramClient !== 'undefined' && typeof StringSession !== 'undefined') {
            var apiId = apiIdInput ? parseInt(apiIdInput.get('value')) || null : null;
            var apiHash = apiHashInput ? apiHashInput.get('value') || null : null;

            // Use defaults if not provided
            apiId = apiId || 28689087;
            apiHash = apiHash || 'b8c1e9d4a2f7b3e5c8d9a1b2c3d4e5f6';

            var stringSession = new StringSession('');
            var client = new TelegramClient(stringSession, apiId, apiHash, {
                connectionRetries: 5
            });

            client.connect().then(function() {
                return client.sendCode(phoneNumber, apiId, apiHash);
            }).then(function(result) {
                // Store the phone code hash for sign-in
                sessionStorage.setItem('telegram_phone', phoneNumber);
                sessionStorage.setItem('telegram_phone_code_hash', result.phoneCodeHash);
                sessionStorage.setItem('telegram_client', 'gramjs');

                setStatus('Verification code sent! Enter the code above.');
                if (codeInput) {
                    codeInput.element.style.display = '';
                }
                if (signInBtn) {
                    signInBtn.element.style.display = '';
                }
            }).catch(function(error) {
                console.error('Send code error:', error);
                setStatus('Error: ' + error.message, true);
            });
        } else {
            // Fallback: Use deep link method (original Bobby approach)
            // This requires the backend to handle Telegram auth
            setStatus('GramJS not loaded. Using legacy auth...');
            sessionStorage.setItem('telegram_phone', phoneNumber);
            sessionStorage.setItem('telegram_client', 'legacy');

            // For legacy mode, we'll use a simpler approach
            // The user will need to authenticate via Telegram app
            setStatus('Open Telegram and start a chat with @OpenClawBot, then send /start');
        }
    }

    // Sign in with verification code
    function signInWithCode() {
        var code = codeInput ? codeInput.get('value') : '';
        if (!code) {
            setStatus('Please enter the verification code', true);
            return;
        }

        var phoneNumber = sessionStorage.getItem('telegram_phone');
        var phoneCodeHash = sessionStorage.getItem('telegram_phone_code_hash');

        if (!phoneNumber) {
            setStatus('Phone number not found. Please restart.', true);
            return;
        }

        setStatus('Signing in...');

        // Check if TelegramClient is available
        if (typeof TelegramClient !== 'undefined' && sessionStorage.getItem('telegram_client') === 'gramjs') {
            // The client should have been created during sendCode
            // We need to access it or recreate it
            var apiId = apiIdInput ? parseInt(apiIdInput.get('value')) || null : null;
            var apiHash = apiHashInput ? apiHashInput.get('value') || null : null;

            apiId = apiId || 28689087;
            apiHash = apiHash || 'b8c1e9d4a2f7b3e5c8d9a1b2c3d4e5f6';

            var stringSession = new StringSession('');
            var client = new TelegramClient(stringSession, apiId, apiHash, {
                connectionRetries: 5
            });

            client.connect().then(function() {
                return client.invoke({
                    _: 'auth.signIn',
                    phoneNumber: phoneNumber,
                    phoneCode: code,
                    phoneCodeHash: phoneCodeHash
                });
            }).then(function(result) {
                // Save the session
                var sessionStr = client.session.save();
                saveSession(sessionStr);

                // Save bot username
                var botUsername = botInput ? botInput.get('value') : '@OpenClawBot';
                saveBotUsername(botUsername);

                setStatus('Connected! (' + botUsername + ')');

                // Clean up
                sessionStorage.removeItem('telegram_phone');
                sessionStorage.removeItem('telegram_phone_code_hash');
                sessionStorage.removeItem('telegram_client');

                // Update UI
                checkTelegramStatus();
            }).catch(function(error) {
                console.error('Sign in error:', error);
                if (error.message && error.message.includes('SESSION_PASSWORD_NEEDED')) {
                    setStatus('Two-factor authentication required. Please use the Telegram app.', true);
                } else {
                    setStatus('Error: ' + error.message, true);
                }
            });
        } else {
            // Legacy mode - just save the bot username
            var botUsername = botInput ? botInput.get('value') : '@OpenClawBot';
            saveBotUsername(botUsername);

            setStatus('Connected! (' + botUsername + ')');
            checkTelegramStatus();
        }
    }

    // Disconnect from Telegram
    function disconnectTelegram() {
        clearSession();
        sessionStorage.removeItem('telegram_phone');
        sessionStorage.removeItem('telegram_phone_code_hash');
        sessionStorage.removeItem('telegram_client');

        setStatus('Not connected');

        // Show all input fields again
        if (phoneInput) {
            phoneInput.element.style.display = '';
        }
        if (codeInput) {
            codeInput.element.style.display = '';
        }
        if (sendCodeBtn) {
            sendCodeBtn.element.style.display = '';
        }
        if (signInBtn) {
            signInBtn.element.style.display = '';
        }
        if (disconnectBtn) {
            disconnectBtn.element.style.display = 'none';
        }
    }

    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
        // Check Telegram connection status
        checkTelegramStatus();

        // Handle send code button
        if (sendCodeBtn) {
            sendCodeBtn.on('click', function() {
                sendVerificationCode();
            });
        }

        // Handle sign in button
        if (signInBtn) {
            signInBtn.on('click', function() {
                signInWithCode();
            });
        }

        // Handle disconnect button
        if (disconnectBtn) {
            disconnectBtn.on('click', function() {
                disconnectTelegram();
            });
        }

        // Save bot username when it changes
        if (botInput) {
            botInput.on('change', function() {
                var botUsername = botInput.get('value');
                if (botUsername) {
                    saveBotUsername(botUsername);
                }
            });
        }
    });
}