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


exports.QUERY_URL = 'wss://bobby-api.rebble.io/query';
exports.QUOTA_URL = 'https://bobby-api.rebble.io/quota';
exports.FEEDBACK_URL = 'https://bobby-api.rebble.io/feedback';
exports.REPORT_URL = 'https://bobby-api.rebble.io/report';
exports.TELEGRAM_AUTH_START_URL = 'https://bobby-api.rebble.io/telegram/auth/start';
exports.TELEGRAM_AUTH_STATUS_URL = 'https://bobby-api.rebble.io/telegram/auth/status';
exports.TELEGRAM_AUTH_CHECK_URL = 'https://bobby-api.rebble.io/telegram/auth/check';
exports.TELEGRAM_AUTH_LOGOUT_URL = 'https://bobby-api.rebble.io/telegram/auth/logout';
exports.TELEGRAM_SET_BOT_URL = 'https://bobby-api.rebble.io/telegram/auth/bot';

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
