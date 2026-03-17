var messageQueue = require('../lib/message_queue').Queue;
var realSession = require('../session');
var telegram = require('../telegram');

function Session(prompt, threadId) {
    console.log('Emulator session created');
    this.prompt = prompt;
    this.threadId = threadId;
}

Session.prototype.run = function() {
    console.log('Emulator session running with prompt:', this.prompt);

    // Check if Telegram is configured
    if (!telegram.hasSession()) {
        console.log('No Telegram session found - showing error');
        messageQueue.enqueue({
            CHAT: 'Not connected to Telegram. Please configure your Telegram connection in the app settings.'
        });
        messageQueue.enqueue({
            CHAT_DONE: true
        });
        return;
    }

    // Use real Telegram session
    console.log('Telegram session found - starting real session');
    var session = new realSession.Session(this.prompt, this.threadId);
    session.run();
};

exports.Session = Session;