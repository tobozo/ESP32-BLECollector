/*

  ESP32 BLE Collector - A BLE scanner with sqlite data persistence on the SD Card
  Source: https://github.com/tobozo/ESP32-BLECollector

  MIT License

  Copyright (c) 2018 tobozo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  -----------------------------------------------------------------------------

*/

bool RTC_is_running = false;
static char timeString[13] = "00:00"; // %02d:%02d
static char UpTimeString[13] = "00:00"; // %02d:%02d
#if RTC_PROFILE > HOBO
static DateTime nowDateTime;
#endif

void updateTimeString() {
  #if RTC_PROFILE > HOBO
  nowDateTime = RTC.now();
  sprintf(timeString, " %02d:%02d ", nowDateTime.hour(), nowDateTime.minute());
  #endif
  unsigned long seconds_since_boot = millis() / 1000;
  uint32_t minutes_since_boot = seconds_since_boot / 60;
  uint32_t mm = minutes_since_boot % 60;
  uint32_t hh = minutes_since_boot / 60;
  sprintf(UpTimeString, " %02d:%02d ", hh, mm);
  Serial.println("Time:" + String(timeString) + " Uptime:" + String(UpTimeString));
}


void checkForTimeUpdate() {
  #ifndef BUILD_NTPMENU_BIN
    #if RTC_PROFILE == CHRONOMANIAC  // chronomaniac mode
    if( millis()/1000 > 86400 ) {
      // time to NTP Sync ?
      updateFromFS( SD_MMC, "/NTPMenu.bin" );
      ESP.restart();
    }
    #endif
  #endif
}



bool RTCSetup() {
  #if RTC_PROFILE == HOBO
    Serial.println("Hobo mode, no time to waste :)");
    return false;
  #else
    Wire.begin(26, 27); // RTC wired to SDA, SCL (26,27 on Wrover Kit)
    if(!RTC.begin()) {
      Serial.println("RTC.begin() failed");
      return false;
    }
    if (!RTC.isrunning()) {
      Serial.println("Suspicious: RTC was NOT running, will try to adjust from hardcoded value");
      RTC.adjust(DateTime(__DATE__, __TIME__));
      #ifndef BUILD_NTPMENU_BIN // BLEMenu.bin mode only !!
        #if RTC_PROFILE == CHRONOMANIAC // chronomaniac mode
          if (RTC.isrunning()) {
            Serial.println("RTC is alive, will load the NTP Sync binary and adjust time");
            updateFromFS( SD_MMC, "/NTPMenu.bin" );
            ESP.restart(); // TODO: adjust from a BLE characteristic or a GPS
          } else {
            Serial.println("RTC Module broken or bad wiring, adjusting from NTP is futile.");
          }
        #endif
      #endif
      RTC_is_running = RTC.isrunning();
    } else {
      Serial.println("RTC is running :-)");
      RTC_is_running = true;
    }
    return RTC_is_running;
  #endif
}

bool SDSetup() {
  unsigned long max_wait = 500;
  byte attempts = 10;
  bool sd_mounted = false;
  while ( sd_mounted == false && attempts>0) {
    if (SD_MMC.begin() ) {
      sd_mounted = true;
    } else {
      Serial.println("Card Mount Failed");
      delay(max_wait);
    }
    attempts--;
  }
  return sd_mounted;
}






#ifndef BUILD_NTPMENU_BIN

  #if RTC_PROFILE==CHRONOMANIAC
    #warning "Scan Mode enabled !!"
    #warning "This sketch binary should be named 'BLEMenu.bin' and saved on the SD Card"
    #warning "If you need to compile the NTP-adjust version, set 'RTC_PROFILE' to 'NTP_MENU' in settings.h"
  #endif

  void timeSetup() {
    if( RTC_PROFILE==HOBO) return;
    if(!RTCSetup()) {
      // RTC failure ....
      Serial.println("RTC Failure, switching to hobo mode");
    }
  }


#else

  #warning "NTP Sync Mode enabled !!"
  #warning "This sketch binary should be named 'NTPMenu.bin' and saved on the SD Card"
  #warning "It will only adjust time from NTP and reload the 'BLEMenu.bin' sketch afterwards"
  #warning "If you need to compile the BLECollector, set 'RTC_PROFILE' to 'NTP_MENU' in settings.h"
  
  #include <WiFi.h>
  #include <HTTPClient.h>
  
  const char* NTP_SERVER = "europe.pool.ntp.org";
  const char* TZ_INFO = "CET-1CEST,M3.3.0,M10.5.0/3"; // build your TZ String here e.g. https://phpsecu.re/tz/get.php?timezone=Europe/Paris
  struct tm timeinfo;
  bool WiFiConnected = false;
  bool NTPTimeSet = false;
  
  #ifdef MYSSID
    #define WiFi_Begin() WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  #else
    #define WiFi_Begin() WiFi.begin();
  #endif
  
  
  bool WiFiConnect() {
    unsigned long init_time = millis();
    unsigned long last_attempt = millis();
    unsigned long max_wait = 10000;
    byte attempts = 5;
    btStop();
    WiFi_Begin();
    while(WiFi.status() != WL_CONNECTED && attempts>0) {
      if( last_attempt + max_wait < millis() ) {
        attempts--;            
        last_attempt = millis();
        Out.println("Restarting WiFi ("+String(attempts)+" attempts left)");
        WiFi.mode(WIFI_OFF);
        WiFi_Begin();
      }
      Serial.print(".");
      delay(500);
    }
    Out.println();
    Out.println("Got an IP Address:" + WiFi.localIP().toString());
    return WiFi.status() == WL_CONNECTED;
  }
  
  
  void timeSetup() {
    if(!RTCSetup()) {
      // RTC failure ....
      Out.println("RTC Failure, check the wiring and battery, halting");
      while(1) { ; }
    }
    if(!SDSetup()) {
      // SD Card failure
      Out.println("SD Card Failure, insert SD or check wiring, halting");
      while(1) { ; }
    }
    configTzTime(TZ_INFO, NTP_SERVER);
    if(WiFiConnect()) {
      WiFiConnected = true;
      if (getLocalTime(&timeinfo, 10000)) {  // wait up to 10sec to sync
        Serial.println(&timeinfo, "Time set: %B %d %Y %H:%M:%S (%A)");
        NTPTimeSet = true;
        RTC.adjust(DateTime(timeinfo.tm_year+1900, timeinfo.tm_mon, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
        if( RTC.isrunning() ) {
          Out.println("RTC Adjusted from NTP \\o/");
        } else {
          Out.println("NTP OK but RTC died :-(");
        }
      } else {
        Out.println("NTP Unreachable. Time not set");
        ESP.restart();
      }
    } else {
      Out.println("No WiFi => No NTP Sync");
    }
    WiFi.mode(WIFI_OFF);
    updateFromFS( SD_MMC, "/BLEMenu.bin" );
    ESP.restart();
  }

#endif
