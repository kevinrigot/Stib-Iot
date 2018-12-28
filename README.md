# Stib-Iot
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

![Alt text](stib-iot_bb.png?raw=true "Breadboard")
