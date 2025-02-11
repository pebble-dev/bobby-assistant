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

var config = require('./config');
var actions = require('./actions');

var API_URL = require('./urls').QUERY_URL;

function Session(prompt, threadId) {
    this.prompt = prompt;
    this.threadId = threadId;
    this.ws = undefined;
    this.queue = [];
    this.hasOpenDialog = false;
}

function getSettings() {
    return JSON.parse(localStorage.getItem('clay-settings')) || {};
}

Session.prototype.run = function() {
    console.log("Opening websocket connection...");
    var url = API_URL + '?prompt=' + encodeURIComponent(this.prompt) + '&token=' + exports.userToken;
    if (this.threadId) {
        url += '&threadId=' + encodeURIComponent(this.threadId);
    }
    // negate this because JavaScript does it backwards for some reason.
    url += '&tzOffset=' + (-(new Date()).getTimezoneOffset());
    url += '&actions=' + actions.getSupportedActions().join(',');
    var settings = getSettings();
    url += '&units=' + settings['UNIT_PREFERENCE'] || '';
    url += '&lang=' + settings['LANGUAGE_CODE'] || '';
    console.log(url);
    this.ws = new WebSocket(url);
    this.ws.addEventListener('message', this.handleMessage.bind(this));
    this.ws.addEventListener('close', this.handleClose.bind(this));
}

Session.prototype.handleMessage = function(event) {
    var message = event.data;
    console.log(message);
    if (message[0] == 'c') {
        this.hasOpenDialog = true;
        Pebble.sendAppMessage({
            CHAT: message.substring(1)
        });
    } else if (message[0] == 'f') {
        if (this.hasOpenDialog) {
            console.log('Received a thought while a dialog is open. Closing the dialog.');
            Pebble.sendAppMessage({
                CHAT_DONE: true
            });
            this.hasOpenDialog = false;
        }
        Pebble.sendAppMessage({
            FUNCTION: message.substring(1)
        });
    } else if (message[0] == 'd') {
        this.hasOpenDialog = false;
        Pebble.sendAppMessage({
            CHAT_DONE: true
        });
    } else if (message[0] == 'a') {
        actions.handleAction(this.ws, message.substring(1));
    } else if (message[0] == 't') {
        Pebble.sendAppMessage({
            THREAD_ID: message.substring(1)
        });
    }
}

Session.prototype.enqueue = function(message) {
    this.queue.push(message);
    if (this.queue.length == 1) {
        console.log('sending immediately');
        this.dequeue();
    } else {
        console.log('enqueued');
    }
}

Session.prototype.dequeue = function() {
    var m = this.queue[0];
    console.log('sending message');
    Pebble.sendAppMessage(m, (function() {
        console.log('sent successfully');
        this.queue.shift();
        if (this.queue.length > 0) {
            console.log('next');
            this.dequeue();
        } else {
            console.log('done');
        }
    }).bind(this), (function() {
        console.log('failed. retrying soon...');
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
