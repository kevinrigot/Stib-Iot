/*
    Small device to retrieve the passing time of the STIB-MIVB public transport at given stops.
    These stops are hardcoded in the code as favourites. 
    The app consists in 2 screen. 
    - The selection of the line from the favourite list.
    - Displaying the time at station
    The user has 3 push buttons. 
    2 for going up/down. 1 for selecting a favourite/going back to favourites page

    Components:
    - NodeMcu
    - LCD-I2C
    - 3 push buttons
    - 3 10K Ohm resistors

    Librairies:
    - ArduinoJson: https://arduinojson.org/
    - ArduinoSort: https://github.com/emilv/ArduinoSort
    - LiquidCrystal I2C: https://github.com/johnrickman/LiquidCrystal_I2C

    Resources:
    - Configure NodeMcu: https://www.youtube.com/watch?v=p06NNRq5NTU

    TODO:
    - Regenerate token
    - Wake up on push
    - led indicator (loading)
    - Get remaining time instead of expected time arrival
    - Auto refresh every 15sec
    - 3D print case
*/
#define DEBUG true
#include "PassingTime.h"
#include "Favourite.h"
//Configure STASSID, STAPSK, API_TOKEN and favourites
#include "config.h"
const char* ssid = STASSID;
const char* password = STAPSK;
const char* token = API_TOKEN;

/** Librairies */
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include "ArduinoJson.h"
#include <ArduinoSort.h>
#include <Wire.h>  // This library is already built in to the Arduino IDE
#include <LiquidCrystal_I2C.h> //This library you can add via Include Library > Manage Library > 

LiquidCrystal_I2C lcd(0x27, 16, 2);

const String APP_NAME = "Stib IOT - " __FILE__;
const String APP_VERSION = "v0.1-" __DATE__ " " __TIME__;

/** STIB-MIVB endpoint configuration */
const String host = "https://opendata-api.stib-mivb.be";
const int httpsPort = 443;
const String endPointToken = "/token";
const String endPointPassingTime = host + "/OperationMonitoring/3.0/PassingTimeByPoint";
// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char fingerprint[] PROGMEM = "5f 88 a4 69 77 f0 69 00 5d 6f 71 19 3d e2 2e 20 44 48 f0 b3";

DynamicJsonBuffer jsonBuffer(100);
PassingTime *passingTimes[10];
const char * headerKeys[] = {"date"};
const size_t numberOfHeaders = 1;

enum UP_DOWN{
  UP,
  DOWN
};

#define UP_BUTTON D4
#define SELECT_BUTTON D5
#define DOWN_BUTTON D6

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(APP_NAME);
  Serial.println(APP_VERSION);
  if(DEBUG)Serial.println(F("Debug mode"));
  
  
  pinMode(UP_BUTTON, INPUT);
  pinMode(SELECT_BUTTON, INPUT); 
  pinMode(DOWN_BUTTON, INPUT); 
  
  lcd.init();   // initializing the LCD
  lcd.backlight(); // Enable or Turn On the backlight 

  if(!connectToWifi()){
    Serial.println("Fail to connect. Reset micro controller.");
    setup();
  }
}

bool connectToWifi(){
  Serial.print(F("Connecting to "));
  Serial.println(ssid);
  lcd.print(F("Connecting WiFi "));
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
  lcd.print(F("WiFi connected"));
  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP()); 
  return true;
}

PassingTimeResponse* getPassingTime(String stopId){
  String url = endPointPassingTime + "/" + stopId;
  Serial.print(F("[HTTPS] begin: "));
  Serial.println(url);
  
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setFingerprint(fingerprint);
  HTTPClient http;
  lcd.clear();
  lcd.print(F("Get passing time"));
  if (http.begin(*client, url)) {  // HTTP
      http.addHeader(F("Accept"), F("application/json"));
      http.addHeader(F("Authorization"), token);
      Serial.println(F("[HTTPS] GET..."));
      http.collectHeaders(headerKeys, numberOfHeaders);
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
          int j = 0;
          Serial.println(http.header(j));
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

          if(DEBUG){
            Serial.println(F("Response:"));
            for(int k =0; k<passingTimeResponse->numberOfResponses ; k++){
              Serial.println(passingTimeResponse->passingTimes[k]->getDisplay());
            }
            Serial.println(F("---------------"));
          }
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

PassingTimeResponse* getPassingTimeResponse(JsonArray& passingTimesArr){
  int i = 0;
  static PassingTime *passingTimes[10];
  for (JsonObject& val : passingTimesArr){
    passingTimes[i] = new PassingTime(val[F("lineId")].as<String>(), val[F("destination")][F("fr")].as<String>(), val[F("expectedArrivalTime")].as<String>());
    Serial.println(passingTimes[i]->getDisplay());
    i++;
  }
  sortArray(passingTimes, i, sortPassingTimes);
  
  return new PassingTimeResponse(passingTimes, i);
}


bool sortPassingTimes(PassingTime *a, PassingTime *b){
  return a->getRemainingTime() > b->getRemainingTime();
}
int upButtonState = 0;
int selectButtonState = 0;
int downButtonState = 0;
String stopIdToFetch;

enum ScreenType {
  FAVOURITE,
  PASSING_TIME
};

ScreenType screen = FAVOURITE;

void loop() {
  switch(screen) {
    case FAVOURITE:
      handleScreenFavourite();
      break;
    case PASSING_TIME:
      handleScreenPassingTime();
      break;
    default:
      screen = FAVOURITE;
  }
  //delay(500);
}

int position = 0;
int previousPosition = 0;
bool reloadFavourites = true;
String line1;
String line2;
bool clearLcd = true;
int arrowPosition = 0;
void handleScreenFavourite(){
  upButtonState=digitalRead(UP_BUTTON);
  selectButtonState=digitalRead(SELECT_BUTTON);
  downButtonState=digitalRead(DOWN_BUTTON);
  if(selectButtonState == HIGH || upButtonState == HIGH || downButtonState == HIGH){
    if(DEBUG){
      Serial.print(F("Button pushed: "));
      Serial.print(F("selectButtonState: "));Serial.print(selectButtonState);
      Serial.print(F(" upButtonState:"));Serial.print(upButtonState);
      Serial.print(F(" downButtonState"));Serial.println(downButtonState);
    }
    if(selectButtonState == HIGH){
      screen = PASSING_TIME;
      stopIdToFetch = favourites[position].stopId;
      Serial.println(F("Switch to Screen PASSING TIME"));
    } else if(upButtonState == HIGH){
      if(position>0){      
        previousPosition = position;
        position = position-1;
      }else{
        endOfRecord(UP, 2);
      }
    } else if(downButtonState == HIGH){
      if(position+1 < (sizeof(favourites)/sizeof(Favourite))){
        previousPosition = position;
        position = position+1;
      }else{
        endOfRecord(DOWN, 2);
      }
    }
    debouncePushButtons();
  }
  //Display favourites
  if(position != previousPosition || reloadFavourites ){
    if(DEBUG){
      Serial.print(F("Position changed: Position="));
      Serial.print(position);
      Serial.print(F(" Previous pos:"));
      Serial.println(previousPosition);
    }
    int modulo = position % 2;
    if(position > previousPosition || (reloadFavourites && modulo==0)){ //Going down
      if(!reloadFavourites)Serial.println(F("Going down"));
      if(modulo == 1){
        arrowPosition = 1;
      }else{
        clearLcd = true;  
        arrowPosition = 0;
        line1 = favourites[position].label;
        if(position+1 < (sizeof(favourites)/sizeof(Favourite))){
          line2 = favourites[position+1].label;
        }else{
          line2 = "----------------";
        }
      }    
    }else{ //Going up
      if(!reloadFavourites)Serial.println(F("Going up"));
      if(modulo == 1){
        clearLcd = true;
        arrowPosition = 1;
        line1 = favourites[position-1].label;
        line2 = favourites[position].label;
      }else{
        arrowPosition = 0;
      }  
    }
    reloadFavourites = false;
    if(clearLcd){
      Serial.println(F("Refresh favourites screen"));
      lcd.clear();  
      clearLcd = false;
      //Display lines
      lcd.setCursor(2,0);
      lcd.print(line1);
      lcd.setCursor(2,1);
      lcd.print(line2);
    }
    //Display arrow
    lcd.setCursor(0,arrowPosition);
    lcd.print(">");
    lcd.setCursor(0,arrowPosition ? 0 : 1);
    lcd.print(" ");
    previousPosition = position;
  }
}
bool passingTimeLoaded = false;
int passingTimePage = 0;
PassingTimeResponse* passingTimeResponse;
void handleScreenPassingTime(){  
  if(passingTimeLoaded){
    upButtonState=digitalRead(UP_BUTTON);
    selectButtonState=digitalRead(SELECT_BUTTON);
    downButtonState=digitalRead(DOWN_BUTTON);
    if(selectButtonState == HIGH || upButtonState == HIGH || downButtonState == HIGH){
      if(selectButtonState == HIGH){
        screen = FAVOURITE;
        reloadFavourites = true;
        passingTimeLoaded = false;
        Serial.println(F("Switch to Screen FAVOURITE"));
      }else if(downButtonState == HIGH){
        if(passingTimePage < (passingTimeResponse->numberOfResponses/2 + passingTimeResponse->numberOfResponses%2)){
          passingTimePage = passingTimePage+1;
          displayPassingTimeOnLcd(passingTimeResponse, passingTimePage); 
        }else{
          endOfRecord(DOWN, 0);
        }
      }else if(upButtonState == HIGH){
        if(passingTimePage > 1){
          passingTimePage = passingTimePage-1;
          displayPassingTimeOnLcd(passingTimeResponse, passingTimePage); 
        }else{
          endOfRecord(UP, 0);
        }
      }
      debouncePushButtons();
    }
  }else{
    passingTimeResponse = getPassingTime(stopIdToFetch);
    if( passingTimeResponse != 0 ){
      passingTimePage = 1;
      displayPassingTimeOnLcd(passingTimeResponse, passingTimePage); 
    }else{
      lcd.clear();
      lcd.print("Error");
    }
    passingTimeLoaded = true;
  }
}

void debouncePushButtons(){
  while(selectButtonState == HIGH || upButtonState == HIGH || downButtonState == HIGH){
    upButtonState=digitalRead(UP_BUTTON);
    selectButtonState=digitalRead(SELECT_BUTTON);
    downButtonState=digitalRead(DOWN_BUTTON);
  }
}

/** Display a small message for the user to tell him that he reached the top/bottom of the list*/
void endOfRecord(UP_DOWN direction, int leftPosition){
  if(direction == DOWN){
    lcd.setCursor(leftPosition,1);
    lcd.print(F("<<Bottom>>"));
    delay(1000);
    lcd.setCursor(leftPosition,1);
    lcd.print(line2);
  }else{
    lcd.setCursor(leftPosition,0);
    lcd.print(F("<< Top >>"));
    delay(1000);
    lcd.setCursor(leftPosition,0);
    lcd.print(line1);    
  }
}

void displayPassingTimeOnLcd(PassingTimeResponse* passingTimeResponse, int page){
  lcd.clear();
  line1 = passingTimeResponse->passingTimes[(page-1)*2]->getDisplay();
  if((page-1)*2 + 1 < passingTimeResponse->numberOfResponses){
    line2 = passingTimeResponse->passingTimes[(page-1)*2 + 1]->getDisplay();
  }else{
    line2 = "----------------";
  }
  lcd.print(line1);
  lcd.setCursor(0,1);
  lcd.print(line2);
}
