// Minimal entry point for tree-shaking Telegram bundle
// Only export what Clawd actually uses

export { TelegramClient } from 'telegram';
export { StringSession } from 'telegram/sessions';
export { NewMessage } from 'telegram/events';