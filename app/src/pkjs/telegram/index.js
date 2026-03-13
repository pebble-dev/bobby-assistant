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
 * Telegram module index.
 * Exports all Telegram-related functionality.
 */

var client = require('./client');
var auth = require('./auth');
var session = require('./session');
var messages = require('./messages');

module.exports = {
    // Client
    initClient: client.initClient,
    isClientConnected: client.isClientConnected,
    getCurrentUser: client.getCurrentUser,
    disconnect: client.disconnect,
    getClient: client.getClient,

    // Auth
    sendCode: auth.sendCode,
    signIn: auth.signIn,
    signInWithPassword: auth.signInWithPassword,
    checkConnection: auth.checkConnection,
    logout: auth.logout,
    getAuthState: auth.getAuthState,

    // Session
    saveSession: session.saveSession,
    loadSession: session.loadSession,
    clearSession: session.clearSession,
    hasSession: session.hasSession,
    saveBotUsername: session.saveBotUsername,
    getBotUsername: session.getBotUsername,
    getSessionInfo: session.getSessionInfo,

    // Messages
    sendMessage: messages.sendMessage,
    onMessage: messages.onMessage,
    startListening: messages.startListening,
    sendAndWaitForResponse: messages.sendAndWaitForResponse,
    sendStreamingRequest: messages.sendStreamingRequest
};