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

var messageKeys = require('message_keys');

var WEATHER_WIDGET_SINGLE_DAY = 1;
var WEATHER_WIDGET_CURRENT = 2;
var WEATHER_WIDGET_MULTI_DAY = 3;

var WEATHER_CONDITION_LIGHT_RAIN = 1;
var WEATHER_CONDITION_HEAVY_RAIN = 2;
var WEATHER_CONDITION_LIGHT_SNOW = 3;
var WEATHER_CONDITION_HEAVY_SNOW = 4;
var WEATHER_CONDITION_CLOUDY_DAY = 5;
var WEATHER_CONDITION_WEATHER_ICON = 6; // ???
var WEATHER_CONDITION_PARTLY_CLOUDY = 7;
var WEATHER_CONDITION_SUN = 8;



var INTEGERS_TO_CONDITIONS = {
    0:  "HEAVY_RAIN",
    1:  "HEAVY_RAIN",
    2:  "HEAVY_RAIN",
    3:  "HEAVY_RAIN",
    4:  "HEAVY_RAIN",
    5:  "LIGHT_SNOW",
    6:  "LIGHT_SNOW",
    7:  "LIGHT_SNOW",
    8:  "LIGHT_SNOW",
    9:  "LIGHT_RAIN",
    10: "LIGHT_SNOW",
    11: "LIGHT_RAIN",
    12: "HEAVY_RAIN",
    13: "LIGHT_SNOW",
    14: "LIGHT_SNOW",
    15: "HEAVY_SNOW",
    16: "HEAVY_SNOW",
    17: "HEAVY_SNOW",
    18: "LIGHT_SNOW",
    19: "CLOUDY_DAY",
    20: "CLOUDY_DAY",
    21: "CLOUDY_DAY",
    22: "CLOUDY_DAY",
    23: "WEATHER_ICON",
    24: "WEATHER_ICON",
    25: "WEATHER_ICON",
    26: "CLOUDY_DAY",
    27: "CLOUDY_DAY",
    28: "CLOUDY_DAY",
    29: "PARTLY_CLOUDY",
    30: "PARTLY_CLOUDY",
    31: "SUN",
    32: "SUN",
    33: "SUN",
    34: "SUN",
    35: "LIGHT_SNOW",
    36: "SUN",
    37: "HEAVY_RAIN",
    38: "HEAVY_RAIN",
    39: "HEAVY_RAIN",
    40: "HEAVY_RAIN",
    41: "HEAVY_SNOW",
    42: "HEAVY_SNOW",
    43: "HEAVY_SNOW",
    44: "WEATHER_ICON",
    45: "LIGHT_RAIN",
    46: "LIGHT_SNOW",
    47: "HEAVY_RAIN"
}

var CONDITION_MAP = {
    "LIGHT_RAIN": WEATHER_CONDITION_LIGHT_RAIN,
    "HEAVY_RAIN": WEATHER_CONDITION_HEAVY_RAIN,
    "LIGHT_SNOW": WEATHER_CONDITION_LIGHT_SNOW,
    "HEAVY_SNOW": WEATHER_CONDITION_HEAVY_SNOW,
    "CLOUDY_DAY": WEATHER_CONDITION_CLOUDY_DAY,
    "WEATHER_ICON": WEATHER_CONDITION_WEATHER_ICON,
    "PARTLY_CLOUDY": WEATHER_CONDITION_PARTLY_CLOUDY,
    "SUN": WEATHER_CONDITION_SUN
}

exports.singleDay = function(session, params) {
    var condInt = INTEGERS_TO_CONDITIONS[params['condition']];
    var condition = CONDITION_MAP[condInt];

    console.log("Sending widget data...");
    session.enqueue({
        "WEATHER_WIDGET": WEATHER_WIDGET_SINGLE_DAY,
        "WEATHER_WIDGET_DAY_HIGH": params['high'],
        "WEATHER_WIDGET_DAY_LOW": params['low'],
        "WEATHER_WIDGET_LOCATION": params['location'].toUpperCase(),
        "WEATHER_WIDGET_DAY_SUMMARY": params['summary'],
        "WEATHER_WIDGET_TEMP_UNIT": params['unit'],
        "WEATHER_WIDGET_DAY_ICON": condition,
        "WEATHER_WIDGET_DAY_OF_WEEK": params['day']
    });
}

exports.current = function(session, params) {
    var condInt = INTEGERS_TO_CONDITIONS[params['condition']];
    var condition = CONDITION_MAP[condInt];

    console.log("Sending widget data...");
    session.enqueue({
        "WEATHER_WIDGET": WEATHER_WIDGET_CURRENT,
        "WEATHER_WIDGET_CURRENT_TEMP": params['temperature'],
        "WEATHER_WIDGET_FEELS_LIKE": params['feels_like'],
        "WEATHER_WIDGET_LOCATION": params['location'].toUpperCase(),
        "WEATHER_WIDGET_DAY_SUMMARY": params['description'],
        "WEATHER_WIDGET_TEMP_UNIT": params['unit'],
        "WEATHER_WIDGET_WIND_SPEED": params['wind_speed'],
        "WEATHER_WIDGET_WIND_SPEED_UNIT": params['wind_speed_unit'],
        "WEATHER_WIDGET_DAY_ICON": condition
    });
}

exports.multiDay = function(session, params) {
    var message = {
        "WEATHER_WIDGET": WEATHER_WIDGET_MULTI_DAY,
        "WEATHER_WIDGET_LOCATION": params['location'].toUpperCase()
    }
    for (var i = 0; i < 3; ++i) {
        var day = params['days'][i];
        var condInt = INTEGERS_TO_CONDITIONS[day['condition']];
        var condition = CONDITION_MAP[condInt];
        message[messageKeys.WEATHER_WIDGET_MULTI_DAY + i] = day['day'].substring(0, 3).toUpperCase();
        message[messageKeys.WEATHER_WIDGET_MULTI_HIGH + i] = day['high'];
        message[messageKeys.WEATHER_WIDGET_MULTI_LOW + i] = day['low'];
        message[messageKeys.WEATHER_WIDGET_MULTI_ICON + i] = condition;
    }
    session.enqueue(message);
}