package widgets

import (
	"context"
	"fmt"
	"log"
	"regexp"
)

var weatherWidgetRegex = regexp.MustCompile(`<!WEATHER-(CURRENT|SINGLE-DAY|MULTI-DAY) location=[\["]?(.+?)[]"!]? units=[\["]?(imperial|metric|uk hybrid)[]"]?(?: day=[\["]?(.+?)[]"]?)?!>`)

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
			log.Printf("widget: +%v\n", widget)
			return Widget{Content: widget, Type: "weather-current"}, nil
		case "SINGLE-DAY":
			widget, err := singleDayWeatherWidget(ctx, weatherWidget[2], weatherWidget[3], weatherWidget[4])
			if err != nil {
				log.Printf("Error processing weather widget: %v", err)
				return nil, fmt.Errorf("error processing weather widget: %w", err)
			}
			log.Printf("widget: +%v\n", widget)
			return Widget{Content: widget, Type: "weather-single-day"}, nil
		case "MULTI-DAY":
			// multiDayWeatherWidget(ctx, location, units)
		default:
			log.Printf("Unknown weather widget %q", weatherWidget[1])
		}
	}
	return nil, fmt.Errorf("unknown widget %q", widget)
}
