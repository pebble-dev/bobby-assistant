package util

type POI struct {
	Name               string
	Address            string
	Categories         []string
	OpeningHours       []string
	CurrentlyOpen      bool
	PhoneNumber        string
	PriceLevel         string
	StarRating         float64
	RatingCount        int64
	DistanceKilometers float64 `json:"DistanceKilometers,omitempty"`
	DistanceMiles      float64 `json:"DistanceMiles,omitempty"`
	Coordinates        Coords  `json:"Coordinates,omitempty"`
}
