#!/usr/bin/env node
/**
 * Prune unused Telegram TL schema definitions from the bundle.
 *
 * The TL schema contains 500+ API types, but we only need a subset.
 */

const fs = require('fs');
const path = require('path');

const bundlePath = process.argv[2] || path.join(__dirname, '../src/pkjs/lib/telegram-bundle.js');

// Required TL definitions (by name pattern)
// These cover: core types, auth, messaging, users/chats, updates
const REQUIRED = new Set([
    // Core primitives
    'boolFalse', 'boolTrue', 'true', 'vector', 'error', 'null',

    // Auth
    'auth.sentCode', 'auth.authorization', 'auth.sentCodeType', 'auth.codeType',
    'auth.loginToken', 'auth.loginTokenMigrateTo', 'auth.loginTokenSuccess',
    'auth.exportedSession', 'auth.exportSession',

    // Account
    'account.password', 'account.passwordSettings', 'account.passwordInputSettings',

    // Users
    'user', 'userFull', 'userProfilePhoto', 'userStatus', 'userEmpty',
    'inputUser', 'inputUserSelf', 'inputUserEmpty', 'inputUserFromMessage',
    'users.userFull', 'users.users',

    // Chats/Channels
    'chat', 'chatFull', 'chatEmpty', 'channel', 'channelFull', 'channelEmpty',
    'inputPeerChat', 'inputPeerChannel', 'inputPeerEmpty', 'inputPeerSelf',
    'inputPeerUser', 'inputPeerUserFromMessage', 'inputPeerChannelFromMessage',
    'chats.chats', 'channels.channels',

    // Messages
    'message', 'messageEmpty', 'messageService',
    'messageMediaEmpty', 'messageMediaPhoto', 'messageMediaDocument',
    'messages.dialogs', 'messages.dialogsSlice',
    'messages.messages', 'messages.messagesSlice', 'messages.channelMessages',
    'messages.sentMessage', 'messages.affectedMessages',
    'inputMessagesEmpty', 'inputMessagesFilterEmpty',

    // Updates
    'updates.state', 'updates.difference', 'updates.differenceEmpty',
    'updates.newMessage', 'updateNewMessage', 'updateReadHistory',
    'updateEditMessage', 'updateMessageID',

    // Documents/Media
    'document', 'photo', 'photoSize', 'inputDocument', 'inputPhoto',
    'documentAttributeFilename', 'documentAttributeVideo',
    'documentAttributeImageSize',

    // Common types
    'peerUser', 'peerChat', 'peerChannel', 'peerEmpty',

    // Request methods (functions) - use dot notation
    'auth.sendCode', 'auth.signIn', 'auth.signUp', 'auth.logOut',
    'auth.importAuthorization', 'auth.exportAuthorization',
    'messages.sendMessage', 'messages.getDialogs', 'messages.getHistory',
    'messages.readHistory', 'messages.getMessages', 'messages.sendMedia',
    'users.getUsers', 'users.getFullUser',
    'chats.getChats', 'channels.getChannels',
    'updates.getState', 'updates.getDifference',
    'account.getPassword', 'account.updatePasswordSettings',
    'help.getConfig',
]);

// Also add name without dot (e.g., 'user' for 'user#...')
for (const name of [...REQUIRED]) {
    if (name.includes('.')) {
        // Add the part after the dot as a constructor pattern
        const parts = name.split('.');
        // auth.sendCode -> also match sendCode#
        REQUIRED.add(parts[1]);
    }
}

function shouldKeep(line) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith('//')) return true;

    // Parse TL definition: name#id fields = Result;
    const match = trimmed.match(/^([a-zA-Z0-9_.]+)#([a-f0-9]+)/);
    if (!match) return true; // Keep non-definition lines

    const name = match[1];

    // Check if this definition is needed
    if (REQUIRED.has(name)) return true;

    // Check namespace (e.g., auth.sendCode)
    const parts = name.split('.');
    if (parts.length > 1) {
        if (REQUIRED.has(name)) return true;
        // Check if just the method name is needed
        if (REQUIRED.has(parts[1])) return true;
    }

    // Check if any required type matches
    for (const req of REQUIRED) {
        if (name.startsWith(req) || trimmed.includes('=' + req + ';')) return true;
    }

    return false;
}

function pruneSchema(schema) {
    const lines = schema.split('\\n');
    let kept = 0, removed = 0;
    const result = [];

    for (const line of lines) {
        if (shouldKeep(line)) {
            result.push(line);
            kept++;
        } else {
            removed++;
        }
    }

    console.log('TL schema: kept', kept, 'definitions, removed', removed);
    return result.join('\\n');
}

function main() {
    console.log('Reading bundle...');
    let code = fs.readFileSync(bundlePath, 'utf8');
    console.log('Original size:', (code.length / 1024).toFixed(1), 'KB');

    // Find the TL schema module
    // Pattern: module.exports="boolFalse#...
    const startPattern = 'module.exports="boolFalse#';
    const startIdx = code.indexOf(startPattern);
    if (startIdx === -1) {
        console.log('Could not find TL schema start');
        process.exit(1);
    }

    console.log('Found TL schema at offset', startIdx);

    // Find end: the schema ends with ";
    // We need to find where the module.exports string ends
    // Look for the pattern: ...= SmsJob;\n...;\n" followed by });

    // Simple approach: find the closing "); after the module wrapper
    // The apiTl module ends with: "}); // node_modules/telegram/tl/schemaTl.js or similar

    // Let's search for the end of the module.exports string
    // It ends with: fragment.getCollectibleInfo...= fragment.CollectibleInfo;\n"

    // Count escaped newlines to find schema end
    let depth = 0;
    let endIdx = startIdx + startPattern.length;
    let inString = true;

    // Actually, simpler: find where module.exports ends by looking for
    // the pattern: "; followed by });
    // The schema is one big string, ending with \n" followed by });

    // Find: fragment.CollectibleInfo;\n" followed by closing
    const endPattern = /fragment\.getCollectibleInfo[^\\]*\\n"/;
    const endMatch = code.slice(startIdx).match(endPattern);
    if (endMatch) {
        endIdx = startIdx + endMatch.index + endMatch[0].length - 1; // -1 to exclude the closing "
        console.log('Found schema end at offset', endIdx - startIdx);
    } else {
        console.log('Could not find schema end, trying alternative...');
        // Alternative: find the end of the module.exports assignment
        const altPattern = /module\.exports="[^"]*";/;
        // This won't work because the string contains escaped quotes...
        // Let's just skip pruning for now
        console.log('Skipping pruning - could not locate schema boundaries');
        process.exit(0);
    }

    // Extract schema
    const schemaStart = startIdx + 'module.exports="'.length;
    const schemaEnd = endIdx;
    const schema = code.slice(schemaStart, schemaEnd);

    console.log('Schema size:', (schema.length / 1024).toFixed(1), 'KB');

    // Prune
    const pruned = pruneSchema(schema);
    console.log('Pruned schema size:', (pruned.length / 1024).toFixed(1), 'KB');

    // Replace in bundle
    const newCode = code.slice(0, schemaStart) + pruned + code.slice(schemaEnd);
    console.log('New bundle size:', (newCode.length / 1024).toFixed(1), 'KB');
    console.log('Reduction:', ((1 - newCode.length / code.length) * 100).toFixed(1), '%');

    fs.writeFileSync(bundlePath, newCode);
    console.log('Done!');
}

main();