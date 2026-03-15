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
 * Tools module index.
 * Exports tool definitions and execution.
 */

var definitions = require('./definitions');
var executor = require('./executor');

module.exports = {
    getToolDefinitions: definitions.getToolDefinitions,
    getToolThought: definitions.getToolThought,
    executeTool: executor.executeTool,
    formatToolResult: executor.formatToolResult
};