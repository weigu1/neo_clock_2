# Neo clock 2: NeoPixel clock with ESP8266 and minimalistic design

![neo clock 2](png/neo_clock_2_800.png "neo clock 2")

## All infos on: <http://www.weigu.lu/microcontroller/neo_clock_2/index.html>

## Some infos

A NeoPixel ring with 60 RGBW LED's connected to an ESP8266 gives us a clock using NTP server to get the accurate time. No external RTC needed. The time lib that is integrated in the ESP8266 core gets the time from an NTP server and runs ab internal RTC. Our hardware is reduced to the strict minimum. Only a 74HCT125 IC is needed as logic-level shifter. A light sensor can be used to dim the LED's at night.





