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

package functions

import (
	"context"
	"encoding/json"
	"fmt"
	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
	"log"
	"reflect"
	"strings"
	"time"

	"golang.org/x/exp/slices"
	"google.golang.org/genai"
	"nhooyr.io/websocket"
)

type ToolFunction func(context.Context, *quota.Tracker, interface{}) interface{}
type CallbackFunction func(context.Context, *quota.Tracker, interface{}, chan<- map[string]interface{}, <-chan map[string]interface{}) interface{}
type ThoughtFunction func(interface{}) string

const MaxResponseSize = 20000

type Registration struct {
	// The function definition. The Parameters field will be filled out automatically and can be omitted.
	Definition genai.FunctionDeclaration
	// Aliases for the function, in case the model gets the name of the function wrong.
	Aliases []string
	// The function to call, for a simple function
	Fn ToolFunction
	// The callback to call, for an action that round trips back to the watch.
	Cb CallbackFunction
	// A function that summarises the provided input
	Thought ThoughtFunction
	// An instance of the object used to hold the function's parameters. This is what will be passed to
	// either Fn or Cb, and it will also be processed to pass to the model - including the comments.
	InputType interface{}
	// A capability the device must report for this function to be provided.
	Capability string
	// A capability the device must *not* report for this function to be provided.
	AntiCapability string
}

type Error struct {
	Error string `json:"error"`
}

var functionMap = make(map[string]Registration)
var functionAliases = make(map[string]string)

// registerFunction registers a function for later use by GetFunctionDefinitions and CallFunction.
// Functions implementations are generally expected to call registerFunction during init to register
// themselves.
func registerFunction(reg Registration) {
	if !strings.Contains("ABCDEFGHIJKLMNOPQRSTUVWXYZ", reflect.TypeOf(reg.InputType).Name()[0:1]) {
		panic(fmt.Sprintf("input type %T must be exported.", reg.InputType))
	}
	functionMap[reg.Definition.Name] = reg
	for _, alias := range reg.Aliases {
		functionAliases[alias] = reg.Definition.Name
	}
}

func IsAction(fn string) bool {
	if realFunction, ok := functionAliases[fn]; ok {
		fn = realFunction
	}
	if _, ok := functionMap[fn]; !ok {
		return false
	}
	return functionMap[fn].Cb != nil
}

// CallFunction calls a function by name with the given arguments. The arguments are expected to be
// a string containing a JSON object (presumably from GPT). The result is returned as a JSON string.
func CallFunction(ctx context.Context, qt *quota.Tracker, fn, args string) (string, error) {
	if realFunction, ok := functionAliases[fn]; ok {
		log.Printf("Model asked for function %q, which is an alias for %q.\n", fn, realFunction)
		fn = realFunction
	}
	if _, ok := functionMap[fn]; !ok || functionMap[fn].Fn == nil {
		return "", fmt.Errorf("function %q not found", fn)
	}
	var result interface{}
	in := reflect.New(reflect.TypeOf(functionMap[fn].InputType)).Interface()
	if err := json.Unmarshal([]byte(FixupBrokenJson(args)), in); err != nil {
		result = Error{"Invalid JSON: " + err.Error()}
	} else {
		result = functionMap[fn].Fn(ctx, qt, in)
	}
	r, err := json.Marshal(result)
	if err != nil {
		return "", fmt.Errorf("unable to marshal response: %v", err)
	}
	//if len(r) > MaxResponseSize {
	//	r = r[:MaxResponseSize]
	//}
	return string(r), nil
}

func CallAction(ctx context.Context, qt *quota.Tracker, fn, args string, ws *websocket.Conn) (string, error) {
	if realFunction, ok := functionAliases[fn]; ok {
		log.Printf("Model asked for action %q, which is an alias for %q.\n", fn, realFunction)
		fn = realFunction
	}
	if _, ok := functionMap[fn]; !ok || functionMap[fn].Cb == nil {
		return "", fmt.Errorf("function %q not found", fn)
	}
	a := reflect.New(reflect.TypeOf(functionMap[fn].InputType)).Interface()
	var result interface{}
	if err := json.Unmarshal([]byte(FixupBrokenJson(args)), &a); err != nil {
		result = Error{"Invalid JSON: " + err.Error()}
	} else {
		reqChan := make(chan map[string]interface{})
		respChan := make(chan map[string]interface{})
		defer close(reqChan)
		defer close(respChan)
		ctxTimeout, cancel := context.WithTimeout(ctx, 10*time.Second)
		go func() {
			defer cancel()
			for req := range reqChan {
				s, err := json.Marshal(req)
				if err != nil {
					log.Printf("unable to marshal request: %v", err)
					respChan <- map[string]interface{}{
						"status": "error",
						"error":  "unable to marshal request: " + err.Error(),
					}
					continue
				}
				log.Println("Sending request to watch...")
				if err := ws.Write(ctxTimeout, websocket.MessageText, append([]byte("a"), s...)); err != nil {
					log.Printf("unable to write request: %v", err)
					respChan <- map[string]interface{}{
						"status": "error",
						"error":  "unable to write request: " + err.Error(),
					}
					continue
				}
				log.Println("Reading response from watch...")
				messageType, respBytes, err := ws.Read(ctxTimeout)
				log.Printf("response read: %v", string(respBytes))
				if err != nil {
					log.Printf("unable to read response: %v", err)
					respChan <- map[string]interface{}{
						"status": "error",
						"error":  "unable to read response: " + err.Error(),
					}
					continue
				}
				if messageType != websocket.MessageText {
					log.Printf("unexpected message type: %v", messageType)
					respChan <- map[string]interface{}{
						"status": "error",
						"error":  "unable to read response: " + err.Error(),
					}
					continue
				}
				var resp map[string]interface{}
				if err := json.Unmarshal(respBytes, &resp); err != nil {
					log.Printf("unable to unmarshal response: %v", err)
					respChan <- map[string]interface{}{
						"status": "error",
						"error":  "unable to unmarshal response: " + err.Error(),
					}
					continue
				}
				respChan <- resp
			}
		}()
		result = functionMap[fn].Cb(ctx, qt, a, reqChan, respChan)
	}
	r, err := json.Marshal(result)
	if err != nil {
		return "", fmt.Errorf("unable to marshal response: %v", err)
	}
	//if len(r) > MaxResponseSize {
	//	r = r[:MaxResponseSize]
	//}
	return string(r), nil
}

func SummariseFunction(fn, args string) string {
	if realFunction, ok := functionAliases[fn]; ok {
		fn = realFunction
	}
	if _, ok := functionMap[fn]; !ok {
		return "Bobby is slightly lost"
	}
	a := reflect.New(reflect.TypeOf(functionMap[fn].InputType)).Interface()
	if err := json.Unmarshal([]byte(FixupBrokenJson(args)), &a); err != nil {
		return "Bobby is doing the wrong thing"
	} else {
		return functionMap[fn].Thought(a)
	}
}

func GetFunctionDefinitionsByCapability() map[string][]genai.FunctionDeclaration {
	definitions := map[string][]genai.FunctionDeclaration{}
	for _, reg := range functionMap {
		if _, ok := definitions[reg.Capability]; !ok {
			definitions[reg.Capability] = []genai.FunctionDeclaration{}
		}
		definitions[reg.Capability] = append(definitions[reg.Capability], reg.Definition)
	}
	return definitions
}

func GetFunctionDefinitionsForCapabilities(capabilities []string) []*genai.FunctionDeclaration {
	var definitions []*genai.FunctionDeclaration
	for _, reg := range functionMap {
		if (reg.Capability == "" || slices.Contains(capabilities, reg.Capability)) &&
			(reg.AntiCapability == "" || !slices.Contains(capabilities, reg.AntiCapability)) {
			d := reg.Definition
			definitions = append(definitions, &d)
		}
	}
	return definitions
}
