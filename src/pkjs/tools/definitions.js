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
 * OpenAI-format tool definitions for Clawd.
 * These are ported from the Go backend and define the tools available to the LLM.
 */

/**
 * Get all available tool definitions in OpenAI format.
 * @param {Array<string>} capabilities - List of supported capabilities
 * @returns {Array<object>} Array of tool definitions
 */
function getToolDefinitions(capabilities) {
    capabilities = capabilities || [];
    var tools = [];

    // Alarm tools
    if (capabilities.indexOf('set_alarm') !== -1) {
        if (capabilities.indexOf('named_alarms') !== -1) {
            // New clients support named alarms
            tools.push({
                type: 'function',
                function: {
                    name: 'set_alarm',
                    description: 'Set an alarm for a given time.',
                    parameters: {
                        type: 'object',
                        properties: {
                            time: {
                                type: 'string',
                                description: 'The time to schedule the alarm for in ISO 8601 format, e.g. "2023-07-12T00:00:00-07:00". Must always be in the future.',
                                nullable: true
                            },
                            name: {
                                type: 'string',
                                description: 'Only if explicitly specified by the user, the name of the alarm. Use title case. If the user didn\'t ask to name the alarm, just leave it empty.',
                                nullable: true
                            }
                        },
                        required: ['time']
                    }
                }
            });
        } else {
            // Old clients don't support names
            tools.push({
                type: 'function',
                function: {
                    name: 'set_alarm',
                    description: 'Set an alarm for a given time.',
                    parameters: {
                        type: 'object',
                        properties: {
                            time: {
                                type: 'string',
                                description: 'The time to schedule the alarm for in ISO 8601 format, e.g. "2023-07-12T00:00:00-07:00". Must always be in the future.',
                                nullable: true
                            }
                        },
                        required: ['time']
                    }
                }
            });
        }

        tools.push({
            type: 'function',
            function: {
                name: 'get_alarms',
                description: 'Get any existing alarms.',
                parameters: {
                    type: 'object',
                    properties: {}
                }
            }
        });

        tools.push({
            type: 'function',
            function: {
                name: 'delete_alarm',
                description: 'Delete a specific alarm by its expiration time.',
                parameters: {
                    type: 'object',
                    properties: {
                        time: {
                            type: 'string',
                            description: 'The time of the alarm to delete in ISO 8601 format, e.g. "2023-07-12T00:00:00-07:00".',
                            nullable: true
                        }
                    }
                }
            }
        });
    }

    // Timer tools
    if (capabilities.indexOf('set_alarm') !== -1) {
        if (capabilities.indexOf('named_alarms') !== -1) {
            tools.push({
                type: 'function',
                function: {
                    name: 'set_timer',
                    description: 'Set a timer for a given time.',
                    parameters: {
                        type: 'object',
                        properties: {
                            duration_seconds: {
                                type: 'integer',
                                description: 'The number of seconds to set the timer for.',
                                nullable: true
                            },
                            name: {
                                type: 'string',
                                description: 'Only if explicitly specified by the user, the name of the timer. Use title case. If the user didn\'t ask to name the timer, just leave it empty.',
                                nullable: true
                            }
                        }
                    }
                }
            });
        } else {
            tools.push({
                type: 'function',
                function: {
                    name: 'set_timer',
                    description: 'Set a timer for a given duration.',
                    parameters: {
                        type: 'object',
                        properties: {
                            duration_seconds: {
                                type: 'integer',
                                description: 'The number of seconds to set the timer for.',
                                nullable: true
                            }
                        }
                    }
                }
            });
        }

        tools.push({
            type: 'function',
            function: {
                name: 'get_timers',
                description: 'Get any existing timers.',
                parameters: {
                    type: 'object',
                    properties: {}
                }
            }
        });

        tools.push({
            type: 'function',
            function: {
                name: 'delete_timer',
                description: 'Delete a specific timer by its expiration time.',
                parameters: {
                    type: 'object',
                    properties: {
                        time: {
                            type: 'string',
                            description: 'The time of the timer to delete in ISO 8601 format, e.g. "2023-07-12T00:00:00-07:00".'
                        }
                    },
                    required: ['time']
                }
            }
        });
    }

    // Reminder tools
    if (capabilities.indexOf('set_reminder') !== -1) {
        tools.push({
            type: 'function',
            function: {
                name: 'set_reminder',
                description: 'Set a reminder for the user to perform a task at a time. Either time or delay must be provided, but not both. If the user specifies a time but not a day, assume they meant the next time that time will happen.',
                parameters: {
                    type: 'object',
                    properties: {
                        time: {
                            type: 'string',
                            description: 'The time to schedule the reminder for in ISO 8601 format, e.g. "2023-07-12T00:00:00-07:00". Always assume the user\'s timezone unless otherwise specified.',
                            nullable: true
                        },
                        delay_mins: {
                            type: 'integer',
                            description: 'The delay from now to when the reminder should be scheduled, in minutes.',
                            nullable: true
                        },
                        what: {
                            type: 'string',
                            description: 'What to remind the user to do.',
                            nullable: true
                        }
                    },
                    required: ['what']
                }
            }
        });

        tools.push({
            type: 'function',
            function: {
                name: 'get_reminders',
                description: 'Get a list of all active reminders.',
                parameters: {
                    type: 'object',
                    properties: {}
                }
            }
        });

        tools.push({
            type: 'function',
            function: {
                name: 'delete_reminder',
                description: 'Delete a specific reminder by its ID.',
                parameters: {
                    type: 'object',
                    properties: {
                        id: {
                            type: 'string',
                            description: 'The ID of the reminder to delete. You *must* call get_reminders first to discover the ID of the correct reminder.'
                        }
                    },
                    required: ['id']
                }
            }
        });
    }

    // Time tool
    tools.push({
        type: 'function',
        function: {
            name: 'get_time_elsewhere',
            description: 'Get the current time in a given valid tzdb timezone. Not all cities have a tzdb entry - be sure to use one that exists. Call multiple times to find the time in multiple timezones.',
            parameters: {
                type: 'object',
                properties: {
                    timezone: {
                        type: 'string',
                        description: 'The timezone, e.g. "America/Los_Angeles".'
                    },
                    offset: {
                        type: 'number',
                        description: 'The number of seconds to add to the current time, if checking a different time. Omit or set to zero for current time.'
                    }
                },
                required: ['timezone']
            }
        }
    });

    return tools;
}

/**
 * Get the "thought" text for a tool call.
 * This is shown to the user while the tool is executing.
 * @param {string} toolName - Name of the tool
 * @param {object} args - Tool arguments
 * @returns {string} The thought text
 */
function getToolThought(toolName, args) {
    switch (toolName) {
        case 'set_alarm':
            if (args.time) {
                return 'Setting an alarm';
            }
            return 'Contemplating time';

        case 'set_timer':
            if (args.duration_seconds) {
                return 'Setting a timer';
            }
            return 'Contemplating time';

        case 'get_alarms':
            return 'Checking your alarms';

        case 'get_timers':
            return 'Checking your timers';

        case 'delete_alarm':
            return 'Deleting an alarm';

        case 'delete_timer':
            return 'Deleting a timer';

        case 'set_reminder':
            return 'Setting a reminder';

        case 'get_reminders':
            return 'Getting your reminders';

        case 'delete_reminder':
            return 'Deleting a reminder';

        case 'get_time_elsewhere':
            if (args.timezone) {
                var parts = args.timezone.split('/');
                var place = parts[parts.length - 1].replace(/_/g, ' ');
                return 'Checking the time in ' + place;
            }
            return 'Checking the time';

        default:
            return 'Working...';
    }
}

exports.getToolDefinitions = getToolDefinitions;
exports.getToolThought = getToolThought;