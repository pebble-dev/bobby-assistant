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

type POIQuery struct {
	Location     string
	Query        string
	LanguageCode string
	Units        string
}

func (p *POIQuery) Equal(other *POIQuery) bool {
	if other == nil {
		return false
	}
	return p.Location == other.Location &&
		p.Query == other.Query &&
		p.LanguageCode == other.LanguageCode &&
		p.Units == other.Units
}
