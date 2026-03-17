var messageQueue = require('../lib/message_queue').Queue;
var realSession = require('../session');

function Session(prompt, threadId) {
    console.log('session');
    this.prompt = prompt;
    this.threadId = threadId;
}

Session.prototype.run = function() {
    console.log(this.prompt);

    // Use real Telegram session (will show "not configured" error if not set up)
    var session = new realSession.Session(this.prompt, this.threadId);
    session.run();
};

exports.Session = Session;