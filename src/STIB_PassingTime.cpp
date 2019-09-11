#include <Arduino.h>
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
    - ArduinoJson v5.13.4: https://arduinojson.org/
    - ArduinoSort: https://github.com/emilv/ArduinoSort
    - LiquidCrystal I2C: https://github.com/johnrickman/LiquidCrystal_I2C
    - Time from ESP8266
    - SimpleDSTadjust: https://platformio.org/lib/show/1276/simpleDSTadjust

    Resources:
    - Configure NodeMcu: https://www.youtube.com/watch?v=p06NNRq5NTU

    TODO:
    v Regenerate token
    v CA cert vs fingerprint? --> Ignore validation of fingerprint
    v Fix memory leak when making couple of Http calls
    v Get remaining time instead of expected time arrival
    v Format the remaining time to be at the end of the line + use down arrow instead of 0 + align texts
    v Auto refresh every 15sec
    v 3D print case
    v Summer time issue
*/
#define APP_NAME  "Stib IOT - " __FILE__
#define APP_VERSION  "v0.5-" __DATE__ " " __TIME__
#define DEBUG true
#include <PassingTime.h>
#include <PassingTimeService.h>
#include <Favourite.h>
//Configure WIFI_SSID, WIFI_PWD, API_TOKEN, REFRESH_RATE_SEC and favourites
#include <config.h>
#include <EEPROM.h>
/** Librairies */
#include <ESP8266WiFi.h>
#include <Wire.h>  // This library is already built in to the Arduino IDE
#include <LiquidCrystal_I2C.h> //This library you can add via Include Library > Manage Library > 
#include <time.h>
#include <simpleDSTadjust.h>


enum UP_DOWN{
  UP,
  DOWN
};

/** Pin Layout 
  * D1: SCL
  * D2: SDA
*/
LiquidCrystal_I2C lcd(0x27, 16, 2);
#define UP_BUTTON D4
#define SELECT_BUTTON D5
#define DOWN_BUTTON D6

uint8_t down_arrow[8]  = {0x4,0x4,0x4,0x4,0xff,0xe,0x4};
const String DOWN_ARROW = String("\1");

BearSSL::WiFiClientSecure * client = new BearSSL::WiFiClientSecure();
HTTPClient http;

/** Method signatures */
void displayPassingTimeOnLcd(PassingTimeResponse* passingTimeResponse, int page);
void retrievePassingTime();
void debugPassingTimeResponse();
void debouncePushButtons();
void requestNewAccessToken();
void fatalErrorInApiCall(String message);
String formatPassingTimeForLcd(PassingTime *passingTime, unsigned long secSinceBeginOfDay);
unsigned long getNumberOfSecSinceBeginOfDay();
void endOfRecord(UP_DOWN direction, int leftPosition);

int timezone = 1 * 3600; //GMT +1
int dst = 0; //Daylight saving
struct dstRule StartRule =  {"CEST", Last, Sun, Mar, 2, 3600};
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};
simpleDSTadjust dstAdjusted(StartRule, EndRule);
char *dstAbbrev;

void configureTime(){
  configTime(timezone, dst, "pool.ntp.org","time.nist.gov");
  Serial.println(F("Waiting for Internet time"));
  while(!time(nullptr)){
     Serial.print(F("*"));
     delay(1000);
  }
  Serial.println(F("Time response....OK"));
  // Load DST rules
  time_t t = dstAdjusted.time(&dstAbbrev);
  struct tm *timeinfo = localtime (&t);
  char buf[30];
  sprintf(buf, "%02d/%02d/%04d %02d:%02d:%02d %s\n", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, dstAbbrev);
  Serial.print(buf);
}

bool connectToWifi(){
  Serial.print(F("Connecting to "));
  Serial.println(WIFI_SSID);
  lcd.setCursor(0, 0);
  lcd.print(F("Connecting WiFi "));
  WiFi.mode(WIFI_STA);
  lcd.setCursor(0,1);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if(count == 0) WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(500);
    lcd.print(".");
    Serial.print(".");
    count++;
    if(count>15){
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

enum ScreenType {
  FAVOURITE,
  PASSING_TIME
};


class AppState {
  public:
    int upButtonState = 0;
    int selectButtonState = 0;
    int downButtonState = 0;
    ScreenType screen = FAVOURITE;
    String stopIdToFetch;
    unsigned int position = 0;
    unsigned int previousPosition = 0;
    bool reloadFavourites = true;
    String line1;
    String line2;
};
AppState appState;

void handleScreenFavourite(){
  appState.upButtonState=digitalRead(UP_BUTTON);
  appState.selectButtonState=digitalRead(SELECT_BUTTON);
  appState.downButtonState=digitalRead(DOWN_BUTTON);
  if(appState.selectButtonState == HIGH || appState.upButtonState == HIGH || appState.downButtonState == HIGH){
    if(DEBUG){
      Serial.print(F("Button pushed: "));
      Serial.print(F("selectButtonState: "));Serial.print(appState.selectButtonState);
      Serial.print(F(" upButtonState:"));Serial.print(appState.upButtonState);
      Serial.print(F(" downButtonState"));Serial.println(appState.downButtonState);
    }
    if(appState.selectButtonState == HIGH){
      appState.screen = PASSING_TIME;
      appState.stopIdToFetch = favourites[appState.position].stopId;
      Serial.println(F("Switch to Screen PASSING TIME"));
      lcd.clear();
    } else if(appState.upButtonState == HIGH){
      if(appState.position>0){      
        appState.previousPosition = appState.position;
        appState.position = appState.position-1;
      }else{
        endOfRecord(UP, 2);
      }
    } else if(appState.downButtonState == HIGH){
      if(appState.position+1 < (sizeof(favourites)/sizeof(Favourite))){
        appState.previousPosition = appState.position;
        appState.position = appState.position+1;
      }else{
        endOfRecord(DOWN, 2);
      }
    }
    debouncePushButtons();
  }
  //Display favourites
  if(appState.position != appState.previousPosition || appState.reloadFavourites ){
    bool clearLcd = false;
    int arrowPosition = 0;
    if(DEBUG){
      Serial.print(F("Position changed: Position="));
      Serial.print(appState.position);
      Serial.print(F(" Previous pos:"));
      Serial.println(appState.previousPosition);
    }
    int modulo = appState.position % 2;
    if(appState.position > appState.previousPosition || (appState.reloadFavourites && modulo==0)){ //Going down
      if(!appState.reloadFavourites)Serial.println(F("Going down"));
      if(modulo == 1){
        arrowPosition = 1;
      }else{
        clearLcd = true;  
        arrowPosition = 0;
        appState.line1 = favourites[appState.position].label;
        if(appState.position+1 < (sizeof(favourites)/sizeof(Favourite))){
          appState.line2 = favourites[appState.position+1].label;
        }else{
          appState.line2 = "----------------";
        }
      }    
    }else{ //Going up
      if(!appState.reloadFavourites)Serial.println(F("Going up"));
      if(modulo == 1){
        clearLcd = true;
        arrowPosition = 1;
        appState.line1 = favourites[appState.position-1].label;
        appState.line2 = favourites[appState.position].label;
      }else{
        arrowPosition = 0;
      }  
    }
    appState.reloadFavourites = false;
    if(clearLcd){
      Serial.println(F("Refresh favourites screen"));
      lcd.clear();  
      clearLcd = false;
      //Display lines
      lcd.setCursor(2,0);
      lcd.print(appState.line1);
      lcd.setCursor(2,1);
      lcd.print(appState.line2);
    }
    //Display arrow
    lcd.setCursor(0,arrowPosition);
    lcd.print((char)126);
    lcd.setCursor(0,arrowPosition ? 0 : 1);
    lcd.print(" ");
    appState.previousPosition = appState.position;
  }
}

class PassingTimeState{
   public:
    unsigned long lastUpdate = 0;
    bool fatalErrorOccured = false;
    int passingTimePage = 0;
    PassingTimeResponse* passingTimeResponse = NULL;
    void clearPassingTimeResponse(){
      if(passingTimeResponse != NULL){
        passingTimeResponse->clear();
        delete passingTimeResponse;
        passingTimeResponse = NULL;
      }
    }
};
PassingTimeState passingTimeState;
void handleScreenPassingTime(){  
  if(passingTimeState.lastUpdate != 0 && (passingTimeState.fatalErrorOccured || (millis() - passingTimeState.lastUpdate) < REFRESH_RATE_SEC * 1000) ){
    appState.upButtonState=digitalRead(UP_BUTTON);
    appState.selectButtonState=digitalRead(SELECT_BUTTON);
    appState.downButtonState=digitalRead(DOWN_BUTTON);
    if(appState.selectButtonState == HIGH || appState.upButtonState == HIGH || appState.downButtonState == HIGH){
      if(appState.selectButtonState == HIGH){
        appState.screen = FAVOURITE;
        appState.reloadFavourites = true;
        passingTimeState.lastUpdate = 0;
        passingTimeState.fatalErrorOccured = false;
        Serial.println(F("Switch to Screen FAVOURITE"));
      }else if(appState.downButtonState == HIGH){
        if(passingTimeState.passingTimePage < (passingTimeState.passingTimeResponse->numberOfResponses/2 + passingTimeState.passingTimeResponse->numberOfResponses%2)){
          passingTimeState.passingTimePage = passingTimeState.passingTimePage+1;
          displayPassingTimeOnLcd(passingTimeState.passingTimeResponse, passingTimeState.passingTimePage); 
        }else{
          endOfRecord(DOWN, 0);
        }
      }else if(appState.upButtonState == HIGH){
        if(passingTimeState.passingTimePage > 1){
          passingTimeState.passingTimePage = passingTimeState.passingTimePage-1;
          displayPassingTimeOnLcd(passingTimeState.passingTimeResponse, passingTimeState.passingTimePage); 
        }else{
          endOfRecord(UP, 0);
        }
      }
      debouncePushButtons();
    }
  }else{
    retrievePassingTime();
  }
}

void retrievePassingTime(){
  lcd.setCursor(0,0);
  lcd.print(F("Loading..."));
  if(DEBUG){
    Serial.print(F("Free RAM = "));
    Serial.println(ESP.getFreeHeap(), DEC); 
  } 
  passingTimeState.clearPassingTimeResponse();
  passingTimeState.passingTimeResponse = getPassingTime(&http, client, appState.stopIdToFetch);
  if( passingTimeState.passingTimeResponse != 0 && passingTimeState.passingTimeResponse->httpCode == 200 ){
    if(DEBUG){
      debugPassingTimeResponse();
    }
    passingTimeState.passingTimePage = 1;
    displayPassingTimeOnLcd(passingTimeState.passingTimeResponse, passingTimeState.passingTimePage); 
    passingTimeState.lastUpdate = millis();
  }else if( passingTimeState.passingTimeResponse != 0 && passingTimeState.passingTimeResponse->httpCode == 401 ){
    requestNewAccessToken();
  }else{
    fatalErrorInApiCall("get passing time");
    // Update fingerprint in config
  }
}

void fatalErrorInApiCall(String message){
  Serial.print(F("Fatal error occured in API call:"));
  Serial.println(message);
  lcd.clear();
  lcd.print(F("Er:Cert expired?"));
  lcd.setCursor(0,1);
  lcd.print(message);
  delay(2000);
  passingTimeState.fatalErrorOccured = true;
}

void requestNewAccessToken(){
  Serial.println(F("Token expired, request new token"));
  lcd.clear();
  lcd.print(F("Token expired"));
  lcd.setCursor(0,1);
  lcd.print(F("Req. new token.."));
  String newToken = getNewToken(&http, client);
  if(newToken != "ERROR"){
    writeToken(newToken);  
  }else{
    fatalErrorInApiCall("request token");
  }
}

void debugPassingTimeResponse(){
  Serial.println(F("Response:"));
  unsigned long numberOfSecSinceBeginOfDay = getNumberOfSecSinceBeginOfDay();
  for(int k =0; k<passingTimeState.passingTimeResponse->numberOfResponses ; k++){
    Serial.println(formatPassingTimeForLcd(passingTimeState.passingTimeResponse->passingTimes[k],numberOfSecSinceBeginOfDay) + " - " +passingTimeState.passingTimeResponse->passingTimes[k]->getRawExpectedTime());
  }
  Serial.println(F("---------------"));
}

void debouncePushButtons(){
  while(appState.selectButtonState == HIGH || appState.upButtonState == HIGH || appState.downButtonState == HIGH){
    appState.upButtonState=digitalRead(UP_BUTTON);
    appState.selectButtonState=digitalRead(SELECT_BUTTON);
    appState.downButtonState=digitalRead(DOWN_BUTTON);
  }
}

/** Display a small message for the user to tell him that he reached the top/bottom of the list*/
void endOfRecord(UP_DOWN direction, int leftPosition){
  if(direction == DOWN){
    lcd.setCursor(leftPosition,1);
    lcd.print(F("<<Bottom>>"));
    delay(1000);
    lcd.setCursor(leftPosition,1);
    lcd.print(appState.line2);
  }else{
    lcd.setCursor(leftPosition,0);
    lcd.print(F("<< Top >>"));
    delay(1000);
    lcd.setCursor(leftPosition,0);
    lcd.print(appState.line1);    
  }
}

unsigned long getNumberOfSecSinceBeginOfDay(){
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm* p_tm = localtime(&now);
  while(p_tm->tm_year < 100){
    Serial.print(F("Current year: "));
    Serial.print(p_tm->tm_year);
    Serial.println(F("[ERROR] Current date is not fully initialized! Delay 200ms"));
    delay(200);
    now = time(nullptr);
    p_tm = localtime(&now);
  }
  if(DEBUG){
    Serial.print(F("Current time:"));
    Serial.print(p_tm->tm_hour);
    Serial.print(F(":"));
    Serial.print(p_tm->tm_min);
    Serial.print(F(":"));
    Serial.println(p_tm->tm_sec);
  }
  return p_tm->tm_hour *3600 + p_tm->tm_min *60 + p_tm->tm_sec;
}

String formatPassingTimeForLcd(PassingTime *passingTime, unsigned long secSinceBeginOfDay){
  long remainingTime = passingTime->getRemainingTime(secSinceBeginOfDay);
  String remainingTimeStr = String(remainingTime);
  if(remainingTime == 0){
    remainingTimeStr = DOWN_ARROW + DOWN_ARROW;
  }else if(remainingTime < 10){
    remainingTimeStr = " " + remainingTimeStr;
  }else if(remainingTime > 90){
    remainingTimeStr = "--";
  }
  
  return (passingTime->getLine() + "  ").substring(0,3) + (passingTime->getDestination() + "       ").substring(0, 10) + " " + remainingTimeStr;
}


void displayPassingTimeOnLcd(PassingTimeResponse* passingTimeResponse, int page){
  lcd.clear();
  unsigned long numberOfSecSinceBeginOfDay = getNumberOfSecSinceBeginOfDay();
  appState.line1 = formatPassingTimeForLcd(passingTimeResponse->passingTimes[(page-1)*2],numberOfSecSinceBeginOfDay);
  if((page-1)*2 + 1 < passingTimeResponse->numberOfResponses){
    appState.line2 = formatPassingTimeForLcd(passingTimeResponse->passingTimes[(page-1)*2 + 1],numberOfSecSinceBeginOfDay);
  }else{
    appState.line2 = F("----------------");
  }
  lcd.print(appState.line1);
  lcd.setCursor(0,1);
  lcd.print(appState.line2);
}

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
  lcd.createChar(1, down_arrow);
  
  if(!connectToWifi()){
    Serial.println(F("Fail to connect. Wrong password? Reset micro controller."));
    ESP.restart();
  }
  configureTime();
  initializeEeprom();
  initializeToken();
}

void loop() {
  switch(appState.screen) {
    case FAVOURITE:
      handleScreenFavourite();
      break;
    case PASSING_TIME:
      handleScreenPassingTime();
      break;
    default:
      appState.screen = FAVOURITE;
  }
}
