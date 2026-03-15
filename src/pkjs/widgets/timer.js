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

exports.timer = function(session, params) {
    console.log(JSON.stringify(params));
    var time = new Date(params['target_time'])
    var name = params['name'] || null;
    var message = {
        TIMER_WIDGET: 1,
        TIMER_WIDGET_TARGET_TIME: Math.round(time.getTime() / 1000),
    };
    if (name && name.length > 0) {
        message['TIMER_WIDGET_NAME'] = name;
    }
    console.log(JSON.stringify(message));
    session.enqueue(message);
}
