package widgets

import (
	"context"
	"fmt"
	"log"
	"regexp"
)

var timerWidgetRegex = regexp.MustCompile(`<!TIMER targetTime=[\["]?(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{0,5})?(?:Z|[+-](?:\d{4}|\d\d:\d\d)))[]"!]? ?(?: name=[\["]?(.*?)[]"]?)?[!/]>`)
var weatherWidgetRegex = regexp.MustCompile(`<!WEATHER-(CURRENT|SINGLE-DAY|MULTI-DAY) location=[\["]?(.+?)[]"!]? units=[\["]?(imperial|metric|uk hybrid)[]"!]?(?: day=[\["]?(.+?)[]"]?)?[!/]>`)
var numberWidgetRegex = regexp.MustCompile(`<!NUMERIC-ANSWER number=[\["]?(.+?)[]"!]? ?(?: unit=[\["]?(.*?)[]"]?)?[!/]>`)

type Widget struct {
	Content any    `json:"content"`
	Type    string `json:"type"`
}

func ProcessWidget(ctx context.Context, widget string) (any, error) {
	weatherWidgets := weatherWidgetRegex.FindAllStringSubmatch(widget, -1)
	for _, weatherWidget := range weatherWidgets {
		switch weatherWidget[1] {
		case "CURRENT":
			widget, err := currentConditionsWeatherWidget(ctx, weatherWidget[2], weatherWidget[3])
			if err != nil {
				log.Printf("Error processing weather widget: %v", err)
				return nil, fmt.Errorf("error processing weather widget: %w", err)
			}
			return Widget{Content: widget, Type: "weather-current"}, nil
		case "SINGLE-DAY":
			widget, err := singleDayWeatherWidget(ctx, weatherWidget[2], weatherWidget[3], weatherWidget[4])
			if err != nil {
				log.Printf("Error processing weather widget: %v", err)
				return nil, fmt.Errorf("error processing weather widget: %w", err)
			}
			return Widget{Content: widget, Type: "weather-single-day"}, nil
		case "MULTI-DAY":
			widget, err := multiDayWeatherWidget(ctx, weatherWidget[2], weatherWidget[3])
			if err != nil {
				log.Printf("Error processing weather widget: %v", err)
				return nil, fmt.Errorf("error processing weather widget: %w", err)
			}
			return Widget{Content: widget, Type: "weather-multi-day"}, nil
		default:
			log.Printf("Unknown weather widget %q", weatherWidget[1])
		}
	}
	timerWidgets := timerWidgetRegex.FindAllStringSubmatch(widget, -1)
	for _, w := range timerWidgets {
		widget, err := timerWidget(ctx, w[1], w[2])
		if err != nil {
			log.Printf("Error processing timer widget: %v", err)
			return nil, fmt.Errorf("error processing timer widget: %w", err)
		}
		return Widget{Content: widget, Type: "timer"}, nil
	}
	numberWidgets := numberWidgetRegex.FindAllStringSubmatch(widget, -1)
	for _, w := range numberWidgets {
		widget, err := numberWidget(ctx, w[1], w[2])
		if err != nil {
			log.Printf("Error processing number widget: %v", err)
			return nil, fmt.Errorf("error processing number widget: %w", err)
		}
		return Widget{Content: widget, Type: "number"}, nil
	}
	return nil, fmt.Errorf("unknown widget %q", widget)
}
