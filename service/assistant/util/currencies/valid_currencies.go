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

package currencies

func IsValidCurrency(currency string) bool {
	_, ok := validCurrencies[currency]
	return ok
}

// A hardcoded list of currencies we can look up, so we don't waste requests looking up nonexistent currencies.
var validCurrencies = map[string]bool{
	"AED": true, // UAE Dirham
	"AFN": true, // Afghan Afghani
	"ALL": true, // Albanian Lek
	"AMD": true, // Armenian Dram
	"ANG": true, // Netherlands Antillian Guilder
	"AOA": true, // Angolan Kwanza
	"ARS": true, // Argentine Peso
	"AUD": true, // Australian Dollar
	"AWG": true, // Aruban Florin
	"AZN": true, // Azerbaijani Manat
	"BAM": true, // Bosnia and Herzegovina Convertible Mark
	"BBD": true, // Barbados Dollar
	"BDT": true, // Bangladeshi Taka
	"BGN": true, // Bulgarian Lev
	"BHD": true, // Bahraini Dinar
	"BIF": true, // Burundian Franc
	"BMD": true, // Bermudian Dollar
	"BND": true, // Brunei Dollar
	"BOB": true, // Bolivian Boliviano
	"BRL": true, // Brazilian Real
	"BSD": true, // Bahamian Dollar
	"BTN": true, // Bhutanese Ngultrum
	"BWP": true, // Botswana Pula
	"BYN": true, // Belarusian Ruble
	"BZD": true, // Belize Dollar
	"CAD": true, // Canadian Dollar
	"CDF": true, // Congolese Franc
	"CHF": true, // Swiss Franc
	"CLP": true, // Chilean Peso
	"CNY": true, // Chinese Renminbi
	"COP": true, // Colombian Peso
	"CRC": true, // Costa Rican Colon
	"CUP": true, // Cuban Peso
	"CVE": true, // Cape Verdean Escudo
	"CZK": true, // Czech Koruna
	"DJF": true, // Djiboutian Franc
	"DKK": true, // Danish Krone
	"DOP": true, // Dominican Peso
	"DZD": true, // Algerian Dinar
	"EGP": true, // Egyptian Pound
	"ERN": true, // Eritrean Nakfa
	"ETB": true, // Ethiopian Birr
	"EUR": true, // Euro
	"FJD": true, // Fiji Dollar
	"FKP": true, // Falkland Islands Pound
	"FOK": true, // Faroese Króna
	"GBP": true, // Pound Sterling
	"GEL": true, // Georgian Lari
	"GGP": true, // Guernsey Pound
	"GHS": true, // Ghanaian Cedi
	"GIP": true, // Gibraltar Pound
	"GMD": true, // Gambian Dalasi
	"GNF": true, // Guinean Franc
	"GTQ": true, // Guatemalan Quetzal
	"GYD": true, // Guyanese Dollar
	"HKD": true, // Hong Kong Dollar
	"HNL": true, // Honduran Lempira
	"HRK": true, // Croatian Kuna
	"HTG": true, // Haitian Gourde
	"HUF": true, // Hungarian Forint
	"IDR": true, // Indonesian Rupiah
	"ILS": true, // Israeli New Shekel
	"IMP": true, // Manx Pound
	"INR": true, // Indian Rupee
	"IQD": true, // Iraqi Dinar
	"IRR": true, // Iranian Rial
	"ISK": true, // Icelandic Króna
	"JEP": true, // Jersey Pound
	"JMD": true, // Jamaican Dollar
	"JOD": true, // Jordanian Dinar
	"JPY": true, // Japanese Yen
	"KES": true, // Kenyan Shilling
	"KGS": true, // Kyrgyzstani Som
	"KHR": true, // Cambodian Riel
	"KID": true, // Kiribati Dollar
	"KMF": true, // Comorian Franc
	"KRW": true, // South Korean Won
	"KWD": true, // Kuwaiti Dinar
	"KYD": true, // Cayman Islands Dollar
	"KZT": true, // Kazakhstani Tenge
	"LAK": true, // Lao Kip
	"LBP": true, // Lebanese Pound
	"LKR": true, // Sri Lanka Rupee
	"LRD": true, // Liberian Dollar
	"LSL": true, // Lesotho Loti
	"LYD": true, // Libyan Dinar
	"MAD": true, // Moroccan Dirham
	"MDL": true, // Moldovan Leu
	"MGA": true, // Malagasy Ariary
	"MKD": true, // Macedonian Denar
	"MMK": true, // Burmese Kyat
	"MNT": true, // Mongolian Tögrög
	"MOP": true, // Macanese Pataca
	"MRU": true, // Mauritanian Ouguiya
	"MUR": true, // Mauritian Rupee
	"MVR": true, // Maldivian Rufiyaa
	"MWK": true, // Malawian Kwacha
	"MXN": true, // Mexican Peso
	"MYR": true, // Malaysian Ringgit
	"MZN": true, // Mozambican Metical
	"NAD": true, // Namibian Dollar
	"NGN": true, // Nigerian Naira
	"NIO": true, // Nicaraguan Córdoba
	"NOK": true, // Norwegian Krone
	"NPR": true, // Nepalese Rupee
	"NZD": true, // New Zealand Dollar
	"OMR": true, // Omani Rial
	"PAB": true, // Panamanian Balboa
	"PEN": true, // Peruvian Sol
	"PGK": true, // Papua New Guinean Kina
	"PHP": true, // Philippine Peso
	"PKR": true, // Pakistani Rupee
	"PLN": true, // Polish Złoty
	"PYG": true, // Paraguayan Guaraní
	"QAR": true, // Qatari Riyal
	"RON": true, // Romanian Leu
	"RSD": true, // Serbian Dinar
	"RUB": true, // Russian Ruble
	"RWF": true, // Rwandan Franc
	"SAR": true, // Saudi Riyal
	"SBD": true, // Solomon Islands Dollar
	"SCR": true, // Seychellois Rupee
	"SDG": true, // Sudanese Pound
	"SEK": true, // Swedish Krona
	"SGD": true, // Singapore Dollar
	"SHP": true, // Saint Helena Pound
	"SLE": true, // Sierra Leonean Leone
	"SLL": true, // Sierra Leonean Leone
	"SOS": true, // Somali Shilling
	"SRD": true, // Surinamese Dollar
	"SSP": true, // South Sudanese Pound
	"STN": true, // São Tomé and Príncipe Dobra
	"SYP": true, // Syrian Pound
	"SZL": true, // Eswatini Lilangeni
	"THB": true, // Thai Baht
	"TJS": true, // Tajikistani Somoni
	"TMT": true, // Turkmenistan Manat
	"TND": true, // Tunisian Dinar
	"TOP": true, // Tongan Paʻanga
	"TRY": true, // Turkish Lira
	"TTD": true, // Trinidad and Tobago Dollar
	"TVD": true, // Tuvaluan Dollar
	"TWD": true, // New Taiwan Dollar
	"TZS": true, // Tanzanian Shilling
	"UAH": true, // Ukrainian Hryvnia
	"UGX": true, // Ugandan Shilling
	"USD": true, // United States Dollar
	"UYU": true, // Uruguayan Peso
	"UZS": true, // Uzbekistani So'm
	"VES": true, // Venezuelan Bolívar Soberano
	"VND": true, // Vietnamese Đồng
	"VUV": true, // Vanuatu Vatu
	"WST": true, // Samoan Tālā
	"XAF": true, // Central African CFA Franc
	"XCD": true, // East Caribbean Dollar
	"XDR": true, // Special Drawing Rights
	"XOF": true, // West African CFA franc
	"XPF": true, // CFP Franc
	"YER": true, // Yemeni Rial
	"ZAR": true, // South African Rand
	"ZMW": true, // Zambian Kwacha
	"ZWL": true, // Zimbabwean Dollar
}
