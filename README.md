# P1-Meter-ESP8266
Software for the ESP8266 that sends P1 smart meter data to Homey (with CRC checking and OTA firmware updates)
Many thanks to Jan ten Hove by writing this code for Domoticz: https://github.com/jantenhove/P1-Meter-ESP8266
This repo is a fork from Jan and changed to communicate with Homey instead of Domoticz

### Installation instructions
- Make sure that your ESP8266 can be flashed from the Arduino environnment: https://github.com/esp8266/Arduino
- Install the SoftSerial library from: https://github.com/plerup/espsoftwareserial
- Install the HomeyDuino library from: https://github.com/athombv/homey-arduino-library
- Place all files from this repository in a directory. Open the .ino file with Arduino.
- Adjust WIFI, Homey and debug settings at the top of the file
- Compile and flash
- Install the HomeyDuino App to Homey from: https://homey.app/nl-nl/app/com.athom.homeyduino/Homeyduino/
- Connect your ESP8266 to the P1 port of your smart meter and power on
- Add a device to Homey from the HomeyDuino category and select the ESPP1Meter

### Connection of the P1 meter to the ESP8266
You need to connect the smart meter with a RJ11 connector. This is the pinout to use
![RJ11 P1 connetor](http://gejanssen.com/howto/Slimme-meter-uitlezen/RJ11-pinout.png)
(You could also use an RJ45 connector and file the sides to make it fit the RJ11 socket)

Connect GND->GND on ESP, RTS->3.3V on ESP and RxD->any digital pin on ESP. In this sketch I use D5.
When you're equipped with an ESMR5 smart meter, it is possible to power the ESP8266 from the 5V on the P1 port.
