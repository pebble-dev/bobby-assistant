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

import (
	"encoding/binary"
	"fmt"
	"io"
)

var byteOrder = binary.LittleEndian

func ParseImageFile(reader io.Reader) (*DrawCommandImage, error) {
	magic, fileSize, err := parseHeader(reader)
	if err != nil {
		return nil, err
	}
	if magic != "PDCI" {
		return nil, fmt.Errorf("invalid magic string: %s (expected PDCI)", magic)
	}
	return parseImage(reader, fileSize)
}

func ParseImageSequence(reader io.Reader) (*DrawCommandSequence, error) {
	magic, fileSize, err := parseHeader(reader)
	if err != nil {
		return nil, err
	}
	if magic != "PDCS" {
		return nil, fmt.Errorf("invalid magic string: %s (expected PDCS)", magic)
	}
	return parseSequence(reader, fileSize)
}

func parseHeader(reader io.Reader) (string, uint32, error) {
	magic := make([]byte, 4)
	if _, err := io.ReadFull(reader, magic); err != nil {
		return "", 0, err
	}
	magicString := string(magic)
	if magicString != "PDCI" && magicString != "PDCS" {
		return magicString, 0, fmt.Errorf("invalid magic string: %s", magicString)
	}
	var size uint32
	if err := binary.Read(reader, byteOrder, &size); err != nil {
		return magicString, 0, err
	}
	return magicString, size, nil
}

func parseImage(reader io.Reader, size uint32) (*DrawCommandImage, error) {
	var image DrawCommandImage
	var err error
	if err := binary.Read(reader, byteOrder, &image.Version); err != nil {
		return nil, fmt.Errorf("failed to read pdci version: %w", err)
	}
	if err := binary.Read(reader, byteOrder, &image.Reserved); err != nil {
		return nil, fmt.Errorf("failed to read pdci reserved: %w", err)
	}
	if image.Reserved != 0 {
		return nil, fmt.Errorf("invalid reserved value: %d (must be 0)", image.Reserved)
	}
	image.ViewBox, err = parseViewBox(reader)
	if err != nil {
		return nil, fmt.Errorf("failed to parse viewbox: %w", err)
	}
	image.CommandList, err = parseCommandList(reader)
	if err != nil {
		return nil, fmt.Errorf("failed to parse command list: %w", err)
	}
	return &image, nil
}

func parseSequence(reader io.Reader, size uint32) (*DrawCommandSequence, error) {
	var sequence DrawCommandSequence
	var err error
	if err := binary.Read(reader, byteOrder, &sequence.Version); err != nil {
		return nil, fmt.Errorf("failed to read pdcs version: %w", err)
	}
	if err := binary.Read(reader, byteOrder, &sequence.Reserved); err != nil {
		return nil, fmt.Errorf("failed to read pdcs reserved: %w", err)
	}
	if sequence.Reserved != 0 {
		return nil, fmt.Errorf("invalid reserved value: %d (must be 0)", sequence.Reserved)
	}
	sequence.ViewBox, err = parseViewBox(reader)
	if err != nil {
		return nil, fmt.Errorf("failed to parse viewbox: %w", err)
	}
	if err := binary.Read(reader, byteOrder, &sequence.PlayCount); err != nil {
		return nil, fmt.Errorf("failed to read play count: %w", err)
	}
	var frameCount uint16
	if err := binary.Read(reader, byteOrder, &frameCount); err != nil {
		return nil, fmt.Errorf("failed to read frame count: %w", err)
	}
	sequence.Frames = make([]DrawCommandFrame, 0, frameCount)
	for i := range frameCount {
		frame, err := parseFrame(reader)
		if err != nil {
			return nil, fmt.Errorf("failed to parse frame %d: %w", i, err)
		}
		sequence.Frames = append(sequence.Frames, frame)
	}
	return &sequence, nil
}

func parseFrame(reader io.Reader) (DrawCommandFrame, error) {
	var frame DrawCommandFrame
	if err := binary.Read(reader, byteOrder, &frame.DurationMs); err != nil {
		return DrawCommandFrame{}, fmt.Errorf("failed to read frame duration: %w", err)
	}
	var err error
	frame.CommandList, err = parseCommandList(reader)
	if err != nil {
		return DrawCommandFrame{}, fmt.Errorf("failed to parse frame command list: %w", err)
	}
	return frame, nil
}

func parseViewBox(reader io.Reader) (ViewBox, error) {
	var viewBox ViewBox
	if err := binary.Read(reader, byteOrder, &viewBox.Width); err != nil {
		return ViewBox{}, fmt.Errorf("failed to read width: %w", err)
	}
	if err := binary.Read(reader, byteOrder, &viewBox.Height); err != nil {
		return ViewBox{}, fmt.Errorf("failed to read height: %w", err)
	}
	return viewBox, nil
}

func parseCommandList(reader io.Reader) (DrawCommandList, error) {
	var commandList DrawCommandList
	var count uint16
	if err := binary.Read(reader, byteOrder, &count); err != nil {
		return DrawCommandList{}, fmt.Errorf("failed to read command count: %w", err)
	}
	if count == 0 {
		return DrawCommandList{}, fmt.Errorf("invalid command count: %d", count)
	}
	for i := range count {
		command, err := parseCommand(reader)
		if err != nil {
			return DrawCommandList{}, fmt.Errorf("failed to parse command %d: %w", i, err)
		}
		commandList.Commands = append(commandList.Commands, command)
	}
	return commandList, nil
}

func parseCommand(reader io.Reader) (DrawCommand, error) {
	var command DrawCommand
	if err := binary.Read(reader, byteOrder, &command.Type); err != nil {
		return DrawCommand{}, fmt.Errorf("failed to read command type: %w", err)
	}
	if err := binary.Read(reader, byteOrder, &command.Flags); err != nil {
		return DrawCommand{}, fmt.Errorf("failed to read command flags: %w", err)
	}
	if err := binary.Read(reader, byteOrder, &command.StrokeColor); err != nil {
		return DrawCommand{}, fmt.Errorf("failed to read stroke color: %w", err)
	}
	if err := binary.Read(reader, byteOrder, &command.StrokeWidth); err != nil {
		return DrawCommand{}, fmt.Errorf("failed to read stroke width: %w", err)
	}
	if err := binary.Read(reader, byteOrder, &command.FillColor); err != nil {
		return DrawCommand{}, fmt.Errorf("failed to read fill color: %w", err)
	}
	if err := binary.Read(reader, byteOrder, &command.Radius); err != nil {
		return DrawCommand{}, fmt.Errorf("failed to read radius: %w", err)
	}
	var pointCount uint16
	if err := binary.Read(reader, byteOrder, &pointCount); err != nil {
		return DrawCommand{}, fmt.Errorf("failed to read point count: %w", err)
	}
	for i := range pointCount {
		point, err := parsePoint(reader)
		if err != nil {
			return DrawCommand{}, fmt.Errorf("failed to parse point %d: %w", i, err)
		}
		command.Points = append(command.Points, point)
	}
	return command, nil
}

func parsePoint(reader io.Reader) (Point, error) {
	var point Point
	if err := binary.Read(reader, byteOrder, &point.X); err != nil {
		return Point{}, fmt.Errorf("failed to read X: %w", err)
	}
	if err := binary.Read(reader, byteOrder, &point.Y); err != nil {
		return Point{}, fmt.Errorf("failed to read Y: %w", err)
	}
	return point, nil
}
