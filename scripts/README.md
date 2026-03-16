# Telegram Bundle Build Scripts

This directory contains scripts for building the Telegram/GramJS bundle that's used by Clawd.

## Files

- `package.json` - Dependencies needed to build the Telegram bundle
- `build-gramjs.js` - Build script using esbuild + Babel
- `babel.config.json` - Babel configuration for ES5 transpilation

## Rebuilding the Bundle

If you need to update the Telegram bundle (e.g., to upgrade GramJS):

```bash
cd scripts
npm install
npm run build:telegram
```

This will regenerate `src/pkjs/lib/telegram-bundle.js`.

## How It Works

The build process has two steps:

1. **esbuild** - Bundles the `telegram` npm package with browser polyfills
2. **Babel** - Transpiles to ES5 for compatibility with older JavaScript runtimes

The bundle creates global variables:

- `Telegram` - The main GramJS object
- `TelegramClient` - The client class
- `StringSession` - Session management
- `NewMessage` - Event handler for messages

These globals are used by `src/pkjs/session.js` and `src/pkjs/custom_config.js` to communicate with Telegram.

## Why ES5?

The Pebble app's JavaScript runtime (PebbleKit JS) runs on mobile devices and may have limited ES6+ support. Transpiling to ES5 ensures maximum compatibility.

## Dependencies

All dependencies in `package.json` are only needed for building the bundle. They are not required for running the Pebble app itself.