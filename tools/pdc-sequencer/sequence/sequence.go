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

package sequence

import (
	"fmt"
	"github.com/pebble-dev/bobby-assistant/tools/pdc-sequencer/pdc"
	"os"
	"path/filepath"
)

type inputFile struct {
	filename string
	image    *pdc.DrawCommandImage
}

func ConstructSequence(pattern, output string, delayMs, playCount uint16) error {
	// Construct the sequence
	matches, err := filepath.Glob(pattern)
	if err != nil {
		return err
	}
	if len(matches) == 0 {
		return fmt.Errorf("no files found matching %q", pattern)
	}
	var inputs []inputFile
	for _, match := range matches {
		err := func() error {
			f, err := os.Open(match)
			if err != nil {
				return fmt.Errorf("failed to open image %q: %w", match, err)
			}
			defer f.Close()
			image, err := pdc.ParseImageFile(f)
			if err != nil {
				return fmt.Errorf("failed to parse image %q: %w", match, err)
			}
			inputs = append(inputs, inputFile{filename: match, image: image})
			return nil
		}()
		if err != nil {
			return err
		}
	}

	fmt.Println("Input images:")
	for i, input := range inputs {
		fmt.Printf("%02d) %s\n", i, input.filename)
	}

	// The bounding box for every image must match.
	viewBox := inputs[0].image.ViewBox
	for _, input := range inputs {
		if viewBox != input.image.ViewBox {
			return fmt.Errorf("image %q has a different viewbox (%d, %d) to %q (%d, %d)", input.filename, input.image.ViewBox.Width, input.image.ViewBox.Height, inputs[0].filename, viewBox.Width, viewBox.Height)
		}
	}

	// Construct the sequence
	sequence := &pdc.DrawCommandSequence{
		Version:   1,
		ViewBox:   viewBox,
		PlayCount: playCount,
	}
	var lastList *pdc.DrawCommandList
	for _, input := range inputs {
		if lastList != nil {
			if pdc.CommandListsEqual(lastList, &input.image.CommandList) {
				sequence.Frames[len(sequence.Frames)-1].DurationMs += delayMs
				continue
			}
		}
		frame := pdc.DrawCommandFrame{
			DurationMs:  delayMs,
			CommandList: input.image.CommandList,
		}
		sequence.Frames = append(sequence.Frames, frame)
		lastList = &frame.CommandList
	}

	// Explain the sequence
	fmt.Println("Result sequence:")
	fmt.Printf("  View box: %d x %d\n", sequence.ViewBox.Width, sequence.ViewBox.Height)
	fmt.Printf("  Play count: %d\n", sequence.PlayCount)
	for i, frame := range sequence.Frames {
		fmt.Printf("  Frame %02d: %d ms\n", i, frame.DurationMs)
	}

	// Write the sequence
	outputFile, err := os.Create(output)
	if err != nil {
		return fmt.Errorf("failed to create output file %q: %w", output, err)
	}
	defer outputFile.Close()
	if err := pdc.WriteImageSequence(outputFile, sequence); err != nil {
		return fmt.Errorf("failed to write sequence to %q: %w", output, err)
	}

	return nil
}
