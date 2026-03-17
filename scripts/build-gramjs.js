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
    // Use minimal entry point for tree-shaking
    entryPoints: [path.join(__dirname, 'telegram-entry.js')],
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
    // Enable tree-shaking
    treeShaking: true,
    // Ignore source maps for smaller output
    sourcemap: false,
    // Minify identifiers for smaller output (but still readable)
    minifyIdentifiers: false,
    minifySyntax: true,
    // Drop debug info
    drop: ['debugger'],
    // Analyze bundle composition (uncomment for debugging)
    // metafile: true,
    plugins: [
        NodeModulesPolyfillPlugin(),
        NodeGlobalsPolyfillPlugin({
            process: true,
            buffer: true,
        }),
    ],
}).then(() => {
    console.log('esbuild bundle created, transpiling to ES5 with Babel...');

    // Transpile with Babel to ES5
    execSync(`npx babel ${tempPath} --out-file ${bundlePath} --config-file ./babel.config.json`, {
        cwd: __dirname,
        stdio: 'inherit'
    });

    // Prune unused TL schema definitions
    console.log('Pruning unused TL definitions...');
    const pruneScript = path.join(__dirname, 'prune-telegram-bundle.js');
    try {
        execSync(`node ${pruneScript} ${bundlePath}`, { stdio: 'inherit' });
    } catch (e) {
        console.log('Warning: Pruning failed, continuing with full bundle');
    }

    // Prepend banner
    const banner = `// Telegram/GramJS bundle for Clawd
// Run 'npm install && npm run build:telegram' in scripts/ directory to rebuild
// Polyfills for browser environment
if (typeof globalThis === 'undefined') {
    if (typeof global !== 'undefined') globalThis = global;
    else if (typeof window !== 'undefined') globalThis = window;
    else if (typeof self !== 'undefined') globalThis = self;
}

`;

    const bundleContent = fs.readFileSync(bundlePath, 'utf8');
    fs.writeFileSync(bundlePath, banner + bundleContent);

    // Append footer
    const footer = `
// Expose TelegramClient and StringSession as globals for Clawd
if (typeof Telegram !== 'undefined') {
    if (typeof TelegramClient === 'undefined' && Telegram.TelegramClient) {
        var TelegramClient = Telegram.TelegramClient;
    }
    if (typeof StringSession === 'undefined' && Telegram.StringSession) {
        var StringSession = Telegram.StringSession;
    }
    if (typeof NewMessage === 'undefined' && Telegram.NewMessage) {
        var NewMessage = Telegram.NewMessage;
    }
}
`;
    fs.appendFileSync(bundlePath, footer);

    // Clean up temp file
    fs.unlinkSync(tempPath);

    console.log('Bundle created successfully!');
    console.log('Output:', bundlePath);
}).catch((error) => {
    console.error('Build failed:', error);
    process.exit(1);
});