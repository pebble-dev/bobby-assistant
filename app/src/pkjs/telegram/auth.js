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
 * Telegram authentication functions.
 * Handles phone number verification and session management.
 */

var client = require('./client');
var session = require('./session');

// Authentication state
var authState = {
    phoneNumber: null,
    phoneCodeHash: null,
    isWaitingForCode: false
};

/**
 * Start the phone number authentication flow.
 * @param {string} phoneNumber - Phone number in international format (e.g., +1234567890)
 * @returns {Promise<object>} Result with success status and next step
 */
function sendCode(phoneNumber) {
    return new Promise(function(resolve, reject) {
        authState.phoneNumber = phoneNumber;

        if (client.getClient()) {
            // Use GramJS to send code
            client.getClient().sendCode(phoneNumber, {
                apiId: getApiId(),
                apiHash: getApiHash()
            }).then(function(result) {
                authState.phoneCodeHash = result.phoneCodeHash;
                authState.isWaitingForCode = true;
                resolve({
                    success: true,
                    status: 'code_sent',
                    message: 'Verification code sent to ' + phoneNumber
                });
            }).catch(function(err) {
                reject(new Error('Failed to send code: ' + err.message));
            });
        } else {
            // Fallback for when GramJS isn't loaded
            // In a real implementation, this would use a bundled MTProto library
            reject(new Error('Telegram client not initialized. Please ensure GramJS is loaded.'));
        }
    });
}

/**
 * Sign in with the verification code.
 * @param {string} code - The verification code received via SMS/Telegram
 * @returns {Promise<object>} Result with success status
 */
function signIn(code) {
    return new Promise(function(resolve, reject) {
        if (!authState.isWaitingForCode) {
            reject(new Error('No pending authentication. Call sendCode first.'));
            return;
        }

        if (client.getClient()) {
            client.getClient().signIn({
                code: code,
                phoneNumber: authState.phoneNumber,
                phoneCodeHash: authState.phoneCodeHash
            }).then(function(user) {
                // Save the session
                var sessionStr = client.getClient().session.save();
                session.saveSession(sessionStr);

                authState.isWaitingForCode = false;
                authState.phoneCodeHash = null;

                resolve({
                    success: true,
                    status: 'signed_in',
                    user: {
                        id: user.id,
                        firstName: user.firstName,
                        lastName: user.lastName,
                        username: user.username
                    }
                });
            }).catch(function(err) {
                // Handle 2FA case
                if (err.message && err.message.includes('SESSION_PASSWORD_NEEDED')) {
                    resolve({
                        success: false,
                        status: '2fa_required',
                        message: 'Two-factor authentication is enabled. Please provide your password.'
                    });
                } else {
                    reject(new Error('Failed to sign in: ' + err.message));
                }
            });
        } else {
            reject(new Error('Telegram client not initialized.'));
        }
    });
}

/**
 * Complete 2FA authentication.
 * @param {string} password - The 2FA password
 * @returns {Promise<object>} Result with success status
 */
function signInWithPassword(password) {
    return new Promise(function(resolve, reject) {
        if (!client.getClient()) {
            reject(new Error('Telegram client not initialized.'));
            return;
        }

        client.getClient().signInPassword(password).then(function(user) {
            var sessionStr = client.getClient().session.save();
            session.saveSession(sessionStr);

            resolve({
                success: true,
                status: 'signed_in',
                user: {
                    id: user.id,
                    firstName: user.firstName,
                    lastName: user.lastName,
                    username: user.username
                }
            });
        }).catch(function(err) {
            reject(new Error('Failed to sign in: ' + err.message));
        });
    });
}

/**
 * Check if there's an existing session.
 * @returns {Promise<object>} Connection status
 */
function checkConnection() {
    return new Promise(function(resolve) {
        var storedSession = session.loadSession();
        if (storedSession) {
            client.initClient().then(function() {
                resolve({
                    connected: true,
                    hasSession: true
                });
            }).catch(function() {
                resolve({
                    connected: false,
                    hasSession: true,
                    needsReauth: true
                });
            });
        } else {
            resolve({
                connected: false,
                hasSession: false
            });
        }
    });
}

/**
 * Disconnect and clear session.
 * @returns {Promise<void>}
 */
function logout() {
    return new Promise(function(resolve, reject) {
        client.disconnect().then(function() {
            session.clearSession();
            authState = {
                phoneNumber: null,
                phoneCodeHash: null,
                isWaitingForCode: false
            };
            resolve();
        }).catch(reject);
    });
}

/**
 * Get API ID from settings or default.
 * @returns {number}
 */
function getApiId() {
    var settings = JSON.parse(localStorage.getItem('clay-settings')) || {};
    return settings.TELEGRAM_API_ID || 28689087;
}

/**
 * Get API Hash from settings or default.
 * @returns {string}
 */
function getApiHash() {
    var settings = JSON.parse(localStorage.getItem('clay-settings')) || {};
    return settings.TELEGRAM_API_HASH || 'b8c1e9d4a2f7b3e5c8d9a1b2c3d4e5f6';
}

/**
 * Get authentication state.
 * @returns {object}
 */
function getAuthState() {
    return {
        phoneNumber: authState.phoneNumber,
        isWaitingForCode: authState.isWaitingForCode
    };
}

exports.sendCode = sendCode;
exports.signIn = signIn;
exports.signInWithPassword = signInWithPassword;
exports.checkConnection = checkConnection;
exports.logout = logout;
exports.getAuthState = getAuthState;