# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import waflib.Context
import waflib.Logs

def get_git_revision(ctx):
    try:
        tag = ctx.cmd_and_log(['git', 'describe', '--dirty'], quiet=waflib.Context.BOTH).strip()
        description = ctx.cmd_and_log(['git', 'describe', '--dirty', '--long'], quiet=waflib.Context.BOTH).strip()
        commit = ctx.cmd_and_log(['git', 'rev-parse', 'HEAD'], quiet=waflib.Context.BOTH).strip()
        commit_short = ctx.cmd_and_log(['git', 'rev-parse', '--short', 'HEAD'], quiet=waflib.Context.BOTH).strip()
        timestamp = ctx.cmd_and_log(['git', 'log', '-1', '--format=%ct', 'HEAD'], quiet=waflib.Context.BOTH).strip()
    except Exception:
        waflib.Logs.warn('get_git_version: unable to determine git revision')
        tag, description, commit, commit_short, timestamp = "?", "?", "?", "?", "1"

    return {'TAG': tag,
            'DESCRIPTION': description,
            'COMMIT': commit,
            'COMMIT_SHORT': commit_short,
            'TIMESTAMP': timestamp}
