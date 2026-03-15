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

var messageQueue = require('./message_queue').Queue;

var CHUNK_SIZE = 200;

function ImageManager() {
    this.nextImageId = 1;
}

ImageManager.prototype.sendImage = function(width, height, /* number[]*/ imageData) {
    var imageId = this.nextImageId++;
    var chunks = Math.ceil(imageData.length / CHUNK_SIZE);
    messageQueue.enqueue({
        IMAGE_ID: imageId,
        IMAGE_START_BYTE_SIZE: imageData.length,
        IMAGE_WIDTH: width,
        IMAGE_HEIGHT: height,
    });
    console.log("Sending image " + imageId + " with size " + imageData.length + " bytes");
    setTimeout(function() {
        for (var i = 0; i < chunks; i++) {
            var start = i * CHUNK_SIZE;
            var end = Math.min(start + CHUNK_SIZE, imageData.length);
            var chunk = imageData.slice(start, end);
            console.log(chunk.length);
            console.log(JSON.stringify(chunk));
            messageQueue.enqueue({
                IMAGE_ID: imageId,
                IMAGE_CHUNK_OFFSET: start,
                IMAGE_CHUNK_DATA: chunk,
            });
        }
        messageQueue.enqueue({
            IMAGE_ID: imageId,
            IMAGE_COMPLETE: 1,
        });
        console.log("Enqueued " + chunks + " chunks for image " + imageId);
    }, 50);
    return imageId;
}

exports.ImageManager = ImageManager;
exports.sharedManager = new ImageManager();
