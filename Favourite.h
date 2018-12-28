class Favourite{
  public:
    String stopId;
    String label;

    Favourite(String stopId, String label) : 
      stopId(stopId),
      label(label) {
        
      }  

    String getDisplay(){
      return label;
    }
};
