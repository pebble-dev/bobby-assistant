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
 * Message handling for Telegram communication.
 * Sends messages to OpenClaw bot and handles responses.
 */

var client = require('./client');
var telegramSession = require('./session');

// Message handlers
var messageHandlers = [];

/**
 * Send a message to the OpenClaw bot.
 * @param {string} message - The message to send
 * @param {string} botUsername - Optional bot username override
 * @returns {Promise<object>} Result with message ID
 */
function sendMessage(message, botUsername) {
    return new Promise(function(resolve, reject) {
        botUsername = botUsername || telegramSession.getBotUsername();

        if (!client.isClientConnected()) {
            reject(new Error('Telegram client not connected'));
            return;
        }

        var tgClient = client.getClient();

        // Remove @ prefix if present for the API call
        var cleanUsername = botUsername.replace(/^@/, '');

        tgClient.sendMessage(cleanUsername, { message: message }).then(function(result) {
            console.log('Message sent to', botUsername);
            resolve({
                success: true,
                messageId: result.id
            });
        }).catch(function(err) {
            console.error('Failed to send message:', err);
            reject(new Error('Failed to send message: ' + err.message));
        });
    });
}

/**
 * Register a handler for incoming messages from OpenClaw bot.
 * @param {function} handler - Handler function(message)
 * @returns {function} Unsubscribe function
 */
function onMessage(handler) {
    messageHandlers.push(handler);

    return function() {
        var index = messageHandlers.indexOf(handler);
        if (index > -1) {
            messageHandlers.splice(index, 1);
        }
    };
}

/**
 * Start listening for messages from the OpenClaw bot.
 * @param {string} botUsername - Optional bot username to filter messages
 */
function startListening(botUsername) {
    botUsername = botUsername || telegramSession.getBotUsername();

    if (!client.isClientConnected()) {
        console.error('Cannot start listening - client not connected');
        return;
    }

    var tgClient = client.getClient();

    // Listen for new messages
    // Note: This uses GramJS event handlers
    if (typeof NewMessage !== 'undefined') {
        tgClient.addEventHandler(function(event) {
            try {
                var message = event.message;
                var senderId = message.peerId;

                // Check if message is from the bot
                if (message && message.message) {
                    // Get the sender's username
                    // For now, we'll check if it matches our bot
                    var fromBot = checkIfFromBot(senderId, botUsername);

                    if (fromBot) {
                        console.log('Received message from OpenClaw bot');
                        handleIncomingMessage(message.message);
                    }
                }
            } catch (err) {
                console.error('Error handling incoming message:', err);
            }
        }, new NewMessage({}));
    } else {
        console.log('GramJS NewMessage not available - using polling fallback');
        startPolling(botUsername);
    }
}

/**
 * Check if message is from the specified bot.
 * @param {object} senderId - Sender ID from message
 * @param {string} botUsername - Bot username to check
 * @returns {boolean}
 */
function checkIfFromBot(senderId, botUsername) {
    // This would need to resolve the sender ID to a username
    // For now, we'll track the bot by ID after first contact
    var cachedBotId = localStorage.getItem('openclaw_bot_id');
    if (cachedBotId) {
        return senderId && senderId.userId === cachedBotId;
    }
    // If we don't have the ID cached, accept messages and verify on first send
    return true;
}

/**
 * Start polling for messages (fallback when event handlers aren't available).
 * @param {string} botUsername - Bot username to poll
 */
function startPolling(botUsername) {
    console.log('Starting message polling for', botUsername);
    // In a full implementation, this would periodically check for new messages
    // For now, we'll rely on the main session loop to handle responses
}

/**
 * Handle an incoming message from the bot.
 * @param {string} message - The message content
 */
function handleIncomingMessage(message) {
    console.log('Processing incoming message:', message.substring(0, 100));

    // Notify all registered handlers
    for (var i = 0; i < messageHandlers.length; i++) {
        try {
            messageHandlers[i](message);
        } catch (err) {
            console.error('Error in message handler:', err);
        }
    }
}

/**
 * Send a message and wait for a response.
 * @param {string} message - The message to send
 * @param {number} timeout - Timeout in milliseconds (default 60000)
 * @param {string} botUsername - Optional bot username override
 * @returns {Promise<string>} The response message
 */
function sendAndWaitForResponse(message, timeout, botUsername) {
    timeout = timeout || 60000; // Default 60 second timeout
    botUsername = botUsername || telegramSession.getBotUsername();

    return new Promise(function(resolve, reject) {
        var timeoutId;
        var unsubscribe;

        // Set up timeout
        timeoutId = setTimeout(function() {
            if (unsubscribe) {
                unsubscribe();
            }
            reject(new Error('Timeout waiting for response'));
        }, timeout);

        // Set up response handler
        unsubscribe = onMessage(function(response) {
            clearTimeout(timeoutId);
            unsubscribe();
            resolve(response);
        });

        // Send the message
        sendMessage(message, botUsername).catch(function(err) {
            clearTimeout(timeoutId);
            unsubscribe();
            reject(err);
        });
    });
}

/**
 * Send a streaming request to OpenClaw bot.
 * This handles the back-and-forth for tool calls.
 * @param {string} prompt - The initial prompt
 * @param {function} onText - Callback for text chunks
 * @param {function} onToolCall - Callback for tool calls
 * @param {function} onComplete - Callback when complete
 * @param {string} botUsername - Optional bot username override
 */
function sendStreamingRequest(prompt, onText, onToolCall, onComplete, botUsername) {
    botUsername = botUsername || telegramSession.getBotUsername();

    // The OpenClaw bot should respond with streaming-style messages
    // We'll need to handle multiple messages for streaming

    var fullResponse = '';
    var messageCount = 0;

    var unsubscribe = onMessage(function(message) {
        messageCount++;

        // Parse the message format from OpenClaw
        // Expected format:
        // "c:text" for text content
        // "f:tool_call" for function/tool call
        // "d:" for done
        // "t:thread_id" for thread ID

        if (message.startsWith('c:')) {
            // Text content
            var text = message.substring(1);
            fullResponse += text;
            if (onText) {
                onText(text);
            }
        } else if (message.startsWith('f:')) {
            // Tool call
            var toolCallStr = message.substring(2);
            if (onToolCall) {
                try {
                    var toolCall = JSON.parse(toolCallStr);
                    onToolCall(toolCall);
                } catch (e) {
                    console.error('Failed to parse tool call:', toolCallStr);
                }
            }
        } else if (message.startsWith('d:')) {
            // Done
            unsubscribe();
            if (onComplete) {
                onComplete(fullResponse);
            }
        } else if (message.startsWith('t:')) {
            // Thread ID - could be used for conversation continuity
            // Store it if needed
        } else {
            // Plain text - treat as content
            fullResponse += message;
            if (onText) {
                onText(message);
            }
        }
    });

    // Send initial prompt
    sendMessage(prompt, botUsername).catch(function(err) {
        unsubscribe();
        if (onComplete) {
            onComplete(null, err);
        }
    });
}

exports.sendMessage = sendMessage;
exports.onMessage = onMessage;
exports.startListening = startListening;
exports.sendAndWaitForResponse = sendAndWaitForResponse;
exports.sendStreamingRequest = sendStreamingRequest;