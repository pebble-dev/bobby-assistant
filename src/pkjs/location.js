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

var cachedLon = undefined;
var cachedLat = undefined;

exports.update = function() {
    // start with whatever we knew before, if anything.
    var oldLon = localStorage.getItem('oldLon');
    var oldLat = localStorage.getItem('oldLat');
    if (oldLon && oldLat) {
        cachedLon = parseFloat(oldLon);
        cachedLat = parseFloat(oldLat);
        console.log("Cached location restored: (" + cachedLat + ", " + cachedLon + ")");
    }
    
    navigator.geolocation.getCurrentPosition(function(pos) {
        cachedLat = pos.coords.latitude;
        cachedLon = pos.coords.longitude;
        console.log("position updated: (" + cachedLat + ", " + cachedLon + ")");
        localStorage.setItem('oldLon', cachedLon);
        localStorage.setItem('oldLat', cachedLat);
    }, function (err) {
        console.log("Failed to update location: " + err);
    }, {
        enableHighAccuracy: true,
        maximumAge: 300000,
        timeout: 30000,
    });
}

exports.isReady = function() {
    return !!(cachedLon && cachedLat);
}

exports.getPos = function() {
    return {lon: cachedLon, lat: cachedLat};
}
