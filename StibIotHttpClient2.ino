/*
    Small device to retrieve the passing time of the STIB-MIVB public transport at 2 given stops
    By pushing the one or the other button, fetch the passing time and display on the LCD screen.

    Components:
    - NodeMcu
    - LCD-I2C
    - 2 push buttons
    - 2 10K Ohm resistors

    Librairies:
    - ArduinoJson: https://arduinojson.org/
    - ArduinoSort: https://github.com/emilv/ArduinoSort
    - LiquidCrystal I2C: https://github.com/johnrickman/LiquidCrystal_I2C

    Resources:
    - Configure NodeMcu: https://www.youtube.com/watch?v=p06NNRq5NTU
*/
#include "PassingTime.h"
//Configure STASSID, STAPSK and API_TOKEN
#include "config.h"
const char* ssid = STASSID;
const char* password = STAPSK;
const char* token = API_TOKEN;

#include <ESP8266WiFi.h>

#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include "ArduinoJson.h"
#include <ArduinoSort.h>


#include <Wire.h>  // This library is already built in to the Arduino IDE
#include <LiquidCrystal_I2C.h> //This library you can add via Include Library > Manage Library > 

const String APP_NAME = "Stib IOT - " __FILE__;
const String APP_VERSION = "v0.1-" __DATE__ " " __TIME__;

LiquidCrystal_I2C lcd(0x27, 16, 2);

const String host = "https://opendata-api.stib-mivb.be";
const int httpsPort = 443;
const String endPointToken = "/token";
const String endPointPassingTime = host + "/OperationMonitoring/3.0/PassingTimeByPoint";
// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char fingerprint[] PROGMEM = "5f 88 a4 69 77 f0 69 00 5d 6f 71 19 3d e2 2e 20 44 48 f0 b3";

/** Stops ids. Can be found in the GTFS*/
const String ARSENAL_TRAM_ROGIER = "5267";
const String ARSENAL_34_AUDERGHEM = "1715";

DynamicJsonBuffer jsonBuffer(512);
PassingTime *passingTimes[10];
const char * headerKeys[] = {"date"};
const size_t numberOfHeaders = 1;

#define BLUE_BUTTON D5
#define RED_BUTTON D4

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(APP_NAME);
  Serial.println(APP_VERSION);
  Serial.print("connecting to ");
  Serial.println(ssid);
  pinMode(BLUE_BUTTON, INPUT);
  pinMode(RED_BUTTON, INPUT); 
  
  lcd.init();   // initializing the LCD
  lcd.backlight(); // Enable or Turn On the backlight 

  if(!connectToWifi()){
    Serial.println("Fail to connect. Reset micro controller.");
    setup();
  }
}

bool connectToWifi(){
  lcd.print("Connecting WiFi ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lcd.setCursor(0,1);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
    Serial.print(".");
    count++;
    if(count>10){
      return false;
    }
  }
  lcd.clear();
  Serial.println("");
  lcd.print("WiFi connected");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP()); 
  return true;
}

void displayPassingTimeOnLcd(PassingTimeResponse* passingTimeResponse){
  lcd.clear();
  lcd.print(passingTimeResponse->passingTimes[0]->getDisplay());
  lcd.setCursor(0,1);
  lcd.print(passingTimeResponse->passingTimes[1]->getDisplay());
}

PassingTimeResponse* getPassingTime(String stopId){
  String url = endPointPassingTime + "/" + stopId;
  Serial.printf("[HTTPS] begin %s \n", url.c_str());
  
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setFingerprint(fingerprint);
  HTTPClient http;
  lcd.clear();
  lcd.print("Get passing time");
  if (http.begin(*client, url)) {  // HTTP
      http.addHeader("Accept", "application/json");
      http.addHeader("Authorization", token);
      Serial.print("[HTTPS] GET...\n");
      http.collectHeaders(headerKeys, numberOfHeaders);
      // start connection and send HTTP header
      int httpCode = http.GET();
      PassingTimeResponse * passingTimeResponse;
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          int j = 0;
          Serial.println(http.header(j));
          JsonObject& root = jsonBuffer.parseObject(payload);
          if(!root.success()){
            Serial.println("Fail to parse object");  
            return 0;
          }
          JsonArray& passingTimesArr = root["points"][0]["passingTimes"];
          if(!passingTimesArr.success()){
            Serial.println("Fail to parse passingTimes");  
            return 0;
          }
          passingTimeResponse = getPassingTimeResponse(passingTimesArr);
          
          Serial.println("Response:");
          for(int k =0; k<passingTimeResponse->numberOfResponses ; k++){
            Serial.println(passingTimeResponse->passingTimes[k]->getDisplay());
          }
          Serial.println("---------------");
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();
      return passingTimeResponse;
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
  return 0;
}

PassingTimeResponse* getPassingTimeResponse(JsonArray& passingTimesArr){
  int i = 0;
  static PassingTime *passingTimes[10];
  for (JsonObject& val : passingTimesArr){
    passingTimes[i] = new PassingTime(val["lineId"].as<String>(), val["destination"]["fr"].as<String>(), val["expectedArrivalTime"].as<String>());
    Serial.println(passingTimes[i]->getDisplay());
    i++;
  }
  sortArray(passingTimes, i, sortPassingTimes);
  
  return new PassingTimeResponse(passingTimes, i);
}


bool sortPassingTimes(PassingTime *a, PassingTime *b){
  return a->getRemainingTime() > b->getRemainingTime();
}
int redState = 0;
int blueState = 0;
String stopIdToFetch;

void loop() {
  redState=digitalRead(RED_BUTTON);
  blueState=digitalRead(BLUE_BUTTON);
  if(redState == HIGH || blueState == HIGH ){
    if(redState == HIGH ){
      Serial.println("Red was pushed");
      stopIdToFetch = ARSENAL_TRAM_ROGIER;
    } else {
      Serial.println("Blue was pushed");
      stopIdToFetch = ARSENAL_34_AUDERGHEM;
    }
    PassingTimeResponse* passingTimeResponse = getPassingTime(stopIdToFetch);
    if( passingTimeResponse != 0 ){
      displayPassingTimeOnLcd(passingTimeResponse); 
    }else{
      lcd.clear();
      lcd.print("Error");
    }
  }
  delay(200);
}
