#ifndef STASSID
#define STASSID "YOUR_SSID"
#define STAPSK  "YOUR_PASSWORD"
#endif

#define API_TOKEN "Bearer YOUR_BEARER_TOKEN";

/** Stops ids. Can be found in the GTFS (stops.txt)*/
const Favourite favourites[5] = {
  Favourite("5311", "Tram > Buyl"),
  Favourite("1715", "34 > Auderghem"),
  Favourite("8211", "Metro > Centre"),  
  Favourite("5267", "Tram > Rogier"),
  Favourite("8212", "Metro > Auderg")
};