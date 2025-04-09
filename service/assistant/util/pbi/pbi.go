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

package pbi

import (
	"encoding/binary"
	"fmt"
	"image"
	"image/color"
	"io"
	"log"
	"math"
)

func Encode(w io.Writer, image image.Image) error {
	// the pbi format is undocumented.
	// based on Pebble's bitmapgen, there are three parts to a pbi file:
	// 1. the header (12 bytes)
	// 2. the image data
	// 3. the palette
	header, err := pbiHeaderForImage(image)
	if err != nil {
		return err
	}
	if err := writeHeader(w, header); err != nil {
		return err
	}
	writePalettisedData(w, image)
	writePalette(w, image)
	return nil
}

type pbiHeader struct {
	RowSizeBytes uint16
	Flags        uint16
	X            int16
	Y            int16
	Width        int16
	Height       int16
}

func writeHeader(w io.Writer, header *pbiHeader) error {
	if err := binary.Write(w, binary.LittleEndian, header); err != nil {
		return err
	}
	return nil
}

func writePalettisedData(w io.Writer, img image.Image) error {
	pimg, ok := img.(*image.Paletted)
	if !ok {
		return fmt.Errorf("image is not paletted")
	}
	bpp, _ := bitsPerPixel(img)
	pixels_per_byte := 8 / bpp
	for y := 0; y < pimg.Rect.Dy(); y++ {
		packed_count := 0
		packed_value := uint8(0)
		for x := 0; x < pimg.Rect.Dx(); x++ {
			idx := pimg.Pix[(y-pimg.Rect.Min.Y)*pimg.Stride+(x-pimg.Rect.Min.X)]
			packed_count++
			packed_value |= idx << ((pixels_per_byte - packed_count) * bpp)
			if packed_count == pixels_per_byte {
				if _, err := w.Write([]byte{packed_value}); err != nil {
					return err
				}
				packed_value = 0
				packed_count = 0
			}
		}
		if packed_count > 0 {
			if _, err := w.Write([]byte{packed_value}); err != nil {
				return err
			}
		}
	}
	return nil
}

func writePalette(w io.Writer, img image.Image) error {
	pimg, ok := img.(*image.Paletted)
	if !ok {
		return fmt.Errorf("image is not paletted")
	}
	actualPaletteSize := int(math.Pow(2, float64(bitLen(uint(len(pimg.Palette))))))
	for i := range actualPaletteSize {
		c := color.Color(color.RGBA{0, 0, 0, 0})
		if i < len(pimg.Palette) {
			c = pimg.Palette[i]
		}
		r, g, b, a := c.RGBA()
		cb := uint8(((a / 0x5555) << 6) | ((r / 0x5555) << 4) | ((g / 0x5555) << 2) | (b / 0x5555))
		if _, err := w.Write([]byte{cb}); err != nil {
			return err
		}
	}
	return nil
}

func pbiHeaderForImage(img image.Image) (*pbiHeader, error) {
	flags, err := flagsForImage(img)
	if err != nil {
		return nil, err
	}
	rowSize, err := bytesPerRow(img)
	if err != nil {
		return nil, err
	}
	return &pbiHeader{
		RowSizeBytes: rowSize,
		Flags:        flags,
		X:            0,
		Y:            0,
		Width:        int16(img.Bounds().Dx()),
		Height:       int16(img.Bounds().Dy()),
	}, nil
}

func bytesPerRow(img image.Image) (uint16, error) {
	bimg, ok := img.(*image.Paletted)
	if !ok {
		return 0, fmt.Errorf("image is not paletted")
	}
	bpp, err := bitsPerPixel(img)
	if err != nil {
		return 0, err
	}
	sparePixels := (bimg.Rect.Dx() * bpp) % 8
	rowSize := (bimg.Rect.Dx() * bpp) / 8
	if sparePixels != 0 {
		rowSize += 1
	}
	return uint16(rowSize), nil
}

func bitsPerPixel(img image.Image) (int, error) {
	pimg, ok := img.(*image.Paletted)
	if !ok {
		return 0, fmt.Errorf("image is not paletted")
	}
	log.Printf("bits per pixel: %d", bitLen(uint(len(pimg.Palette))))
	return bitLen(uint(len(pimg.Palette))), nil
}

func flagsForImage(img image.Image) (uint16, error) {
	bitmapDict := map[int]uint16{
		1: 2,
		2: 3,
		4: 4,
	}
	pimg, ok := img.(*image.Paletted)
	if !ok {
		return 0, fmt.Errorf("image is not paletted")
	}
	formatValue, ok := bitmapDict[bitLen(uint(len(pimg.Palette)))]
	if !ok {
		return 0, fmt.Errorf("image has illegal number of palette entries: %d", len(pimg.Palette))
	}
	return 1<<12 | (formatValue << 1), nil
}

func bitLen(x uint) int {
	switch {
	case x == 0:
		return 0
	case x <= 2:
		return 1
	case x <= 4:
		return 2
	case x <= 8:
		return 3
	}
	return -1
}
