// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"flag"
	"fmt"
	"github.com/pebble-dev/bobby-assistant/tools/pdc-sequencer/sequence"
	"os"
	"strconv"
	"strings"
)

type Options struct {
	Pattern   string
	Output    string
	DelayMs   uint
	PlayCount uint
}

func parseFlags() Options {
	o := Options{}
	flag.StringVar(&o.Pattern, "pattern", ".", "Pattern containing PDC files to join")
	flag.StringVar(&o.Output, "output", "", "Output filename to write to")
	flag.UintVar(&o.DelayMs, "delay", 33, "Delay between playing each frame in milliseconds")
	var playCount string
	flag.StringVar(&playCount, "playcount", "1", "Number of times to play the animation, or 'forever'")
	flag.Parse()

	if playCount == "forever" {
		o.PlayCount = 0xFFFF
	} else {
		count, err := strconv.ParseUint(playCount, 10, 16)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Invalid playcount: %q\n", playCount)
			os.Exit(1)
		}
		o.PlayCount = uint(count)
	}

	if o.Output == "" {
		fmt.Fprintf(os.Stderr, "Output filename must be specified\n")
		os.Exit(1)
	}
	return o
}

func main() {
	options := parseFlags()
	if strings.HasPrefix(options.Pattern, "~/") {
		options.Pattern = strings.Replace(options.Pattern, "~", os.Getenv("HOME"), 1)
	}

	err := sequence.ConstructSequence(options.Pattern, options.Output, uint16(options.DelayMs), uint16(options.PlayCount))
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to construct sequence: %v\n", err)
		os.Exit(1)
	}
}
