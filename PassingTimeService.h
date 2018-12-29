#pragma once
#include "config.h"
#include "PassingTime.h"

//Librairies
#include "ArduinoJson.h"
#include <ArduinoSort.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

/** STIB-MIVB endpoint configuration */
const String host = "https://opendata-api.stib-mivb.be";
const int httpsPort = 443;
const String endPointToken = "/token";
const String endPointPassingTime = host + "/OperationMonitoring/3.0/PassingTimeByPoint";
// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char *fingerprint = "5f 88 a4 69 77 f0 69 00 5d 6f 71 19 3d e2 2e 20 44 48 f0 b3";

DynamicJsonBuffer jsonBuffer(512);

bool sortPassingTimes(PassingTime *a, PassingTime *b){
  return a->getRawExpectedTime() > b->getRawExpectedTime();
}

PassingTimeResponse* getPassingTimeResponse(JsonArray& passingTimesArr){
  int i = 0;
  static PassingTime *passingTimes[10];
  for (JsonObject& val : passingTimesArr){
    passingTimes[i] = new PassingTime(val[F("lineId")].as<String>(), val[F("destination")][F("fr")].as<String>(), val[F("expectedArrivalTime")].as<String>());
    i++;
  }
  sortArray(passingTimes, i, sortPassingTimes);
  
  return new PassingTimeResponse(passingTimes, i);
}

PassingTimeResponse* getPassingTime(String stopId){
  String url = endPointPassingTime + "/" + stopId;
  Serial.print(F("[HTTPS] begin: "));
  Serial.println(url);
  
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setFingerprint(fingerprint);
  HTTPClient http;
  if (http.begin(*client, url)) {  // HTTP
      http.addHeader(F("Accept"), F("application/json"));
      http.addHeader(F("Authorization"), API_TOKEN);
      Serial.println(F("[HTTPS] GET..."));
      // start connection and send HTTP header
      int httpCode = http.GET();
      PassingTimeResponse * passingTimeResponse;
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.print(F("[HTTPS] GET... code:"));
        Serial.println(httpCode);
        
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          JsonObject& root = jsonBuffer.parseObject(payload);
          if(!root.success()){
            Serial.println(F("Fail to parse object"));  
            return 0;
          }
          JsonArray& passingTimesArr = root[F("points")][0][F("passingTimes")];
          if(!passingTimesArr.success()){
            Serial.println(F("Fail to parse passingTimes"));  
            return 0;
          }
          passingTimeResponse = getPassingTimeResponse(passingTimesArr);
        }
      } else {
        Serial.print(F("[HTTPS] GET... failed, error: "));
        Serial.println(http.errorToString(httpCode));
      }

      http.end();
      return passingTimeResponse;
  } else {
    Serial.println(F("[HTTPS] Unable to connect"));
  }
  return 0;
}
