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

package pdc

type Point struct {
	X, Y int16
}

type ViewBox struct {
	Width, Height uint16
}

const (
	DrawCommandTypeInvalid     = 0
	DrawCommandTypePath        = 1
	DrawCommandTypeCircle      = 2
	DrawCommandTypePrecisePath = 3
)

const (
	DrawCommandFlagHidden = 1 << 0
)

type DrawCommand struct {
	Type        uint8
	Flags       uint8
	StrokeColor uint8
	StrokeWidth uint8
	FillColor   uint8
	Radius      uint16
	Points      []Point
}

type DrawCommandList struct {
	Commands []DrawCommand
}

type DrawCommandFrame struct {
	DurationMs  uint16
	CommandList DrawCommandList
}
type DrawCommandImage struct {
	Version     uint8
	Reserved    uint8
	ViewBox     ViewBox
	CommandList DrawCommandList
}

type DrawCommandSequence struct {
	Version   uint8
	Reserved  uint8
	ViewBox   ViewBox
	PlayCount uint16
	Frames    []DrawCommandFrame
}

func CommandListsEqual(a, b *DrawCommandList) bool {
	if len(a.Commands) != len(b.Commands) {
		return false
	}
	for i := range a.Commands {
		if !commandsEqual(&a.Commands[i], &b.Commands[i]) {
			return false
		}
	}
	return true
}

func commandsEqual(a, b *DrawCommand) bool {
	if a.Type != b.Type {
		return false
	}
	if a.Flags != b.Flags {
		return false
	}
	if a.StrokeColor != b.StrokeColor {
		return false
	}
	if a.StrokeWidth != b.StrokeWidth {
		return false
	}
	if a.FillColor != b.FillColor {
		return false
	}
	if a.Radius != b.Radius {
		return false
	}
	if len(a.Points) != len(b.Points) {
		return false
	}
	for i := range a.Points {
		if a.Points[i] != b.Points[i] {
			return false
		}
	}
	return true
}
