const esbuild = require('esbuild');
const { NodeGlobalsPolyfillPlugin } = require('@esbuild-plugins/node-globals-polyfill');
const { NodeModulesPolyfillPlugin } = require('@esbuild-plugins/node-modules-polyfill');
const { execSync } = require('child_process');
const path = require('path');
const fs = require('fs');

const rootDir = path.resolve(__dirname, '..');
const bundlePath = path.join(rootDir, 'src/pkjs/lib/telegram-bundle.js');
const tempPath = path.join(rootDir, 'src/pkjs/lib/telegram-bundle.temp.js');

esbuild.build({
    entryPoints: [path.join(__dirname, 'node_modules/telegram/index.js')],
    bundle: true,
    format: 'iife',
    globalName: 'Telegram',
    outfile: tempPath,
    define: {
        'process.env.NODE_ENV': '"production"',
        'global': 'globalThis',
    },
    platform: 'browser',
    target: ['es2020'],
    plugins: [
        NodeModulesPolyfillPlugin(),
        NodeGlobalsPolyfillPlugin({
            process: true,
            buffer: true,
        }),
    ],
}).then(() => {
    console.log('esbuild bundle created, transpiling to ES5 with Babel...');

    // Transpile with Babel to ES5 (using babel.config.json)
    execSync(`npx babel ${tempPath} --out-file ${bundlePath} --config-file ./babel.config.json`, {
        cwd: __dirname,
        stdio: 'inherit'
    });

    // Prepend regenerator-runtime polyfill
    const regeneratorRuntime = fs.readFileSync(
        path.join(__dirname, 'node_modules/regenerator-runtime/runtime.js'),
        'utf8'
    );

    const banner = `// Telegram/GramJS bundle for Clawd
// Run 'npm install && npm run build:telegram' in scripts/ directory to rebuild
// Polyfills for browser environment
if (typeof globalThis === 'undefined') {
    if (typeof global !== 'undefined') globalThis = global;
    else if (typeof window !== 'undefined') globalThis = window;
    else if (typeof self !== 'undefined') globalThis = self;
}

// Regenerator runtime polyfill for async/await
${regeneratorRuntime}

`;

    // Read the current content and prepend the banner
    const bundleContent = fs.readFileSync(bundlePath, 'utf8');
    fs.writeFileSync(bundlePath, banner + bundleContent);

    // Add global exports at the end
    const exports = `
// Expose TelegramClient and StringSession as globals for Clawd
if (typeof Telegram !== 'undefined') {
    if (typeof TelegramClient === 'undefined' && Telegram.TelegramClient) {
        var TelegramClient = Telegram.TelegramClient;
    }
    if (typeof StringSession === 'undefined' && Telegram.sessions && Telegram.sessions.StringSession) {
        var StringSession = Telegram.sessions.StringSession;
    }
    if (typeof NewMessage === 'undefined' && Telegram.events && Telegram.events.NewMessage) {
        var NewMessage = Telegram.events.NewMessage;
    }
}
`;
    fs.appendFileSync(bundlePath, exports);

    // Clean up temp file
    fs.unlinkSync(tempPath);

    console.log('Bundle created successfully!');
    console.log('Output:', bundlePath);
}).catch((error) => {
    console.error('Build failed:', error);
    process.exit(1);
});