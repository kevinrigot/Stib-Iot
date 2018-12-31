#pragma once
#include "config.h"
#include <EEPROM.h>
//Librairies
//ArduinoJson v5.13.4
#include "ArduinoJson.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#define IS_TOKEN_INITIALIZED_ADDR 0
#define TOKEN_START_ADDR 1
#define TOKEN_LENGTH 32

/*Token for the API*/
String API_TOKEN;
const String endPointToken = HOST + "/token";

String readToken(){
    // If Api token has been stored
    if((char)EEPROM.read(IS_TOKEN_INITIALIZED_ADDR) == 'Y'){
      Serial.println("Token exists in EEPROM, reading it...");
      char token[TOKEN_LENGTH];
      EEPROM.get(TOKEN_START_ADDR, token);
      Serial.println("Token read from EEPROM: " + String(token));
      return String(token);
    } else {
      Serial.println("Use default api token: " + DEFAULT_API_TOKEN);
      return DEFAULT_API_TOKEN;
    }
}

void initializeToken(){
    API_TOKEN = readToken();
}

void initializeEeprom(){
    EEPROM.begin(512); 
    delay(10);    
}

void resetTokenFromEeprom(){
  EEPROM.put(IS_TOKEN_INITIALIZED_ADDR, 'N');
  EEPROM.commit();  
}

void writeToken(String token){
  EEPROM.begin(512); delay(10);
  Serial.println("Writing token <" + token + "> to EEPROM");
  const char * tokenChar = token.c_str();
  for(int i = 0; i<TOKEN_LENGTH; i++){
    EEPROM.write(TOKEN_START_ADDR+i, tokenChar[i]);  
  }  
  EEPROM.put(IS_TOKEN_INITIALIZED_ADDR, 'Y');
  EEPROM.commit();  
  Serial.println("Token written to EEPROM");
}

const size_t TOKEN_RESPONSE_CAPACITY = JSON_OBJECT_SIZE(4);
String getNewToken(HTTPClient * http, BearSSL::WiFiClientSecure * client){
  Serial.println(F("Retrieve new OAuth2 API token"));
  client->setFingerprint(FINGERPRINT);
  Serial.print(F("[HTTPS] begin: "));
  Serial.println(endPointToken);
  if (http->begin(*client, endPointToken)) {  // HTTP
    http->addHeader(F("Accept"), F("application/json"));
    http->addHeader(F("Authorization"), String("Basic ") + API_BASIC_AUTH);
    Serial.print(F("[HTTPS] GET... "));
    // start connection and send HTTP header
    int httpCode = http->POST("grant_type=client_credentials");
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.print(F("code:"));
      Serial.println(httpCode);
      DynamicJsonBuffer jsonBuffer(TOKEN_RESPONSE_CAPACITY);
      JsonObject& root = jsonBuffer.parseObject(http->getString());
      if(!root.success()){
        Serial.println(F("Fail to parse token response"));  
        return "ERROR";
      }
      http->end();
      API_TOKEN = root["access_token"].as<String>();
      Serial.print(F("New token received:"));
      Serial.println(API_TOKEN);
      jsonBuffer.clear();
      return API_TOKEN;
    }else{
      Serial.print(F("[HTTPS] GET... failed, error: "));
      Serial.println(http->errorToString(httpCode));
      http->end();
    }
  } else {
    Serial.println(F("[HTTPS] Unable to connect"));
  }
  return "ERROR";
}
