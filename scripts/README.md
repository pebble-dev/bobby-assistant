# Telegram Bundle Build Scripts

This directory contains scripts for building the Telegram/GramJS bundle that's used by Clawd.

## Files

- `package.json` - Dependencies needed to build the Telegram bundle
- `build-gramjs.js` - esbuild script to bundle Telegram with browser polyfills

## Rebuilding the Bundle

If you need to update the Telegram bundle (e.g., to upgrade GramJS):

```bash
cd scripts
npm install
npm run build:telegram
```

This will regenerate `src/pkjs/lib/telegram-bundle.js`.

## How It Works

The build script uses esbuild to bundle GramJS (the `telegram` npm package) with all necessary browser polyfills. The bundle creates global variables:

- `Telegram` - The main GramJS object
- `TelegramClient` - The client class
- `StringSession` - Session management
- `NewMessage` - Event handler for messages

These globals are used by `src/pkjs/session.js` and `src/pkjs/custom_config.js` to communicate with Telegram.

## Dependencies

All dependencies in `package.json` are only needed for building the bundle. They are not required for running the Pebble app itself.