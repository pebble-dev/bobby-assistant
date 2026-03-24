/**
 * Mock Telegram/GramJS bundle for testing
 * Provides stub implementations of TelegramClient and StringSession
 */

// Mock StringSession
function StringSession(sessionString) {
    this._sessionString = sessionString || '';
}

StringSession.prototype.save = function() {
    return this._sessionString;
};

// Mock TelegramClient
function TelegramClient(session, apiId, apiHash, params) {
    this.session = session;
    this.apiId = apiId;
    this.apiHash = apiHash;
    this.params = params || {};
    this._connected = false;
}

TelegramClient.prototype.connect = function() {
    console.log('[MOCK] TelegramClient.connect()');
    this._connected = true;
    return Promise.resolve();
};

TelegramClient.prototype.disconnect = function() {
    console.log('[MOCK] TelegramClient.disconnect()');
    this._connected = false;
    return Promise.resolve();
};

TelegramClient.prototype.invoke = function(request) {
    console.log('[MOCK] TelegramClient.invoke()', request);
    return Promise.reject(new Error('Mock client - not implemented'));
};

TelegramClient.prototype.sendCode = function(phone, apiId, apiHash) {
    console.log('[MOCK] TelegramClient.sendCode()', phone);
    return Promise.resolve({ phoneCodeHash: 'mock-hash' });
};

TelegramClient.prototype.signIn = function(params) {
    console.log('[MOCK] TelegramClient.signIn()', params);
    return Promise.resolve({ user: { id: 0, username: 'mock-user' } });
};

TelegramClient.prototype.getSession = function() {
    return this.session;
};

// Expose as globals
if (typeof global !== 'undefined') {
    global.TelegramClient = TelegramClient;
    global.StringSession = StringSession;
} else if (typeof window !== 'undefined') {
    window.TelegramClient = TelegramClient;
    window.StringSession = StringSession;
}

// Also expose via module.exports for CommonJS
module.exports = {
    TelegramClient: TelegramClient,
    StringSession: StringSession
};

console.log('[MOCK] Telegram bundle loaded - using mock implementation');