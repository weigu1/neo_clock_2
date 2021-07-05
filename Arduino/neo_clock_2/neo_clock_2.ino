/*
  neo_clock_2.ino
  
  weigu.lu

  V1.0 2021-07-05

  ---------------------------------------------------------------------------
  Copyright (C) 2021 Guy WEILER www.weigu.lu
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
  ---------------------------------------------------------------------------

  ConfigTime is only settings. The real work is done by the time()
  function (esp8266 core). Time() always returns an internal time counter.
  When the delay since prevous sync is reached (60 minutes by default), time()
  re-synchronises its internal counter with NTP server on the internet.

  CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00
  -1 = summer one hour behind UTC; -2 = winter two hour behind UTC
  M3.5.0: changes M3 = 3 month (march) 5 = 5 week (last week of march) 0 = sunday 2h00

  Andreas Spiess Video 299: https://youtu.be/r2UAmBLBBRM 
  https://werner.rothschopf.net/201802_arduino_esp8266_ntp.htm

  
  ESP8266: LOLIN/WEMOS D1 mini pro
  ESP32:   MH ET LIVE ESP32-MINI-KIT 
  
  MHET    | MHET    - LOLIN        |---| LOLIN      - MHET    | MHET
  
  GND     | RST     - RST          |---| TxD        - RxD(3)  | GND
   NC     | SVP(36) -  A0          |---| RxD        - TxD(1)  | 27 
  SVN(39) | 26      -  D0(16)      |---|  D1(5,SCL) -  22     | 25
   35     | 18      -  D5(14,SCK)  |---|  D2(4,SDA) -  21     | 32
   33     | 19      -  D6(12,MISO) |---|  D3(0)     -  17     | TDI(12)
   34     | 23      -  D7(13,MOSI) |---|  D4(2,LED) -  16     | 4
  TMS(14) | 5       -  D8(15,SS)   |---| GND        - GND     | 0   
   NC     | 3V3     - 3V3          |---|  5V        -  5V     | 2
  SD2(9)  | TCK(13)                |---|              TD0(15) | SD1(8)
  CMD(11) | SD3(10)                |---|              SD0(7)  | CLK(6)

  If using OTA:  We set the password here with it's md5 value
  To get md5 on Ubuntu use "md5sum": md5sum, enter, type pw, 2x Ctrl D

*/

#define STATIC        // if static IP needed (no DHCP)
#define OTA           // if Over The Air update needed (security risk!)
// The file "secrets.h" has to be placed in the sketchbook libraries folder
// in a folder named "Secrets" and must contain your secrets e.g.:
// const char *MY_WIFI_SSID = "mySSID"; const char *MY_WIFI_PASSWORD = "myPASS"; ...
#define USE_SECRETS
#define LIGHT_SENSOR

#include <coredecls.h>  // ! optional settimeofday_cb() callback to check on server
#include <Adafruit_NeoPixel.h>
#include "ESPToolbox.h"

/****** Lightsensor settings ******/
#ifdef LIGHT_SENSOR
  #include <Adafruit_Sensor.h>
  #include <Adafruit_TSL2561_U.h>
  Adafruit_TSL2561_Unified TSL = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
#endif //ifdef LIGHT_SENSOR

/****** WiFi and network settings ******/
#ifdef USE_SECRETS
  #include <secrets.h>
  const char *WIFI_SSID = MY_WIFI_SSID;           // ssid     
  const char *WIFI_PASSWORD = MY_WIFI_PASSWORD;   // password
  #ifdef OTA                                      // Over The Air update settings
    const char *OTA_NAME = "neo_clock_2";
    const char *OTA_PASS_HASH = MY_OTA_PASS_HASH; // md5 hash for OTA
  #endif // ifdef OTA  
#else
  const char *WIFI_SSID = mySSID;           // if no secrets file, add your SSID here
  const char *WIFI_PASSWORD = myPASSWORD;   // if no secrets file, add your PASS here
  #ifdef OTA                                // Over The Air update settings
    const char *OTA_NAME = "neo_clock_2";
    const char *OTA_PASS_HASH = myOTAHASH;  // if no secrets file, add your OTA HASH here
  #endif // ifdef OTA      
#endif  // ifdef USE_SECRETS
const char *NET_MDNSNAME = "NeoClock2";     // optional (access with myESP.local)
const char *NET_HOSTNAME = "NeoClock2";     // optional
#ifdef STATIC
  IPAddress NET_LOCAL_IP (192,168,1,77);    // 3x optional for static IP
  IPAddress NET_GATEWAY (192,168,1,20);
  IPAddress NET_MASK (255,255,255,0);  
#endif // ifdef STATIC*/
const word UDP_LOG_PORT = 6668;             // UDP logging settings
IPAddress UDP_LOG_PC_IP(192,168,1,50);
const char *NTP_SERVER = "lu.pool.ntp.org"; // NTP settings
// your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
const char *TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";


/****** NeoPixel settings ******/
const byte LED_PIN = 13;
const byte LED_COUNT = 60;
Adafruit_NeoPixel NeoPix(LED_COUNT, LED_PIN, NEO_GRBW);
const uint32_t long COLOR_SEC  = 0x00000001; // blue
const uint32_t long COLOR_MIN  = 0x00000100; // green
const uint32_t long COLOR_HOUR = 0x00010000; // red 
const uint32_t long COLOR_OFF  = 0x00000000; // black
byte neopix_sec,neopix_min, neopix_hour, neopix_sec_prev,neopix_min_prev, neopix_hour_prev;
uint32_t color_sec, color_min, color_hour, color_sm, color_sh, color_mh, color_smh;
byte brightness = 100;

ESPToolbox Tb;

time_t now = 0;
tm timeinfo;                      // time structure

struct My_Timeinfo {
  byte second;
  byte minute;
  byte hour;
  byte day;
  byte month;
  unsigned int year;
  byte weekday;
  unsigned int yearday;
  bool daylight_saving_flag;
  String name_of_day;
  String name_of_month;
  String date;
  String time;
  String datetime;
} my;

// ! optional  callback function to check if NTP server called
void time_is_set(bool from_sntp /* <= this parameter is optional */) {  
  Tb.log_ln("The NTP server was called!");
}

// ! optional change here if you want another NTP polling interval (default 1h)
uint32_t sntp_update_delay_MS_rfc_not_less_than_15000 () {
  return 4 * 60 * 60 * 1000UL; // every 4 hours
}

void setup() {
  Tb.set_led_log(true);                 // use builtin LED for debugging
  Tb.set_serial_log(true,0);            // 2 parameter = interface (1 = Serial1)
  Tb.set_udp_log(true, UDP_LOG_PC_IP, UDP_LOG_PORT); // use "nc -kulw 0 6666"  
  NeoPix.begin();
  settimeofday_cb(time_is_set); // ! optional  callback function to check if NTP server called
  init_ntp_time();
  init_wifi(); 
  get_time();
  Tb.log_ln("Epoch time: " + String(now));
  #ifdef LIGHT_SENSOR
    init_light_sensor();
  #endif //ifdef LIGHT_SENSOR
  #ifdef OTA
    Tb.init_ota(OTA_NAME, OTA_PASS_HASH);
  #endif // ifdef OTA
  Tb.blink_led_x_times(3);
  Tb.log_ln("Setup done!");
}

void loop() {  
  #ifdef OTA
    ArduinoOTA.handle();
  #endif // ifdef OTA
  get_time();
  neo_clock();
  if (Tb.non_blocking_delay(5000)) {
    Tb.log_ln(my.datetime);
    delay(50);    
    #ifdef LIGHT_SENSOR
      set_brightness_from_light();
    #endif //ifdef LIGHT_SENSOR
  }
  delay(20);
}

/********** INIT FUNCTIONS ****************************************************/
// init WiFi (static or dhcp)
void init_wifi() {
  #ifdef STATIC
    Tb.init_wifi_sta(WIFI_SSID, WIFI_PASSWORD, NET_HOSTNAME, NET_LOCAL_IP, 
                  NET_GATEWAY, NET_MASK);
  #else                  
    Tb.init_wifi_sta(WIFI_SSID, WIFI_PASSWORD, NET_MDNSNAME, NET_HOSTNAME);                       
  #endif // ifdef STATIC
}

#ifdef LIGHT_SENSOR
  // init light sensor
  void init_light_sensor() {
    Wire.begin(4, 5); // D3 and D4 on ESP8266
    Wire.setClock(400000L);
    Wire.status();
    if(!TSL.begin()) {
      Tb.log("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
      while(1);
    }
    TSL.enableAutoRange(true);                              // Auto-gain
    TSL.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);   // fast but low resolution  
  }
#endif //ifdef LIGHT_SENSOR
  
// init NTP time: call this before the WiFi connects!
void init_ntp_time() { 
  #ifdef ESP8266
    configTime(TZ_INFO, NTP_SERVER);    
  #else  
    configTime(0, 0, NTP_SERVER); // 0, 0 because we will use TZ in the next line
    setenv("TZ", TZ_INFO, 1);     // set environment variable with your time zone
    tzset();
  #endif
}

/********** TIME FUNCTION ****************************************************/

// epoch to tm structure and update global struct
void get_time() {
  time(&now);                     // this function calls the NTP server only every hour
  localtime_r(&now, &timeinfo);   // converts epoch time to tm structure
  my.second  = timeinfo.tm_sec;
  my.minute  = timeinfo.tm_min;
  my.hour  = timeinfo.tm_hour;
  my.day  = timeinfo.tm_mday;
  my.month  = timeinfo.tm_mon + 1;    // beer (Andreas video)
  my.year  = timeinfo.tm_year + 1900; // beer
  my.weekday = timeinfo.tm_wday; 
  if (my.weekday == 0) {              // beer
    my.weekday = 7;
  }
  my.yearday = timeinfo.tm_yday + 1;  // beer
  my.daylight_saving_flag  = timeinfo.tm_isdst;
  char buffer[25];  
  strftime(buffer, 25, "%A", localtime(&now));
  my.name_of_day = String(buffer);
  strftime(buffer, 25, "%B", localtime(&now));
  my.name_of_month = String(buffer);
  strftime(buffer, 25, "20%y-%m-%d", localtime(&now));
  my.date = String(buffer);
  strftime(buffer, 25, "%H:%M:%S", localtime(&now));
  my.time = String(buffer);  
  strftime(buffer, 25, "20%y-%m-%dT%H:%M:%S", localtime(&now));
  my.datetime = String(buffer);  
}

/******* CLOCK FUNCTIONS ******************************************************/
void neo_clock() {  
  neopix_sec = my.second;
  neopix_min = my.minute;
  neopix_hour = my.hour;
  if (neopix_sec == neopix_sec_prev) {
    return;
  }
  color_sec  = COLOR_SEC * brightness;  
  color_min  = COLOR_MIN * brightness;  
  color_hour = COLOR_HOUR * brightness;  
  color_sm   = color_sec + color_min;
  color_sh   = color_sec + color_hour;
  color_mh   = color_min + color_hour;
  color_smh  = color_sec + color_min + color_hour; 
  //Clear previous LEDs
  NeoPix.setPixelColor(neopix_sec_prev, COLOR_OFF);
  NeoPix.setPixelColor(neopix_min_prev, COLOR_OFF);
  NeoPix.setPixelColor(neopix_hour_prev, COLOR_OFF);  
  
  
  if (neopix_hour >= 12) {
    neopix_hour -= 12;
  }
  neopix_hour *= 5;
  neopix_hour += (neopix_min/12);
  neopix_sec_prev = neopix_sec;
  neopix_min_prev = neopix_min;
  neopix_hour_prev = neopix_hour;
  if ((neopix_sec == neopix_min) && (neopix_sec == neopix_hour)) {
    NeoPix.setPixelColor(neopix_sec, color_smh); 
    NeoPix.setPixelColor(neopix_min, color_smh);
    NeoPix.setPixelColor(neopix_hour, color_smh);
  }
  else if (neopix_sec == neopix_min) {
    NeoPix.setPixelColor(neopix_sec, color_sm); 
    NeoPix.setPixelColor(neopix_min, color_sm); 
    NeoPix.setPixelColor(neopix_hour, color_hour); 
  }
  else if (neopix_sec == neopix_hour) {
    NeoPix.setPixelColor(neopix_sec, color_sh); 
    NeoPix.setPixelColor(neopix_min, color_min);
    NeoPix.setPixelColor(neopix_hour, color_sh); 
  }
  else if (neopix_min == neopix_hour) {
    NeoPix.setPixelColor(neopix_sec, color_sec);  
    NeoPix.setPixelColor(neopix_min, color_mh); 
    NeoPix.setPixelColor(neopix_hour, color_mh);
  }
  else {          
    NeoPix.setPixelColor(neopix_sec, color_sec); 
    NeoPix.setPixelColor(neopix_min, color_min); 
    NeoPix.setPixelColor(neopix_hour, color_hour); 
  }
  NeoPix.show(); // Send the updated pixel colors to the hardware.    
}

#ifdef LIGHT_SENSOR
  void set_brightness_from_light() {
    sensors_event_t event;
    TSL.getEvent(&event);
    if (event.light) {    
      if (event.light > 255) {    
        event.light = 255;
      }
      brightness = map(event.light,1,255,20,150);
      Tb.log_ln("Light: " + String(event.light) +" lux, brightness: " + String(brightness));
    }
    else {
      Tb.log_ln("Sensor overload");
      brightness = 9;
    }
  }
#endif //ifdef LIGHT_SENSOR  
