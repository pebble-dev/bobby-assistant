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
 * Tool executor for Clawd.
 * Executes tool calls locally on the phone app.
 */

var alarms = require('../actions/alarms');
var reminders = require('../actions/reminders');
var settings = require('../actions/settings');
var definitions = require('./definitions');

/**
 * Execute a tool call.
 * @param {string} toolName - Name of the tool
 * @param {object} args - Tool arguments
 * @param {object} session - Session object for communicating with watch
 * @returns {Promise<object>} Tool result
 */
function executeTool(toolName, args, session) {
    return new Promise(function(resolve, reject) {
        console.log('Executing tool:', toolName, JSON.stringify(args));

        switch (toolName) {
            case 'set_alarm':
                executeSetAlarm(args, session, resolve, reject);
                break;

            case 'set_timer':
                executeSetTimer(args, session, resolve, reject);
                break;

            case 'get_alarms':
                executeGetAlarm(false, session, resolve, reject);
                break;

            case 'get_timers':
                executeGetAlarm(true, session, resolve, reject);
                break;

            case 'delete_alarm':
                executeDeleteAlarm(args, session, resolve, reject);
                break;

            case 'delete_timer':
                executeDeleteTimer(args, session, resolve, reject);
                break;

            case 'set_reminder':
                executeSetReminder(args, session, resolve, reject);
                break;

            case 'get_reminders':
                executeGetReminders(session, resolve, reject);
                break;

            case 'delete_reminder':
                executeDeleteReminder(args, session, resolve, reject);
                break;

            case 'get_time_elsewhere':
                executeGetTimeElsewhere(args, resolve, reject);
                break;

            case 'wikipedia':
                executeWikipedia(args, resolve, reject);
                break;

            default:
                reject(new Error('Unknown tool: ' + toolName));
        }
    });
}

/**
 * Execute set_alarm tool.
 */
function executeSetAlarm(args, session, resolve, reject) {
    var message = {
        action: 'set_alarm',
        time: args.time,
        name: args.name || null,
        isTimer: false,
        cancel: false
    };

    alarms.setAlarm(session, message, function(result) {
        resolve(result);
    });
}

/**
 * Execute set_timer tool.
 */
function executeSetTimer(args, session, resolve, reject) {
    var duration = args.duration_seconds || 0;
    duration += (args.duration_minutes || 0) * 60;
    duration += (args.duration_hours || 0) * 3600;

    if (duration === 0) {
        resolve({ error: 'You need to pass the timer duration in seconds to duration_seconds (e.g. duration_seconds=300 for a 5 minute timer).' });
        return;
    }

    var message = {
        action: 'set_alarm',
        duration: duration,
        name: args.name || null,
        isTimer: true,
        cancel: false
    };

    alarms.setAlarm(session, message, function(result) {
        resolve(result);
    });
}

/**
 * Execute get_alarm tool.
 */
function executeGetAlarm(isTimer, session, resolve, reject) {
    var message = {
        action: 'get_alarm',
        isTimer: isTimer
    };

    alarms.getAlarm(session, message, function(result) {
        resolve(result);
    });
}

/**
 * Execute delete_alarm tool.
 */
function executeDeleteAlarm(args, session, resolve, reject) {
    var message = {
        action: 'set_alarm',
        time: args.time,
        isTimer: false,
        cancel: true
    };

    alarms.setAlarm(session, message, function(result) {
        resolve(result);
    });
}

/**
 * Execute delete_timer tool.
 */
function executeDeleteTimer(args, session, resolve, reject) {
    var message = {
        action: 'set_alarm',
        time: args.time,
        isTimer: true,
        cancel: true
    };

    alarms.setAlarm(session, message, function(result) {
        resolve(result);
    });
}

/**
 * Execute set_reminder tool.
 */
function executeSetReminder(args, session, resolve, reject) {
    var time = args.time;
    var delay = args.delay_mins;

    // Handle delay
    if (delay && delay > 0) {
        time = new Date(Date.now() + delay * 60 * 1000).toISOString();
    }

    var message = {
        action: 'set_reminder',
        time: time,
        what: args.what
    };

    reminders.setReminder(session, message, function(result) {
        resolve(result);
    });
}

/**
 * Execute get_reminders tool.
 */
function executeGetReminders(session, resolve, reject) {
    var message = {
        action: 'get_reminders'
    };

    reminders.getReminders(session, message, function(result) {
        resolve(result);
    });
}

/**
 * Execute delete_reminder tool.
 */
function executeDeleteReminder(args, session, resolve, reject) {
    var message = {
        action: 'delete_reminder',
        id: args.id
    };

    reminders.deleteReminder(session, message, function(result) {
        resolve(result);
    });
}

/**
 * Execute get_time_elsewhere tool.
 */
function executeGetTimeElsewhere(args, resolve, reject) {
    try {
        var timezone = args.timezone;
        var offset = args.offset || 0;

        // Calculate the time in the specified timezone
        var now = new Date(Date.now() + offset * 1000);
        var options = {
            timeZone: timezone,
            weekday: 'short',
            month: 'short',
            day: 'numeric',
            hour: 'numeric',
            minute: '2-digit',
            second: '2-digit',
            year: 'numeric',
            timeZoneName: 'short'
        };

        var timeStr = now.toLocaleString('en-US', options);
        resolve({ time: timeStr });
    } catch (err) {
        if (err.message && err.message.indexOf('time zone') !== -1) {
            resolve({ error: 'The timezone "' + args.timezone + '" is not valid' });
        } else {
            resolve({ error: 'Failed to get time: ' + err.message });
        }
    }
}

/**
 * Execute wikipedia tool.
 */
function executeWikipedia(args, resolve, reject) {
    var wiki = args.wiki || 'wikipedia';
    var articleName = args.article_name;
    var completeArticle = args.complete_article || false;

    // Map of wiki URLs
    var wikiUrls = {
        'wikipedia': 'https://en.wikipedia.org/',
        'bulbapedia': 'https://bulbapedia.bulbagarden.net/'
    };

    if (!wikiUrls[wiki]) {
        resolve({ error: 'Unknown wiki: ' + wiki });
        return;
    }

    var baseUrl = wikiUrls[wiki];

    // First try to get the article
    var url = baseUrl + 'w/api.php?action=query&prop=revisions&rvprop=content&format=json&titles=' + encodeURIComponent(articleName);

    if (!completeArticle) {
        url += '&rvsection=0';
    }

    fetch(url).then(function(response) {
        return response.json();
    }).then(function(data) {
        // Check if page exists
        var pages = data.query && data.query.pages;
        if (!pages) {
            // Try searching
            return searchWiki(wiki, articleName).then(function(title) {
                if (title) {
                    // Retry with the found title
                    return fetchWikiArticle(baseUrl, title, completeArticle);
                }
                return { error: wiki + ' page "' + articleName + '" not found. Try to answer using your general knowledge.' };
            });
        }

        var pageId = Object.keys(pages)[0];
        if (pageId === '-1') {
            // Page not found, try search
            return searchWiki(wiki, articleName).then(function(title) {
                if (title) {
                    return fetchWikiArticle(baseUrl, title, completeArticle);
                }
                return { error: wiki + ' page "' + articleName + '" not found. Try to answer using your general knowledge.' };
            });
        }

        var content = pages[pageId].revisions[0]['*'];
        var addendum = completeArticle ? '' : '\n\nThis was only the summary. If necessary, more information can be returned by repeating the query with complete_article = true.';
        return { results: content + addendum };
    }).catch(function(err) {
        resolve({ error: 'Failed to fetch wiki article: ' + err.message });
    }).then(function(result) {
        resolve(result);
    });
}

/**
 * Search for a wiki article.
 */
function searchWiki(wiki, query) {
    var wikiUrls = {
        'wikipedia': 'https://en.wikipedia.org/',
        'bulbapedia': 'https://bulbapedia.bulbagarden.net/'
    };

    var url = wikiUrls[wiki] + 'w/api.php?action=opensearch&limit=5&namespace=0&format=json&redirects=resolve&search=' + encodeURIComponent(query);

    return fetch(url).then(function(response) {
        return response.json();
    }).then(function(data) {
        if (data && data.length >= 2 && data[1] && data[1].length > 0) {
            return data[1][0]; // Return first result title
        }
        return null;
    }).catch(function() {
        return null;
    });
}

/**
 * Fetch a wiki article by title.
 */
function fetchWikiArticle(baseUrl, title, completeArticle) {
    var url = baseUrl + 'w/api.php?action=query&prop=revisions&rvprop=content&format=json&titles=' + encodeURIComponent(title);
    if (!completeArticle) {
        url += '&rvsection=0';
    }

    return fetch(url).then(function(response) {
        return response.json();
    }).then(function(data) {
        var pages = data.query && data.query.pages;
        if (!pages) {
            return { error: 'Article not found' };
        }

        var pageId = Object.keys(pages)[0];
        if (pageId === '-1') {
            return { error: 'Article not found' };
        }

        var content = pages[pageId].revisions[0]['*'];
        var addendum = completeArticle ? '' : '\n\nThis was only the summary. If necessary, more information can be returned by repeating the query with complete_article = true.';
        return { results: content + addendum };
    });
}

/**
 * Format a tool result for sending back to the LLM.
 * @param {string} toolCallId - The tool call ID
 * @param {string} toolName - Name of the tool
 * @param {object} result - The tool result
 * @returns {object} Formatted tool result message
 */
function formatToolResult(toolCallId, toolName, result) {
    return {
        tool_call_id: toolCallId,
        role: 'tool',
        content: JSON.stringify(result)
    };
}

exports.executeTool = executeTool;
exports.formatToolResult = formatToolResult;
exports.getToolDefinitions = definitions.getToolDefinitions;
exports.getToolThought = definitions.getToolThought;