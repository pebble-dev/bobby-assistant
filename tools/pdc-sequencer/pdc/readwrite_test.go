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
	"encoding/hex"
	"fmt"
	"io"
	"os"
	"slices"
	"testing"
)

func TestRoundTripImage(t *testing.T) {
	f, err := os.Open("testdata/cog.pdci")
	if err != nil {
		t.Fatal("failed to open testdata/cog.pdci")
	}
	content, err := io.ReadAll(f)
	_ = f.Close()
	if err != nil {
		t.Fatalf("failed to read testdata/cog.pdci: %v", err)
	}
	r := bytes.NewReader(content)
	pdc, err := ParseImageFile(r)
	if err != nil {
		t.Fatalf("failed to parse image: %v", err)
	}
	if pdc == nil {
		t.Fatal("nil image")
	}
	fmt.Printf("Parsed image: %+v\n", pdc)
	w := new(bytes.Buffer)
	if err := WriteImageFile(w, pdc); err != nil {
		t.Fatalf("failed to write image: %v", err)
	}
	if !slices.Equal(content, w.Bytes()) {
		t.Fatalf("round trip failed: content does not match.\no: %s\nr: %s", hex.EncodeToString(content), hex.EncodeToString(w.Bytes()))
	}
}

func TestRoundTripSequence(t *testing.T) {
	f, err := os.Open("testdata/sent.pdcs")
	if err != nil {
		t.Fatal("failed to open testdata/sent.pdcs")
	}
	content, err := io.ReadAll(f)
	_ = f.Close()
	if err != nil {
		t.Fatalf("failed to read testdata/csent.pdcs: %v", err)
	}
	r := bytes.NewReader(content)
	pdc, err := ParseImageSequence(r)
	if err != nil {
		t.Fatalf("failed to parse image: %v", err)
	}
	if pdc == nil {
		t.Fatal("nil image")
	}
	fmt.Printf("Parsed image: %+v\n", pdc)
	w := new(bytes.Buffer)
	if err := WriteImageSequence(w, pdc); err != nil {
		t.Fatalf("failed to write image: %v", err)
	}
	if !slices.Equal(content, w.Bytes()) {
		t.Fatalf("round trip failed: content does not match.\no: %s\nr: %s", hex.EncodeToString(content), hex.EncodeToString(w.Bytes()))
	}
}
