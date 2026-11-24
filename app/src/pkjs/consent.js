/**
 * Copyright 2026 Rebble Foundation
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

// The consent text comes from the backend, so it can change (e.g. when the LLM provider changes)
// without an app release. We cache it so the consent screen still works if the fetch fails.

var messageQueue = require('./lib/message_queue').Queue;
var CONSENT_URL = require('./urls').CONSENT_URL;

var CACHE_KEY = 'consent-cache';

function getCached() {
    try {
        return JSON.parse(localStorage.getItem(CACHE_KEY));
    } catch (e) {
        return null;
    }
}

function fetchConsent(callback) {
    console.log("Fetching consent from " + CONSENT_URL);
    var req = new XMLHttpRequest();
    req.open('GET', CONSENT_URL, true);
    req.onload = function() {
        if (req.readyState !== 4) {
            return;
        }
        if (req.status !== 200) {
            console.log("Consent request returned error code " + req.status);
            callback(null);
            return;
        }
        var consent;
        try {
            consent = JSON.parse(req.responseText);
        } catch (e) {
            console.log("Consent response wasn't valid JSON: " + e);
            callback(null);
            return;
        }
        localStorage.setItem(CACHE_KEY, JSON.stringify(consent));
        callback(consent);
    };
    req.onerror = function() {
        console.log("Consent request failed.");
        callback(null);
    };
    req.send();
}

// Fetches the latest consent and tells the watch which version it requires.
exports.refresh = function() {
    fetchConsent(function(consent) {
        if (!consent) {
            return;
        }
        console.log("Required consent version: " + consent.version);
        messageQueue.enqueue({
            CONSENT_REQUIRED_VERSION: consent.version
        });
    });
};

function sendPages(consent) {
    var pages = (consent.pages || []).concat([consent.location]);
    for (var i = 0; i < pages.length; i++) {
        messageQueue.enqueue({
            CONSENT_VERSION: consent.version,
            CONSENT_PAGE_COUNT: pages.length,
            CONSENT_PAGE_INDEX: i,
            CONSENT_PAGE_TITLE: pages[i].title,
            CONSENT_PAGE_TEXT: pages[i].text
        });
    }
}

// Sends the consent pages to the watch. The location page is always last.
exports.handleRequest = function() {
    fetchConsent(function(consent) {
        consent = consent || getCached();
        if (!consent) {
            messageQueue.enqueue({CONSENT_ERROR: 1});
            return;
        }
        sendPages(consent);
    });
};
