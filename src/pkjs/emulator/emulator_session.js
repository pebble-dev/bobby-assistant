var messageQueue = require('../lib/message_queue').Queue;
var prerecorded = require('./prerecorded');

function Session(prompt, threadId) {
    console.log('session');
    this.prompt = prompt;
    this.threadId = threadId;
    this.randomResponseIndex = 0;
}

Session.prototype.run = function() {
    var response;
    console.log(this.prompt)
    if (this.prompt in prerecorded.specificResponses) {
        response = prerecorded.specificResponses[this.prompt];
    } else {
        response = prerecorded.randomResponses[this.randomResponseIndex];
        this.randomResponseIndex = (this.randomResponseIndex + 1) % prerecorded.randomResponses.length;
    }
    console.log("Sending response: " + response);
    for (var i = 0; i < response.length; i++) {
        messageQueue.enqueue(response[i]);
    }
}

exports.Session = Session;