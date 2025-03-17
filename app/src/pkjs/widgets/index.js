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

var weather = require('./weather');
var timer = require('./timer');
var highlights = require('./highlights');

var widgetMap = {
    'timer': timer.timer,
    'number': highlights.number,
    'weather-single-day': weather.singleDay,
    'weather-current': weather.current,
    'weather-multi-day': weather.multiDay
}

exports.handleWidget = function(session, widgetString) {
    var params = JSON.parse(widgetString);
    var name = params['type'];
    console.log("got a widget: ", widgetString);
    if (name in widgetMap) {
        widgetMap[name](session, params['content']);
    } else {
        console.log("Unknown widget '" + name + "'.");
    }
}