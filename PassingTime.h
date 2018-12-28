class PassingTime{
  String line;
  String destination;
  String expectedTime;

  public:
    PassingTime(String line, String destination, String expectedTime) : 
      line(line),
      destination(destination),
      expectedTime(expectedTime)
    {      
    }

    String getDisplay(){
      return line + " " + destination.substring(0, 7) + " " + expectedTime.substring(11, 16);
    }

    String getRemainingTime(){
      return expectedTime.substring(14, 16);
    }
};


class PassingTimeResponse{
  public:
    int numberOfResponses = 0;
    PassingTime **passingTimes;
  
    PassingTimeResponse(PassingTime **items, int arraySize){
      passingTimes = items;
      numberOfResponses = arraySize;    
    }
};
