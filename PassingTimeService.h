#pragma once
#include "config.h"
#include "PassingTime.h"
#include "TokenService.h"

//Librairies
//ArduinoJson v5.13.4
#include "ArduinoJson.h"
#include <ArduinoSort.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

const String endPointPassingTime = HOST + "/OperationMonitoring/3.0/PassingTimeByPoint";

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

const size_t PASSING_TIME_RESPONSE_CAPACITY = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(4) + JSON_OBJECT_SIZE(1) + 5*JSON_OBJECT_SIZE(2) + 4*JSON_OBJECT_SIZE(3);

PassingTimeResponse* getPassingTime(HTTPClient * http, BearSSL::WiFiClientSecure * client, String stopId){
  http->setReuse(true);
  //client->setFingerprint(FINGERPRINT);
  client->setInsecure();
  String url = endPointPassingTime + "/" + stopId;
  Serial.print(F("[HTTPS] begin: "));
  Serial.println(url);
  
  if (http->begin(*client, url)) {  // HTTP
      http->addHeader(F("Accept"), F("application/json"));
      http->addHeader(F("Authorization"), String("Bearer ") + API_TOKEN);
      Serial.print(F("[HTTPS] GET... "));
      // start connection and send HTTP header
      int httpCode = http->GET();
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.print(F("code:"));
        Serial.println(httpCode);
        
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          DynamicJsonBuffer jsonBuffer(PASSING_TIME_RESPONSE_CAPACITY);
          JsonObject& root = jsonBuffer.parseObject(http->getString());
          if(!root.success()){
            Serial.println(F("Fail to parse object"));  
            return 0;
          }
          JsonArray& passingTimesArr = root[F("points")][0][F("passingTimes")];
          if(!passingTimesArr.success()){
            Serial.println(F("Fail to parse passingTimes"));  
            return 0;
          }
          http->end();
          PassingTimeResponse* response = getPassingTimeResponse(passingTimesArr);
          jsonBuffer.clear();
          return response;
        }else{
          http->end();
          return new PassingTimeResponse(httpCode);
        }
      } else {
        Serial.print(F("[HTTPS] GET... failed, error: "));
        Serial.println(http->errorToString(httpCode));
        http->end();
        return 0;
      }
  } else {
    Serial.println(F("[HTTPS] Unable to connect"));
  }
  return 0;
}
