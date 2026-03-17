var messageQueue = require('../lib/message_queue').Queue;
var prerecorded = require('./prerecorded');
var realSession = require('../session');

function Session(prompt, threadId) {
    console.log('session');
    this.prompt = prompt;
    this.threadId = threadId;
    this.randomResponseIndex = 0;
}

Session.prototype.run = function() {
    var self = this;
    console.log(this.prompt);

    // Try to use real Telegram session if configured
    if (typeof TelegramClient !== 'undefined') {
        console.log("Using real Telegram session");
        var session = new realSession.Session(this.prompt, this.threadId);
        session.run();
        return;
    }

    // Fall back to prerecorded responses
    console.log("Telegram not configured, using prerecorded responses");
    var response;
    if (this.prompt in prerecorded.specificResponses) {
        response = prerecorded.specificResponses[this.prompt];
    } else {
        response = prerecorded.randomResponses[this.randomResponseIndex];
        this.randomResponseIndex = (this.randomResponseIndex + 1) % prerecorded.randomResponses.length;
    }
    console.log("Sending response: " + JSON.stringify(response));
    for (var i = 0; i < response.length; i++) {
        messageQueue.enqueue(response[i]);
    }
}

exports.Session = Session;