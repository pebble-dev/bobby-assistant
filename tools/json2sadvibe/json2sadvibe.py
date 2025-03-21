#!/usr/bin/env python
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

import argparse
import os
import json

from libpebble2.protocol.base import PebblePacket
from libpebble2.protocol.base.types import *

class Note(PebblePacket):
	time_on = Uint32()
	time_off = Uint32()

class DisappointingVibeScore(PebblePacket):
	class Meta:
		endianness = '<'
	pattern_duration = Uint32()
	repeat_delay_ms = Uint32()
	note_count = Uint16()
	pattern = FixedList(Note)

def serialize(json_data):
    note_dictionary = {note['id']: note for note in json_data['notes']}

    pattern = [
    	Note(time_on=note_dictionary[x]['vibe_duration_ms'], time_off=note_dictionary[x]['brake_duration_ms'])
    	if note_dictionary[x]['strength'] > 50
    	else Note(time_on=0, time_off=note_dictionary[x]['vibe_duration_ms'] + note_dictionary[x]['brake_duration_ms'])
    	for x in json_data['pattern']]

    score = DisappointingVibeScore(
    	pattern_duration=sum(x.time_on+x.time_off for x in pattern),
    	repeat_delay_ms=json_data.get('repeat_delay_ms', 0),
    	pattern=pattern,
    	note_count=len(pattern)*2
	)

    # do the dirty work
    return score.serialise()


def convert(file_path, out_path=None):
    if out_path is None:
        out_path = os.path.splitext(file_path)[0] + ".vibe"

    with open(out_path, 'wb') as o:
        convert_to_file(file_path, o)


def convert_to_file(input_file_path, output_file):
    with open(input_file_path, 'r') as f:
        data = json.loads(f.read())

    output_file.write(serialize(data))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('json_file')
    args = parser.parse_args()

    convert(args.json_file)
