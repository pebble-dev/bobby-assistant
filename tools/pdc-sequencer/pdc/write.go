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
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
)

func WriteImageFile(writer io.Writer, image *DrawCommandImage) error {
	buf := new(bytes.Buffer)
	if err := writeImage(buf, image); err != nil {
		return err
	}
	if err := writeHeader(writer, "PDCI", uint32(buf.Len())); err != nil {
		return fmt.Errorf("failed to write header: %w", err)
	}
	if _, err := writer.Write(buf.Bytes()); err != nil {
		return fmt.Errorf("failed to write image: %w", err)
	}
	return nil
}

func WriteImageSequence(writer io.Writer, sequence *DrawCommandSequence) error {
	buf := new(bytes.Buffer)
	if err := writeSequence(buf, sequence); err != nil {
		return err
	}
	if err := writeHeader(writer, "PDCS", uint32(buf.Len())); err != nil {
		return fmt.Errorf("failed to write header: %w", err)
	}
	if _, err := writer.Write(buf.Bytes()); err != nil {
		return fmt.Errorf("failed to write sequence: %w", err)
	}
	return nil
}

func writeHeader(writer io.Writer, magic string, size uint32) error {
	if _, err := writer.Write([]byte(magic)); err != nil {
		return fmt.Errorf("failed to write magic string: %w", err)
	}
	if err := binary.Write(writer, byteOrder, size); err != nil {
		return fmt.Errorf("failed to write size: %w", err)
	}
	return nil
}

func writeImage(writer io.Writer, image *DrawCommandImage) error {
	if err := binary.Write(writer, byteOrder, image.Version); err != nil {
		return fmt.Errorf("failed to write pdci version: %w", err)
	}
	if err := binary.Write(writer, byteOrder, image.Reserved); err != nil {
		return fmt.Errorf("failed to write pdci reserved: %w", err)
	}
	if err := writeViewBox(writer, &image.ViewBox); err != nil {
		return fmt.Errorf("failed to write pdci viewbox: %w", err)
	}
	if err := writeDrawCommandList(writer, &image.CommandList); err != nil {
		return fmt.Errorf("failed to write pdci command list: %w", err)
	}
	return nil
}

func writeSequence(writer io.Writer, sequence *DrawCommandSequence) error {
	if err := binary.Write(writer, byteOrder, sequence.Version); err != nil {
		return fmt.Errorf("failed to write pdcs version: %w", err)
	}
	if err := binary.Write(writer, byteOrder, sequence.Reserved); err != nil {
		return fmt.Errorf("failed to write pdcs reserved: %w", err)
	}
	if err := writeViewBox(writer, &sequence.ViewBox); err != nil {
		return fmt.Errorf("failed to write pdcs viewbox: %w", err)
	}
	if err := binary.Write(writer, byteOrder, sequence.PlayCount); err != nil {
		return fmt.Errorf("failed to write pdcs play count: %w", err)
	}
	if err := binary.Write(writer, byteOrder, uint16(len(sequence.Frames))); err != nil {
		return fmt.Errorf("failed to write pdcs frame count: %w", err)
	}
	for i, frame := range sequence.Frames {
		if err := writeDrawCommandFrame(writer, &frame); err != nil {
			return fmt.Errorf("failed to write pdcs frame %d: %w", i, err)
		}
	}
	return nil
}

func writeDrawCommandFrame(writer io.Writer, frame *DrawCommandFrame) error {
	if err := binary.Write(writer, byteOrder, frame.DurationMs); err != nil {
		return fmt.Errorf("failed to write frame duration: %w", err)
	}
	if err := writeDrawCommandList(writer, &frame.CommandList); err != nil {
		return fmt.Errorf("failed to write frame command list: %w", err)
	}
	return nil
}

func writeViewBox(writer io.Writer, viewBox *ViewBox) error {
	if err := binary.Write(writer, byteOrder, viewBox.Width); err != nil {
		return fmt.Errorf("failed to write viewbox width: %w", err)
	}
	if err := binary.Write(writer, byteOrder, viewBox.Height); err != nil {
		return fmt.Errorf("failed to write viewbox height: %w", err)
	}
	return nil
}

func writeDrawCommandList(writer io.Writer, list *DrawCommandList) error {
	if err := binary.Write(writer, byteOrder, uint16(len(list.Commands))); err != nil {
		return fmt.Errorf("failed to write command count: %w", err)
	}
	for _, command := range list.Commands {
		if err := writeDrawCommand(writer, &command); err != nil {
			return fmt.Errorf("failed to write command: %w", err)
		}
	}
	return nil
}

func writeDrawCommand(writer io.Writer, command *DrawCommand) error {
	if err := binary.Write(writer, byteOrder, command.Type); err != nil {
		return fmt.Errorf("failed to write command type: %w", err)
	}
	if err := binary.Write(writer, byteOrder, command.Flags); err != nil {
		return fmt.Errorf("failed to write command flags: %w", err)
	}
	if err := binary.Write(writer, byteOrder, command.StrokeColor); err != nil {
		return fmt.Errorf("failed to write command stroke color: %w", err)
	}
	if err := binary.Write(writer, byteOrder, command.StrokeWidth); err != nil {
		return fmt.Errorf("failed to write command stroke width: %w", err)
	}
	if err := binary.Write(writer, byteOrder, command.FillColor); err != nil {
		return fmt.Errorf("failed to write command fill color: %w", err)
	}
	if err := binary.Write(writer, byteOrder, command.Radius); err != nil {
		return fmt.Errorf("failed to write command radius: %w", err)
	}
	if err := binary.Write(writer, byteOrder, uint16(len(command.Points))); err != nil {
		return fmt.Errorf("failed to write command point count: %w", err)
	}
	for i, point := range command.Points {
		if err := writePoint(writer, &point); err != nil {
			return fmt.Errorf("failed to write point %d: %w", i, err)
		}
	}
	return nil
}

func writePoint(writer io.Writer, point *Point) error {
	if err := binary.Write(writer, byteOrder, point.X); err != nil {
		return fmt.Errorf("failed to write point x: %w", err)
	}
	if err := binary.Write(writer, byteOrder, point.Y); err != nil {
		return fmt.Errorf("failed to write point y: %w", err)
	}
	return nil
}
