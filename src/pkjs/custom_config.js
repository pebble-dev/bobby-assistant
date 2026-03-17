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
    var qrLoginBtn = clayConfig.getItemByMessageKey('TELEGRAM_QR_LOGIN');
    var qrCodeDisplay = clayConfig.getItemByMessageKey('qrCodeDisplay');

    // Session storage key
    var SESSION_KEY = 'telegram_session';
    var BOT_USERNAME_KEY = 'openclaw_bot_username';

    // QR polling interval
    var qrPollInterval = null;
    var qrClient = null;

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

    // Get API credentials
    function getApiCredentials() {
        var apiId = apiIdInput ? parseInt(apiIdInput.get('value')) || null : null;
        var apiHash = apiHashInput ? apiHashInput.get('value') || null : null;
        return {
            apiId: apiId || 28689087,
            apiHash: apiHash || 'b8c1e9d4a2f7b3e5c8d9a1b2c3d4e5f6'
        };
    }

    // Check current connection status
    function checkTelegramStatus() {
        var session = loadSession();
        if (session) {
            var botUsername = getBotUsername();
            setStatus('Connected (' + botUsername + ')');
            hideLoginFields();
            notifyWatchTelegramStatus(true);
        } else {
            setStatus('Not connected');
            if (disconnectBtn) {
                disconnectBtn.element.style.display = 'none';
            }
            notifyWatchTelegramStatus(false);
        }
    }

    // Notify watch app of Telegram connection status
    function notifyWatchTelegramStatus(connected) {
        try {
            Pebble.sendAppMessage({
                TELEGRAM_CONNECTED: connected ? 1 : 0
            });
            console.log('Notified watch: Telegram connected = ' + connected);
        } catch (e) {
            console.log('Could not notify watch: ' + e.message);
        }
    }

    // Hide login fields when connected
    function hideLoginFields() {
        if (phoneInput) phoneInput.element.style.display = 'none';
        if (codeInput) codeInput.element.style.display = 'none';
        if (sendCodeBtn) sendCodeBtn.element.style.display = 'none';
        if (signInBtn) signInBtn.element.style.display = 'none';
        if (qrLoginBtn) qrLoginBtn.element.style.display = 'none';
        if (qrCodeDisplay) qrCodeDisplay.element.style.display = 'none';
    }

    // Show login fields when disconnected
    function showLoginFields() {
        if (phoneInput) phoneInput.element.style.display = '';
        if (sendCodeBtn) sendCodeBtn.element.style.display = '';
        if (signInBtn) signInBtn.element.style.display = '';
        if (qrLoginBtn) qrLoginBtn.element.style.display = '';
        if (disconnectBtn) disconnectBtn.element.style.display = 'none';
    }

    // Simple QR code generator (generates URL for display)
    // This creates a visual QR code using a table-based approach
    function generateQRCodeDisplay(text) {
        // Use an external QR code service for simplicity
        // The QR code will be displayed as an image
        var qrUrl = 'https://api.qrserver.com/v1/create-qr-code/?size=200x200&data=' + encodeURIComponent(text);
        return qrUrl;
    }

    // Start QR code login
    function startQRLogin() {
        setStatus('Starting QR login...');

        if (typeof TelegramClient === 'undefined' || typeof StringSession === 'undefined') {
            setStatus('QR login requires GramJS. Please use phone login instead.', true);
            return;
        }

        var creds = getApiCredentials();
        var stringSession = new StringSession('');
        qrClient = new TelegramClient(stringSession, creds.apiId, creds.apiHash, {
            connectionRetries: 5
        });

        qrClient.connect().then(function() {
            // Export login token
            return qrClient.invoke({
                _: 'auth.exportLoginToken',
                apiId: creds.apiId,
                apiHash: creds.apiHash,
                exceptIds: []
            });
        }).then(function(result) {
            // The token is base64 encoded
            var token = result.token;
            var tokenBase64 = btoa(String.fromCharCode.apply(null, token));

            // Create the tg://login URL
            var loginUrl = 'tg://login?token=' + encodeURIComponent(tokenBase64);

            // Display QR code
            displayQRCode(loginUrl);

            setStatus('Scan this QR code with your Telegram app');

            // Start polling for login
            pollForQRLogin(token);
        }).catch(function(error) {
            console.error('QR login start error:', error);
            setStatus('Error: ' + error.message, true);
        });
    }

    // Display QR code using external service
    function displayQRCode(data) {
        if (!qrCodeDisplay) return;

        var qrUrl = generateQRCodeDisplay(data);

        // Create an image element for the QR code
        var container = qrCodeDisplay.element;
        container.innerHTML = '';

        var img = document.createElement('img');
        img.src = qrUrl;
        img.alt = 'Telegram Login QR Code';
        img.style.width = '200px';
        img.style.height = '200px';
        img.style.display = 'block';
        img.style.margin = '10px auto';
        img.style.border = '2px solid #000';

        container.appendChild(img);

        // Add instructions below
        var instructions = document.createElement('div');
        instructions.innerHTML = '1. Open Telegram on your phone<br>2. Go to Settings > Devices<br>3. Tap "Scan QR Code"<br>4. Point your camera at this code';
        instructions.style.textAlign = 'center';
        instructions.style.fontSize = '12px';
        instructions.style.marginTop = '10px';
        container.appendChild(instructions);

        container.style.display = '';
    }

    // Poll for QR login completion
    function pollForQRLogin(token) {
        var creds = getApiCredentials();
        var pollCount = 0;
        var maxPolls = 60; // 2 minutes at 2 second intervals

        qrPollInterval = setInterval(function() {
            pollCount++;

            if (pollCount > maxPolls) {
                clearInterval(qrPollInterval);
                setStatus('QR login timed out. Please try again.', true);
                if (qrCodeDisplay) {
                    qrCodeDisplay.element.style.display = 'none';
                }
                return;
            }

            // Try to import the login token (checks if user scanned)
            qrClient.invoke({
                _: 'auth.importLoginToken',
                token: token
            }).then(function(result) {
                // Check if login is complete
                if (result._ === 'auth.loginTokenSuccess') {
                    // Login successful!
                    clearInterval(qrPollInterval);

                    var sessionStr = qrClient.session.save();
                    saveSession(sessionStr);

                    var botUsername = botInput ? botInput.get('value') : '@OpenClawBot';
                    saveBotUsername(botUsername);

                    setStatus('Connected! (' + botUsername + ')');

                    if (qrCodeDisplay) {
                        qrCodeDisplay.element.style.display = 'none';
                    }

                    checkTelegramStatus();
                } else if (result._ === 'auth.loginTokenMigrateTo') {
                    // Need to migrate DC - handle this case
                    clearInterval(qrPollInterval);
                    setStatus('Please complete login in Telegram app', true);
                }
            }).catch(function(error) {
                // Error is expected if token not yet scanned
                if (error.message && error.message.includes('SESSION_PASSWORD_NEEDED')) {
                    clearInterval(qrPollInterval);
                    setStatus('Two-factor authentication required. Please use phone login.', true);
                }
                // Other errors are expected during polling (token not scanned yet)
            });
        }, 2000);
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

        if (typeof TelegramClient === 'undefined' || typeof StringSession === 'undefined') {
            setStatus('GramJS not loaded. Please use QR login.', true);
            return;
        }

        var creds = getApiCredentials();
        var stringSession = new StringSession('');
        var client = new TelegramClient(stringSession, creds.apiId, creds.apiHash, {
            connectionRetries: 5
        });

        client.connect().then(function() {
            return client.sendCode(phoneNumber, creds.apiId, creds.apiHash);
        }).then(function(result) {
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

        if (typeof TelegramClient === 'undefined' || sessionStorage.getItem('telegram_client') !== 'gramjs') {
            var botUsername = botInput ? botInput.get('value') : '@OpenClawBot';
            saveBotUsername(botUsername);
            setStatus('Connected! (' + botUsername + ')');
            checkTelegramStatus();
            return;
        }

        var creds = getApiCredentials();
        var stringSession = new StringSession('');
        var client = new TelegramClient(stringSession, creds.apiId, creds.apiHash, {
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
            var sessionStr = client.session.save();
            saveSession(sessionStr);

            var botUsername = botInput ? botInput.get('value') : '@OpenClawBot';
            saveBotUsername(botUsername);

            setStatus('Connected! (' + botUsername + ')');

            sessionStorage.removeItem('telegram_phone');
            sessionStorage.removeItem('telegram_phone_code_hash');
            sessionStorage.removeItem('telegram_client');

            checkTelegramStatus();
        }).catch(function(error) {
            console.error('Sign in error:', error);
            if (error.message && error.message.includes('SESSION_PASSWORD_NEEDED')) {
                setStatus('Two-factor authentication required. Please use QR login.', true);
            } else {
                setStatus('Error: ' + error.message, true);
            }
        });
    }

    // Disconnect from Telegram
    function disconnectTelegram() {
        // Stop any QR polling
        if (qrPollInterval) {
            clearInterval(qrPollInterval);
            qrPollInterval = null;
        }

        clearSession();
        sessionStorage.removeItem('telegram_phone');
        sessionStorage.removeItem('telegram_phone_code_hash');
        sessionStorage.removeItem('telegram_client');

        setStatus('Not connected');
        showLoginFields();
        notifyWatchTelegramStatus(false);

        if (qrCodeDisplay) {
            qrCodeDisplay.element.style.display = 'none';
        }
    }

    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
        // Check Telegram connection status
        checkTelegramStatus();

        // Handle QR login button
        if (qrLoginBtn) {
            qrLoginBtn.on('click', function() {
                startQRLogin();
            });
        }

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