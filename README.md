# Stib-Mivb-Passing Time
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
    v Regenerate token
    - CA cert vs fingerprint?
    v Fix memory leak when making couple of Http calls
    v Get remaining time instead of expected time arrival
    v Format the remaining time to be at the end of the line + use down arrow instead of 0 + align texts
    v Auto refresh every 15sec
    - 3D print case

![Alt text](stib-iot_bb.png?raw=true "Breadboard")
