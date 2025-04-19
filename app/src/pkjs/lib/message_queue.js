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

var MAX_BYTES_IN_FLIGHT = 600;

function MessageQueue() {
    this.queue = [];
    this.log = null;
    this.messagesInFlight = 0;
    this.bytesInFlight = 0;
}

function countBytes(message) {
    var bytes = 0;
    for (var key in message) {
        if (message.hasOwnProperty(key)) {
            var value = message[key];
            if (typeof value === 'string') {
                bytes += value.length;
            } else if (typeof value === 'number') {
                bytes += 4; // 4 bytes for numbers
            } else if (typeof value == 'boolean') {
                bytes += 1; // 1 byte for boolean
            } else if (Array.isArray(value)) {
                bytes += value.length; // 1 byte per array element
            } else if (value instanceof Uint8Array) {
                bytes += value.length; // 1 byte per array element
            }
            bytes += 12; // space for some overhead for the key.
        }
    }
    return bytes;
}

MessageQueue.prototype.startLogging = function() {
    this.log = [];
};

MessageQueue.prototype.stopLogging = function() {
    this.log = null;
}

MessageQueue.prototype.getLog = function() {
    return this.log || [];
}

MessageQueue.prototype.enqueue = function(message) {
    if (this.log) {
        this.log.push(message);
    }
    this.queue.push(message);
    if (this.messagesInFlight < 10 && this.bytesInFlight < MAX_BYTES_IN_FLIGHT) {
        console.log('sending immediately, messages in flight: ' + this.messagesInFlight + ', bytes in flight: ' + this.bytesInFlight);
        this.dequeue();
    } else {
        console.log('enqueued, queue length: ' + this.queue.length + ', bytes: ' + this.bytesInFlight);
    }
}

MessageQueue.prototype.dequeue = function() {
    var m = this.queue.shift();
    var mSize = countBytes(m);
    console.log('sending message, remaining: ' + this.queue.length + ', bytes in flight: ' + this.bytesInFlight);
    this.messagesInFlight++;
    this.bytesInFlight += mSize;
    Pebble.sendAppMessage(m, (function() {
        this.messagesInFlight--;
        this.bytesInFlight -= mSize;
        console.log('sent successfully');
        if (this.queue.length > 0) {
            if (this.bytesInFlight > MAX_BYTES_IN_FLIGHT) {
                console.log('still too many bytes in flight (' + this.bytesInFlight + '), waiting');
            } else {
                console.log('next');
                this.dequeue();
            }
        } else {
            console.log('done');
        }
    }).bind(this), (function() {
        this.messagesInFlight--;
        this.bytesInFlight -= mSize;
        console.log('failed, message lost. carrying on shortly.');
        setTimeout(function() {
            this.dequeue();
        }.bind(this), 10);
    }).bind(this));
}

exports.Queue = new MessageQueue();
