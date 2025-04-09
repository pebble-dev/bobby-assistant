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

var imageManager = require('../lib/image_transfer').sharedManager;

exports.map = function(session, params) {
    console.log(JSON.stringify(params));
    var base64pbi = params['image'];
    var width = params['width'];
    var height = params['height'];
    var pbi = atob(base64pbi); // TODO: this doesn't work on iOS!
    var imageData = new Array(pbi.length);
    for (var i = 0; i < pbi.length; i++) {
        imageData[i] = pbi.charCodeAt(i);
    }
    var imageId = imageManager.sendImage(width, height, imageData);
    var message = {
        MAP_WIDGET: 1,
        MAP_WIDGET_IMAGE_ID: imageId,
    };
    console.log(JSON.stringify(message));
    session.enqueue(message);
}
