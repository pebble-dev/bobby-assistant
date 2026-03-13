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

var LOGGING_ENABLED = false;

var location = require('./location');
var config = require('./config');
var actions = require('./actions');
var widgets = require('./widgets');
var messageQueue = require('./lib/message_queue').Queue;
var features = require('./features');
var tools = require('./tools');
var telegram = require('./telegram');

var package_json = require('package.json');

function Session(prompt, threadId) {
    this.prompt = prompt;
    this.threadId = threadId;
    this.hasOpenDialog = false;
    this.pendingToolCalls = [];
    this.messageBuffer = '';
}

function getSettings() {
    return JSON.parse(localStorage.getItem('clay-settings')) || {};
}

Session.prototype.run = function() {
    if (LOGGING_ENABLED) {
        messageQueue.startLogging();
    }
    console.log("Starting Telegram session...");

    var self = this;

    // Check if we have a Telegram session
    if (!telegram.hasSession()) {
        this.enqueue({
            CHAT: 'Not connected to Telegram. Please configure your Telegram connection in the app settings.'
        });
        this.enqueue({
            CHAT_DONE: true
        });
        return;
    }

    // Build the message with metadata
    var message = this.buildMessage();

    // Send to OpenClaw bot via Telegram
    this.sendToOpenClaw(message).then(function(response) {
        self.handleResponse(response);
    }).catch(function(error) {
        console.error('Telegram session error:', error);
        self.enqueue({
            CHAT: 'Error communicating with OpenClaw: ' + error.message
        });
        self.enqueue({
            CHAT_DONE: true
        });
    });
};

Session.prototype.buildMessage = function() {
    var settings = getSettings();
    var metadata = {
        tzOffset: -(new Date()).getTimezoneOffset(),
        actions: actions.getSupportedActions(),
        widgets: ['weather', 'timer', 'number'],
        units: settings['UNIT_PREFERENCE'] || '',
        lang: settings['LANGUAGE_CODE'] || '',
        version: package_json['version'],
        bot: telegram.getBotUsername()
    };

    // Add location if available
    if (location.isReady() && config.isLocationEnabled()) {
        var loc = location.getPos();
        metadata.lon = loc.lon;
        metadata.lat = loc.lat;
    } else {
        metadata.location = 'unknown';
    }

    // Add screen info
    if (typeof Pebble !== 'undefined' && Pebble.getActiveWatchInfo) {
        var platform = Pebble.getActiveWatchInfo().platform;
        var supportsColour, screenWidth, screenHeight;
        switch (platform) {
            case 'aplite':
                supportsColour = false;
                screenWidth = 144;
                screenHeight = 168;
                break;
            case 'basalt':
                supportsColour = true;
                screenWidth = 144;
                screenHeight = 168;
                break;
            case 'chalk':
                supportsColour = true;
                screenWidth = 180;
                screenHeight = 180;
                break;
            case 'diorite':
                supportsColour = false;
                screenWidth = 144;
                screenHeight = 168;
                break;
            case 'emery':
                supportsColour = true;
                screenWidth = 200;
                screenHeight = 228;
                break;
            default:
                supportsColour = false;
                screenWidth = 144;
                screenHeight = 168;
        }
        metadata.supportsColour = supportsColour;
        metadata.screenWidth = screenWidth;
        metadata.screenHeight = screenHeight;
    }

    // Format the message
    // OpenClaw expects metadata in a specific format
    var formattedMessage = this.prompt;
    if (this.threadId) {
        formattedMessage = '[thread:' + this.threadId + '] ' + formattedMessage;
    }

    // Append metadata as JSON
    formattedMessage += '\n\n---METADATA---\n' + JSON.stringify(metadata);

    return formattedMessage;
};

Session.prototype.sendToOpenClaw = function(message) {
    var self = this;
    var botUsername = telegram.getBotUsername();

    return new Promise(function(resolve, reject) {
        // Check if GramJS is available
        if (typeof TelegramClient !== 'undefined' && typeof StringSession !== 'undefined') {
            self.sendViaGramJS(message, botUsername, resolve, reject);
        } else {
            // Fallback: Use a simpler HTTP-based approach or show error
            reject(new Error('Telegram client not available. Please reconnect.'));
        }
    });
};

Session.prototype.sendViaGramJS = function(message, botUsername, resolve, reject) {
    var self = this;
    var sessionStr = telegram.loadSession();
    var settings = getSettings();
    var apiId = settings.TELEGRAM_API_ID ? parseInt(settings.TELEGRAM_API_ID) : 28689087;
    var apiHash = settings.TELEGRAM_API_HASH || 'b8c1e9d4a2f7b3e5c8d9a1b2c3d4e5f6';

    var stringSession = new StringSession(sessionStr);
    var client = new TelegramClient(stringSession, apiId, apiHash, {
        connectionRetries: 5
    });

    client.connect().then(function() {
        // Remove @ prefix for API call
        var cleanUsername = botUsername.replace(/^@/, '');

        // Send message
        return client.sendMessage(cleanUsername, { message: message });
    }).then(function(result) {
        console.log('Message sent to', botUsername);

        // Start listening for response
        self.listenForResponse(client, botUsername, resolve, reject);
    }).catch(function(error) {
        console.error('GramJS error:', error);
        reject(error);
    });
};

Session.prototype.listenForResponse = function(client, botUsername, resolve, reject) {
    var self = this;
    var timeout = 120000; // 2 minute timeout
    var startTime = Date.now();
    var responseBuffer = '';
    var expectedLength = null;
    var receivedComplete = false;

    // Clean username for comparison
    var cleanBotUsername = botUsername.replace(/^@/, '').toLowerCase();

    // Set up timeout
    var timeoutId = setTimeout(function() {
        reject(new Error('Timeout waiting for response from OpenClaw'));
    }, timeout);

    // Listen for new messages
    if (typeof NewMessage !== 'undefined') {
        client.addEventHandler(function(event) {
            try {
                var msg = event.message;
                if (msg && msg.message) {
                    // Check if from the bot
                    // This is a simplified check - in practice you'd resolve the bot's user ID
                    self.handleIncomingMessage(msg.message, resolve);
                }
            } catch (err) {
                console.error('Error handling message:', err);
            }
        }, new NewMessage({}));

        // Also poll for messages as fallback
        self.pollForMessages(client, botUsername, startTime, timeoutId, resolve, reject);
    } else {
        // Just poll
        self.pollForMessages(client, botUsername, startTime, timeoutId, resolve, reject);
    }
};

Session.prototype.pollForMessages = function(client, botUsername, startTime, timeoutId, resolve, reject) {
    var self = this;
    var pollInterval = 2000; // Poll every 2 seconds

    function poll() {
        if (Date.now() - startTime > 120000) {
            clearTimeout(timeoutId);
            return;
        }

        // Use getMessages to fetch new messages
        // This is a simplified approach
        try {
            client.getMessages(botUsername.replace(/^@/, ''), { limit: 1 }).then(function(messages) {
                if (messages && messages.length > 0) {
                    var latestMsg = messages[0];
                    // Check if this is a new message (after our sent message)
                    if (latestMsg && latestMsg.date * 1000 > startTime) {
                        self.handleIncomingMessage(latestMsg.message, resolve);
                    }
                }
            }).catch(function(err) {
                console.log('Poll error (non-fatal):', err);
            });
        } catch (e) {
            console.log('Poll error:', e);
        }

        setTimeout(poll, pollInterval);
    }

    poll();
};

Session.prototype.handleIncomingMessage = function(message, resolve) {
    console.log('Received message:', message.substring(0, 100));

    // Parse the OpenClaw message format
    // Expected format:
    // "c:text" - content
    // "f:tool_call" - function call
    // "d:" - done
    // "t:thread_id" - thread ID
    // "w:warning" - warning

    if (message.startsWith('c:')) {
        var content = message.substring(1);
        var widgetRegex = /<<!!WIDGET:(.+?)!!>>/;
        var match;

        while (content.length > 0) {
            match = widgetRegex.exec(content);
            if (!match) {
                break;
            }

            var widget = match[1];
            console.log("Widget found: " + widget);
            var start = match.index;
            if (start !== 0) {
                this.enqueue({
                    CHAT: content.substring(0, start)
                });
            }
            this.processWidget(widget);
            this.hasOpenDialog = false;
            content = content.substring(match.index + match[0].length);
        }

        if (content.length > 0) {
            this.hasOpenDialog = true;
            this.enqueue({
                CHAT: content
            });
        }
    } else if (message.startsWith('f:')) {
        if (this.hasOpenDialog) {
            console.log('Received a tool call while a dialog is open. Closing the dialog.');
            this.enqueue({
                CHAT_DONE: true
            });
            this.hasOpenDialog = false;
        }
        var toolCallStr = message.substring(1);
        this.handleToolCall(toolCallStr);
    } else if (message.startsWith('d:')) {
        this.hasOpenDialog = false;
        this.enqueue({
            CHAT_DONE: true
        });
        if (LOGGING_ENABLED) {
            console.log(JSON.stringify(messageQueue.getLog()));
            messageQueue.stopLogging();
        }
        resolve({ complete: true });
    } else if (message.startsWith('t:')) {
        this.enqueue({
            THREAD_ID: message.substring(1)
        });
    } else if (message.startsWith('w:')) {
        this.enqueue({
            WARNING: message.substring(1)
        });
    } else if (message.startsWith('a:')) {
        // Action from the bot
        actions.handleAction(this, { send: function() {} }, message.substring(1));
    } else {
        // Plain text - treat as content
        this.hasOpenDialog = true;
        this.enqueue({
            CHAT: message
        });
    }
};

Session.prototype.handleToolCall = function(toolCallStr) {
    var self = this;

    try {
        var toolCall = JSON.parse(toolCallStr);
        var toolName = toolCall.name || toolCall.function?.name;
        var toolArgs = toolCall.arguments || toolCall.function?.arguments || {};
        var toolCallId = toolCall.id || 'call_' + Date.now();

        // Parse arguments if string
        if (typeof toolArgs === 'string') {
            try {
                toolArgs = JSON.parse(toolArgs);
            } catch (e) {
                console.error('Failed to parse tool arguments:', toolArgs);
            }
        }

        console.log('Tool call:', toolName, JSON.stringify(toolArgs));

        // Get thought text for UI
        var thought = tools.getToolThought(toolName, toolArgs);
        this.enqueue({
            FUNCTION: thought
        });

        // Execute the tool
        tools.executeTool(toolName, toolArgs, this).then(function(result) {
            console.log('Tool result:', JSON.stringify(result));

            // Send result back to OpenClaw
            var resultMessage = 'r:' + JSON.stringify({
                tool_call_id: toolCallId,
                name: toolName,
                result: result
            });

            self.sendToOpenClaw(resultMessage).then(function() {
                console.log('Tool result sent');
            }).catch(function(err) {
                console.error('Failed to send tool result:', err);
                // Continue anyway - maybe the bot will handle it
            });
        }).catch(function(error) {
            console.error('Tool execution error:', error);

            // Send error back to OpenClaw
            var errorMessage = 'r:' + JSON.stringify({
                tool_call_id: toolCallId,
                name: toolName,
                result: { error: error.message || 'Tool execution failed' }
            });

            self.sendToOpenClaw(errorMessage);
        });
    } catch (e) {
        console.error('Failed to handle tool call:', e);
    }
};

Session.prototype.processWidget = function(widgetData) {
    widgets.handleWidget(this, widgetData);
};

Session.prototype.enqueue = function(message) {
    messageQueue.enqueue(message);
};

Session.prototype.dequeue = function() {
    messageQueue.dequeue();
};

// Keep the original WebSocket-based session as fallback
Session.prototype.runLegacy = function() {
    if (LOGGING_ENABLED) {
        messageQueue.startLogging();
    }
    console.log("Opening websocket connection...");

    var API_URL = require('./urls').QUERY_URL;
    var url = API_URL + '?prompt=' + encodeURIComponent(this.prompt) + '&token=' + exports.userToken;

    if (location.isReady() && config.isLocationEnabled()) {
        var loc = location.getPos();
        url += '&lon=' + loc.lon + '&lat=' + loc.lat;
    } else {
        url += '&location=unknown';
    }
    if (this.threadId) {
        url += '&threadId=' + encodeURIComponent(this.threadId);
    }
    url += '&tzOffset=' + (-(new Date()).getTimezoneOffset());
    url += '&actions=' + actions.getSupportedActions().join(',');
    url += '&widgets=weather,timer,number';
    if (features.FEATURE_MAP_WIDGET) {
        url += ',map';
    }
    var settings = getSettings();
    url += '&units=' + settings['UNIT_PREFERENCE'] || '';
    url += '&lang=' + settings['LANGUAGE_CODE'] || '';
    url += '&version=' + package_json['version'];

    console.log(url);
    this.ws = new WebSocket(url);
    this.ws.addEventListener('message', this.handleLegacyMessage.bind(this));
    this.ws.addEventListener('close', this.handleClose.bind(this));
};

Session.prototype.handleLegacyMessage = function(event) {
    var message = event.data;
    console.log(message);

    if (message[0] == 'c') {
        var widgetRegex = /<<!!WIDGET:(.+?)!!>>/;
        var content = message.substring(1);
        var match;

        while (content.length > 0) {
            match = widgetRegex.exec(content);
            if (!match) {
                break;
            }
            var widget = match[1];
            console.log("Widget found: " + widget);
            var start = match.index;
            if (start != 0) {
                this.enqueue({
                    CHAT: content.substring(0, start)
                });
            }
            this.processWidget(widget);
            this.hasOpenDialog = false;
            content = content.substring(match.index + match[0].length);
        }
        if (content.length > 0) {
            this.hasOpenDialog = true;
            this.enqueue({
                CHAT: content
            });
        }
    } else if (message[0] == 'f') {
        if (this.hasOpenDialog) {
            console.log('Received a thought while a dialog is open. Closing the dialog.');
            this.enqueue({
                CHAT_DONE: true
            });
            this.hasOpenDialog = false;
        }
        this.enqueue({
            FUNCTION: message.substring(1)
        });
    } else if (message[0] == 'd') {
        this.hasOpenDialog = false;
        this.enqueue({
            CHAT_DONE: true
        });
        if (LOGGING_ENABLED) {
            console.log(JSON.stringify(messageQueue.getLog()));
            messageQueue.stopLogging();
        }
    } else if (message[0] == 'a') {
        actions.handleAction(this, this.ws, message.substring(1));
    } else if (message[0] == 't') {
        this.enqueue({
            THREAD_ID: message.substring(1)
        });
    } else if (message[0] == 'w') {
        this.enqueue({
            WARNING: message.substring(1)
        });
    }
};

Session.prototype.handleClose = function(event) {
    console.log("Connection closed. Code: " + event.code + ". Reason: \"" + event.reason + "\". Was clean: " + event.wasClean);
    this.enqueue({
        CLOSE_CODE: event.code,
        CLOSE_REASON: event.reason,
        CLOSE_WAS_CLEAN: event.wasClean
    });
};

exports.Session = Session;
exports.userToken = null;