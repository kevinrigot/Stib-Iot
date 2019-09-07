#pragma once
#include <Arduino.h>

class PassingTime{
  String line;
  String destination;
  String expectedTime;
  unsigned long expectedTimeInSec;

  public:
    PassingTime(String line, String destination, String expectedTime) : 
      line(line),
      destination(destination),
      expectedTime(expectedTime)
    {      
      int hour = expectedTime.substring(11, 13).toInt();
      int min = expectedTime.substring(14, 16).toInt();
      int sec = expectedTime.substring(17, 19).toInt();
      expectedTimeInSec = hour*3600 + min*60 + sec;
    }

    String getLine(){
      return line;
    }

    String getDestination(){
      return destination;
    }

    String getRawExpectedTime(){
      return expectedTime;
    }

    /** Calculate the remaining time in min
     *  If difference is less than 45sec, count it as 0, if greater than 45sec, count the full minute
     *  When current time is 21h28m14s and it is expected at 21h30m00s, it should show 2min
     *  When current time is 21h28m16s and it is expected at 21h30m00s, it should show 1min
     *  
     *  When current time is greater than the expected time, return 0 as incoming.
     */
    long getRemainingTime(unsigned long secSinceBeginOfDay){
      long diff = expectedTimeInSec - secSinceBeginOfDay;
      if( diff < -5*60){
        diff = (24*3600 - secSinceBeginOfDay) + expectedTimeInSec;
      } else if( diff < 60){
        return 0;
      }
      if( (diff % 60)>45){
        return (diff/60)+1;
      }else{
        return diff/60;
      }
    }
};


class PassingTimeResponse{
  public:
    int httpCode = 0;
    int numberOfResponses = 0;
    PassingTime **passingTimes = NULL;

    PassingTimeResponse(int httpCode):
      httpCode(httpCode)
    {      
    }
  
    PassingTimeResponse(PassingTime **items, int arraySize){
      passingTimes = items;
      numberOfResponses = arraySize;    
      httpCode = 200;
    }

    void clear(){
      if(passingTimes != NULL){
        for(int i = 0; i< numberOfResponses; i++){
          delete passingTimes[i];
          passingTimes[i] = NULL;
        }
      }
    }
};
