#pragma once
#include "Favourite.h"

/**WiFi config*/
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PWD  "YOUR_PASSWORD"

/**Stib-Mivb Api Token*/
#define API_TOKEN "Bearer YOUR_BEARER_TOKEN";
/**Refresh rate (in sec) in the passing time screen */
#define REFRESH_RATE_SEC 15

/** Stops ids. Can be found in the GTFS (stops.txt)*/
const Favourite favourites[5] = {
  Favourite("5311", "Tram > Buyl"),
  Favourite("1715", "34 > Auderghem"),
  Favourite("8211", "Metro > Centre"),  
  Favourite("5267", "Tram > Rogier"),
  Favourite("8212", "Metro > Auderg")
};