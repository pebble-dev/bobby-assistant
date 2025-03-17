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
	"fmt"
	"log"

	"github.com/honeycombio/beeline-go"
	"google.golang.org/genai"

	"github.com/pebble-dev/bobby-assistant/service/assistant/quota"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/currencies"
)

type CurrencyConversionRequest struct {
	Amount float64
	From   string
	To     string
}

type CurrencyConversionResponse struct {
	Amount   string // we return a nicely formatted string for the LLM's benefit
	Currency string
}

func init() {
	registerFunction(Registration{
		Definition: genai.FunctionDeclaration{
			Name:        "convert_currency",
			Description: "Convert an amount of one (real, non-crypto) currency to another. *Always* call this function to get exchange rates when doing currency conversion - never use memorised rates.",
			Parameters: &genai.Schema{
				Type:     genai.TypeObject,
				Nullable: false,
				Properties: map[string]*genai.Schema{
					"amount": {
						Type:        genai.TypeNumber,
						Format:      "double",
						Description: "The amount of currency to convert.",
						Nullable:    false,
					},
					"from": {
						Type:        genai.TypeString,
						Description: "The currency code to convert from.",
						Nullable:    false,
					},
					"to": {
						Type:        genai.TypeString,
						Description: "The currency code to convert to.",
					},
				},
				Required: []string{"amount", "from", "to"},
			},
		},
		Fn:        convertCurrency,
		Thought:   convertCurrencyThought,
		InputType: CurrencyConversionRequest{},
	})
}

func convertCurrency(ctx context.Context, qt *quota.Tracker, input interface{}) interface{} {
	ctx, span := beeline.StartSpan(ctx, "convert_currency")
	defer span.Send()
	ccr := input.(*CurrencyConversionRequest)

	if !currencies.IsValidCurrency(ccr.From) {
		return Error{Error: "Unknown currency code " + ccr.From}
	}
	if !currencies.IsValidCurrency(ccr.To) {
		return Error{Error: "Unknown currency code " + ccr.To}
	}

	cdm := currencies.GetCurrencyDataManager()

	data, err := cdm.GetExchangeData(ctx, ccr.From)
	if err != nil {
		log.Printf("error getting currency data for %s/%s: %v", ccr.From, ccr.To, err)
		return Error{Error: err.Error()}
	}
	if data == nil {
		return Error{Error: "returned currency data is nil!?"}
	}

	rate, ok := data.ConversionRates[ccr.To]
	if !ok {
		return Error{Error: fmt.Sprintf("No currency conversion available from %s to %s", ccr.From, ccr.To)}
	}

	result := rate * ccr.Amount
	return &CurrencyConversionResponse{
		Amount:   fmt.Sprintf("%.2f", result),
		Currency: ccr.To,
	}
}

func convertCurrencyThought(i interface{}) string {
	args := i.(*CurrencyConversionRequest)
	return fmt.Sprintf("Checking the %s/%s rate...", args.From, args.To)
}
