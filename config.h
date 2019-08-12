#pragma once
#include "Favourite.h"

/**WiFi config*/
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PWD  "YOUR_PASSWORD"

/**Refresh rate (in sec) in the passing time screen */
#define REFRESH_RATE_SEC 15
/** STIB-MIVB endpoint configuration */
const String HOST = "https://opendata-api.stib-mivb.be";
/**Stib-Mivb Api Token*/
const String DEFAULT_API_TOKEN "Bearer YOUR_BEARER_TOKEN";
/*Convert << yourConsumerKey:yourConsumerSecret >> in Base64 */
const String API_BASIC_AUTH = "<< yourConsumerKey:yourConsumerSecret >>";
// DEPRECATED. client->setInsecure(); solve this issue
// Use web browser to view and copy
// SHA1 fingerprint of the certificate
// Go to https://www.grc.com/fingerprints.htm and enter https://opendata-api.stib-mivb.be/
const char *FINGERPRINT = "5f 88 a4 69 77 f0 69 00 5d 6f 71 19 3d e2 2e 20 44 48 f0 b3";

/** Stops ids. Can be found in the GTFS (stops.txt)*/
const Favourite favourites[5] = {
  Favourite("5311", "Tram > Buyl"),
  Favourite("1715", "34 > Auderghem"),
  Favourite("8211", "Metro > Centre"),  
  Favourite("5267", "Tram > Rogier"),
  Favourite("8212", "Metro > Auderg")
};