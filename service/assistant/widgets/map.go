package widgets

import (
	"bytes"
	"context"
	_ "embed"
	"encoding/base64"
	"fmt"
	"github.com/pebble-dev/bobby-assistant/service/assistant/config"
	"github.com/pebble-dev/bobby-assistant/service/assistant/query"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util"
	"github.com/pebble-dev/bobby-assistant/service/assistant/util/pbi"
	gmaps "googlemaps.github.io/maps"
	"image"
	"image/color"
	"image/draw"
	"image/png"
	"log"
	"strconv"
	"strings"
)

//go:embed google2bit.png
var googleLogo2BitBytes []byte
var googleLogo2Bit image.Image

var mapClient *gmaps.Client

func init() {
	var err error
	c := config.GetConfig()
	mapClient, err = gmaps.NewClient(gmaps.WithAPIKeyAndSignature(c.GoogleMapsStaticKey, c.GoogleMapsStaticSecret))
	if err != nil {
		panic(err)
	}
	googleLogo2Bit, err = png.Decode(bytes.NewReader(googleLogo2BitBytes))
	if err != nil {
		panic(err)
	}
}

type MapWidget struct {
	Image         string `json:"image"`
	Height        int16  `json:"height"`
	Width         int16  `json:"width"`
	UserLocationX int16  `json:"user_location_x"`
	UserLocationY int16  `json:"user_location_y"`
}

func mapWidget(ctx context.Context, markerString, includeLocationString string) (*MapWidget, error) {
	log.Println("MAP WIDGET!")
	includeLocation := strings.EqualFold(includeLocationString, "true")
	markers := make(map[string]util.Coords)
	threadContext := query.ThreadContextFromContext(ctx)
	poiInfo := threadContext.ContextStorage.POIs
	markerString = strings.ReplaceAll(markerString, "\"", "")
	for _, marker := range strings.Split(markerString, ",") {
		parts := strings.Split(marker, ":")
		if len(parts) != 2 {
			continue
		}
		idxStr := parts[0]
		name := parts[1]
		idx, err := strconv.Atoi(idxStr)
		if err != nil {
			log.Printf("Error parsing index %s: %v", idxStr, err)
			continue
		}
		if idx < 0 || idx >= len(poiInfo) {
			log.Printf("Index %d out of range for POI results", idx)
			continue
		}
		poi := poiInfo[idx]
		markers[name] = poi.Coordinates
	}
	var userLocation *util.Coords
	if includeLocation {
		l := query.LocationFromContext(ctx)
		if l != nil {
			userLocation = &util.Coords{
				Latitude:  l.Lat,
				Longitude: l.Lon,
			}
		}
	}
	mapImage, err := generateMap(ctx, markers, userLocation)
	if err != nil {
		// Handle error
		return nil, err
	}
	userX, userY := findUserLocation(mapImage)
	if query.SupportsColourFromContext(ctx) {
		mapImage = lowColour(mapImage)
	} else {
		mapImage = monochrome(mapImage)
	}
	mapImageBase64, err := encodeImageToBase64(mapImage)
	return &MapWidget{
		Image:         mapImageBase64,
		Height:        int16(mapImage.Bounds().Dy()),
		Width:         int16(mapImage.Bounds().Dx()),
		UserLocationX: int16(userX),
		UserLocationY: int16(userY),
	}, nil
}

func generateMap(ctx context.Context, markers map[string]util.Coords, userLocation *util.Coords) (image.Image, error) {
	// For legal reasons, we *must* use Google Maps here - we're obliged to use it as long as we're using Google's
	// Places API.
	var mapMarkers []gmaps.Marker
	for name, coords := range markers {
		mapMarkers = append(mapMarkers, gmaps.Marker{
			Location: []gmaps.LatLng{{
				Lat: coords.Latitude,
				Lng: coords.Longitude,
			}},
			Label: name,
			Color: "0x555555",
			Size:  "mid",
		})
	}
	if userLocation != nil {
		mapMarkers = append(mapMarkers, gmaps.Marker{
			Location: []gmaps.LatLng{{
				Lat: userLocation.Latitude,
				Lng: userLocation.Longitude,
			}},
			CustomIcon: gmaps.CustomIcon{
				IconURL: "https://storage.googleapis.com/bobby-assets/user-location-pixel.png",
				Anchor:  "center",
			},
		})
	}

	screenWidth := query.ScreenWidthFromContext(ctx)
	if screenWidth == 0 {
		screenWidth = 144
	}
	request := gmaps.StaticMapRequest{
		Size:    fmt.Sprintf("%dx100", screenWidth),
		Format:  "png8",
		MapType: "roadmap",
		MapId:   config.GetConfig().GoogleMapsStaticMapId,
		Markers: mapMarkers,
	}
	return mapClient.StaticMap(ctx, &request)
}

func findUserLocation(img image.Image) (x, y int) {
	// Find the user location in the image
	bounds := img.Bounds()
	for y := 0; y < bounds.Max.Y; y++ {
		for x := 0; x < bounds.Max.X; x++ {
			c := img.At(x, y)
			r, g, b, a := c.RGBA()
			// This is the colour of the magic pixel we put on the map to mark the user's location.
			if r == 2827 && g == 5911 && b == 56540 && a == 65535 {
				return x, y
			}
		}
	}
	return 0, 0
}

func encodeImageToBase64(img image.Image) (string, error) {
	// Convert the image to a base64 string
	buf := bytes.Buffer{}
	err := pbi.Encode(&buf, img)
	if err != nil {
		return "", err
	}
	return base64.StdEncoding.EncodeToString(buf.Bytes()), nil
}

func monochrome(img image.Image) image.Image {
	p := color.Palette{color.White, color.Black}
	newImg := image.NewPaletted(img.Bounds(), p)
	for y := 0; y < img.Bounds().Max.Y; y++ {
		for x := 0; x < img.Bounds().Max.X; x++ {
			c := img.At(x, y)
			if shouldDither(c) {
				if x%2 != y%2 {
					newImg.Set(x, y, color.White)
				} else {
					newImg.Set(x, y, color.Black)
				}
			} else if closeToBlack(c) {
				newImg.Set(x, y, color.Black)
			} else {
				newImg.Set(x, y, color.White)
			}
		}
	}
	return newImg
}

func lowColour(img image.Image) image.Image {
	p := color.Palette{color.White, color.Black, color.Gray{0x55}, color.Gray{0xaa}}
	newImg := image.NewPaletted(img.Bounds(), p)
	for y := 0; y < img.Bounds().Max.Y; y++ {
		for x := 0; x < img.Bounds().Max.X; x++ {
			c := img.At(x, y)
			if shouldDither(c) {
				newImg.Set(x, y, color.Gray{0xaa})
			} else {
				newImg.Set(x, y, greyTo2BitGrey(c))
			}
		}
	}
	// slap the 2-bit version of the Google logo over the existing one
	topCorner := image.Pt(8, newImg.Bounds().Max.Y-16)
	targetImageRect := image.Rect(topCorner.X, topCorner.Y, topCorner.X+googleLogo2Bit.Bounds().Dx(), topCorner.Y+googleLogo2Bit.Bounds().Dy())
	draw.Draw(newImg, targetImageRect, googleLogo2Bit, image.Point{0, 0}, draw.Over)
	return newImg
}

func greyTo2BitGrey(c color.Color) color.Color {
	y := color.GrayModel.Convert(c).(color.Gray).Y
	switch {
	case y <= 0x2a:
		return color.Gray{0x00}
	case y <= 0x7f:
		return color.Gray{0x55}
	case y <= 0xd4:
		return color.Gray{0xaa}
	default:
		return color.Gray{0xff}
	}
}

func colourToMonochrome(c color.Color) color.Color {
	isBlack := color.Gray16Model.Convert(c).(color.Gray16).Y <= 0xEFFF
	if isBlack {
		return color.Black
	} else {
		return color.White
	}
}

func closeToWhite(c color.Color) bool {
	r, g, b, _ := c.RGBA()
	return r > 0x7FFF && g > 0x7FFF && b > 0x7FFF
}

func closeToBlack(c color.Color) bool {
	r, _, _, _ := c.RGBA()
	return r <= 0x8FFF
}

func shouldDither(c color.Color) bool {
	r, g, b, _ := c.RGBA()
	return r == 42148 && g == 43176 && b == 43690
}

func isGrey(c color.Color) bool {
	r, g, b, _ := c.RGBA()
	return r == g && g == b
}
