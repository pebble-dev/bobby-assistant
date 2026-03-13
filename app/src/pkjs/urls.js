/**
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * URLs module for Clawd.
 * All communication now goes directly through Telegram to OpenClaw.
 * No backend URLs are needed - the phone app communicates directly.
 */

// No backend URLs needed - direct Telegram communication
// These are kept for backwards compatibility but are not used
exports.QUERY_URL = '';
exports.QUOTA_URL = '';
exports.FEEDBACK_URL = '';
exports.REPORT_URL = '';

// Telegram auth URLs - no longer needed, auth is handled client-side
exports.TELEGRAM_AUTH_START_URL = '';
exports.TELEGRAM_AUTH_STATUS_URL = '';
exports.TELEGRAM_AUTH_CHECK_URL = '';
exports.TELEGRAM_AUTH_LOGOUT_URL = '';
exports.TELEGRAM_SET_BOT_URL = '';

// URL overrides (for development/testing)
var override = require('./urls_override');

if (override.QUERY_URL) {
    exports.QUERY_URL = override.QUERY_URL;
}
if (override.QUOTA_URL) {
    exports.QUOTA_URL = override.QUOTA_URL;
}
if (override.FEEDBACK_URL) {
    exports.FEEDBACK_URL = override.FEEDBACK_URL;
}
if (override.REPORT_URL) {
    exports.REPORT_URL = override.REPORT_URL;
}