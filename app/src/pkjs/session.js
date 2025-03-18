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

var location = require('./location');
var config = require('./config');
var actions = require('./actions');
var widgets = require('./widgets');

var API_URL = require('./urls').QUERY_URL;
var package_json = require('package.json');

function Session(prompt, threadId) {
    this.prompt = prompt;
    this.threadId = threadId;
    this.ws = undefined;
    this.queue = [];
    this.hasOpenDialog = false;
    this.messagesInFlight = 0;
}

function getSettings() {
    return JSON.parse(localStorage.getItem('clay-settings')) || {};
}

Session.prototype.run = function() {
    console.log("Opening websocket connection...");
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
    // negate this because JavaScript does it backwards for some reason.
    url += '&tzOffset=' + (-(new Date()).getTimezoneOffset());
    url += '&actions=' + actions.getSupportedActions().join(',');
    url += '&widgets=weather,timer,number';
    var settings = getSettings();
    url += '&units=' + settings['UNIT_PREFERENCE'] || '';
    url += '&lang=' + settings['LANGUAGE_CODE'] || '';
    url += '&version=' + package_json['version'];

    console.log(url);
    this.ws = new WebSocket(url);
    this.ws.addEventListener('message', this.handleMessage.bind(this));
    this.ws.addEventListener('close', this.handleClose.bind(this));
}

Session.prototype.handleMessage = function(event) {
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
}

Session.prototype.processWidget = function(widgetData) {
    widgets.handleWidget(this, widgetData);
}

Session.prototype.enqueue = function(message) {
    this.queue.push(message);
    if (this.messagesInFlight < 10) {
        console.log('sending immediately, messages in flight: ' + this.messagesInFlight);
        this.dequeue();
    } else {
        console.log('enqueued, queue length: ' + this.queue.length);
    }
}

Session.prototype.dequeue = function() {
    var m = this.queue.shift();
    console.log('sending message, remaining: ' + this.queue.length);
    this.messagesInFlight++;
    Pebble.sendAppMessage(m, (function() {
        this.messagesInFlight--;
        console.log('sent successfully');
        if (this.queue.length > 0) {
            console.log('next');
            this.dequeue();
        } else {
            console.log('done');
        }
    }).bind(this), (function() {
        this.messagesInFlight--;
        console.log('failed, message lost. carrying on shortly.');
        setTimeout(function() {
            this.dequeue();
            }, 10);
    }).bind(this));
}

Session.prototype.handleClose = function(event) {
    console.log("Connection closed. Code: " + event.code + ". Reason: \"" + event.reason + "\". Was clean: " + event.wasClean);
    this.enqueue({
        CLOSE_CODE: event.code,
        CLOSE_REASON: event.reason,
        CLOSE_WAS_CLEAN: event.wasClean
    });
}

exports.Session = Session;
exports.userToken = null;
