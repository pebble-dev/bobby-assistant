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

var WEATHER_WIDGET_SINGLE_DAY = 1;
var WEATHER_WIDGET_CURRENT = 2;

var WEATHER_CONDITION_LIGHT_RAIN = 1;
var WEATHER_CONDITION_HEAVY_RAIN = 2;
var WEATHER_CONDITION_LIGHT_SNOW = 3;
var WEATHER_CONDITION_HEAVY_SNOW = 4;
var WEATHER_CONDITION_CLOUDY_DAY = 5;
var WEATHER_CONDITION_WEATHER_ICON = 6; // ???
var WEATHER_CONDITION_PARTLY_CLOUDY = 7;
var WEATHER_CONDITION_SUN = 8;

// in ye olde javascript, there's no way to make a map using variables as keys.
var COLOUR_MAP = {
    1: 0x5500FF,
    2: 0x5500FF,
    3: 0xFFFFFF,
    4: 0xFFFFFF,
    5: 0xAAAAAA,
    6: 0x5500FF,
    7: 0xAAAAAA,
    8: 0xFFFF00
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
    var condition = CONDITION_MAP[params['condition']];
    var colour = COLOUR_MAP[condition];

    console.log("Sending widget data...");
    session.enqueue({
        "WEATHER_WIDGET": WEATHER_WIDGET_SINGLE_DAY,
        "WEATHER_WIDGET_DAY_HIGH": params['high'],
        "WEATHER_WIDGET_DAY_LOW": params['low'],
        "WEATHER_WIDGET_LOCATION": params['location'].toUpperCase(),
        "WEATHER_WIDGET_DAY_SUMMARY": params['summary'],
        "WEATHER_WIDGET_TEMP_UNIT": params['unit'],
        "WEATHER_WIDGET_DAY_ICON": condition,
        "WEATHER_WIDGET_COLOUR": colour,
        "WEATHER_WIDGET_DAY_OF_WEEK": params['day']
    });
}

exports.current = function(session, params) {
    var condition = CONDITION_MAP[params['condition']];
    var colour = COLOUR_MAP[condition];

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
        "WEATHER_WIDGET_DAY_ICON": condition,
        "WEATHER_WIDGET_COLOUR": colour
    });
}