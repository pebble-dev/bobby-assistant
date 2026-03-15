const esbuild = require('esbuild');
const { NodeGlobalsPolyfillPlugin } = require('@esbuild-plugins/node-globals-polyfill');
const { NodeModulesPolyfillPlugin } = require('@esbuild-plugins/node-modules-polyfill');
const path = require('path');

const rootDir = path.resolve(__dirname, '..');

esbuild.build({
    entryPoints: [path.join(rootDir, 'node_modules/telegram/index.js')],
    bundle: true,
    format: 'iife',
    globalName: 'Telegram',
    outfile: path.join(rootDir, 'src/pkjs/lib/telegram-bundle.js'),
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
    banner: {
        js: `
// Telegram/GramJS bundle for Clawd
// Run 'npm install && npm run build:telegram' in scripts/ directory to rebuild
// Polyfills for browser environment
if (typeof globalThis === 'undefined') {
    if (typeof global !== 'undefined') globalThis = global;
    else if (typeof window !== 'undefined') globalThis = window;
    else if (typeof self !== 'undefined') globalThis = self;
}
`,
    },
}).then(() => {
    console.log('Bundle created successfully!');
    console.log('Output:', path.join(rootDir, 'src/pkjs/lib/telegram-bundle.js'));
}).catch((error) => {
    console.error('Build failed:', error);
    process.exit(1);
});