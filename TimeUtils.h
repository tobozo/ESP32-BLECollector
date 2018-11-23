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
static char hhmmString[13] = "--:--"; // %02d:%02d
static char hhmmssString[13] = "--:--:--"; // %02d:%02d:%02d
static char UpTimeString[13] = "00:00"; // %02d:%02d
static char LastSyncTimeString[32] = "YYYY-MM-DD HH:MM:SS";


#if RTC_PROFILE > HOBO // all profiles manage time except HOBO

  static DateTime nowDateTime;
  static DateTime lastSyncDateTime;
  
  void logTimeActivity(byte source) {
    preferences.begin("BLECollector", false);
    DateTime epoch = RTC.now();
    preferences.putUInt("epoch", epoch.unixtime());
    preferences.putUChar("source", source);
    preferences.end();
  }
  
  enum OTAPartitionNames {
    NO_PARTITION = -1,
    CURRENT_PARTITION = 0,
    NEXT_PARTITION = 1
  };
  
  enum TimeUpdateSources {
    SOURCE_NONE = 0,
    SOURCE_COMPILER = 1,
    SOURCE_RTC = 2,
    SOURCE_NTP = 3
  };
  
  struct TimeActivity {
    DateTime epoch;
    byte source;
  };
  
  
  TimeActivity getTimeActivity() {
    TimeActivity timeActivity;
    preferences.begin("BLECollector", true);
    timeActivity.epoch  = preferences.getUInt("epoch", 0);
    timeActivity.source = preferences.getUChar("source", 0);
    preferences.end();
  
    sprintf(LastSyncTimeString, "%04d-%02d-%02d %02d:%02d:%02d", 
      timeActivity.epoch.year(),
      timeActivity.epoch.month(),
      timeActivity.epoch.day(),
      timeActivity.epoch.hour(),
      timeActivity.epoch.minute(),
      timeActivity.epoch.second()
    );
  
    if(timeActivity.source==SOURCE_NONE) {
      if(RTC.isrunning()) {
        Serial.println("[RTC] Forcing source to RTC and rebooting");
        logTimeActivity(SOURCE_RTC);
        ESP.restart();
      } else {
        Serial.println("[RTC] isn't running!");
      }
    } else {
      lastSyncDateTime = timeActivity.epoch;
      nowDateTime = RTC.now();
      int32_t deltaInSeconds = nowDateTime.unixtime() - lastSyncDateTime.unixtime();
      Serial.println("[RTC] Last Time Sync: " + String(LastSyncTimeString) 
                     + " ( " + String(deltaInSeconds) 
                     + " seconds ago) using source #" + String(timeActivity.source));
  
      #if RTC_PROFILE > ROGUE // on NTP_MENU and CHRONOMANIAC SD-mirror themselves
        // mirror current binary to SD Card if needed
        char *currentMenuSignature = (char*)malloc(sizeoftrail);
        char *nextMenuSignature = (char*)malloc(sizeoftrail);
        char* binarySignature = (char*)malloc(sizeoftrail);
        currentMenuSignature = getSignature(CURRENT_PARTITION);
        nextMenuSignature    = getSignature(NEXT_PARTITION);
    
        if(strcmp(buildSignature, currentMenuSignature)==0) {
          // Build signature matches with current partition, looks fine!
          binarySignature = getBinarySignature( SD_MMC, MENU_FILENAME );
          if( strcmp(binarySignature, currentMenuSignature)==0 ) {
            // Perfect match, nothing to do \o/
            return timeActivity;
          }
        } else if(strcmp(buildSignature, nextMenuSignature)==0) {
          // strange situation where current partition doesn't match the binary in memory
          Serial.println("[WUT] Build signature matches with next partition");
          Serial.println("[WUT] Current Build Signature is: " + String( buildSignature ));
          Serial.println("[WUT] Current Partition Signature is: " + String( currentMenuSignature ));
          Serial.println("[WUT] Next Partition Signature is: " + String( nextMenuSignature ));
          return timeActivity;
        } else { // this should not happen
          Serial.println("[WUT] Build signature matches neither current nor next partition");
          Serial.println("[WUT] Current Build Signature is: " + String( buildSignature ));
          Serial.println("[WUT] Current Partition Signature is: " + String( currentMenuSignature ));
          Serial.println("[WUT] Next Partition Signature is: " + String( nextMenuSignature ));
          delay(10000);
          return timeActivity;
        }

        //Serial.println("[Flash2SD] Binary signature " + String(binarySignature));
        Out.println();
        Out.println("[SD!=FLASH]"); // mirror current flash partition to SD
        updateToFS(SD_MMC, MENU_FILENAME, CURRENT_PARTITION);
  
      #endif
  
    }
    return timeActivity;
  }

#endif

void updateTimeString(bool checkNTP=false) {
  unsigned long seconds_since_boot = millis() / 1000;
  uint32_t minutes_since_boot = seconds_since_boot / 60;
  uint32_t mm = minutes_since_boot % 60;
  uint32_t hh = minutes_since_boot / 60;
  
  #if RTC_PROFILE > HOBO
    nowDateTime = RTC.now();
    sprintf(hhmmString, "%02d:%02d", nowDateTime.hour(), nowDateTime.minute());
    sprintf(hhmmssString, "%02d:%02d:%02d", nowDateTime.hour(), nowDateTime.minute(), nowDateTime.second());
    #if RTC_PROFILE == CHRONOMANIAC  // chronomaniac mode
      if (checkNTP && RTC_is_running) {
        int32_t deltaInSeconds = nowDateTime.unixtime() - lastSyncDateTime.unixtime();
        if ( deltaInSeconds > 86400/*300*/) {
          Serial.println("[CHRONOMANIAC] Last Time Sync: " 
           + String(LastSyncTimeString) 
           + " ( " + String(deltaInSeconds) 
           + " seconds ago). Time isn't fresh anymore, should reload NTP menu !!");
          rollBackOrUpdateFromFS( SD_MMC, NTP_MENU_FILENAME );
          ESP.restart();
        }
      }
    #endif
  #else
    //sprintf(hhmmString, "%02d:%02d", hh, mm);
    sprintf(hhmmssString, "%02d:%02d:%02d", hh, mm, ss);
  #endif

  sprintf(UpTimeString, "%02d:%02d", hh, mm);
  //Serial.println("Time:" + String(hhmmString) + " Uptime:" + String(UpTimeString));
}


bool RTCSetup() {
  #if RTC_PROFILE == HOBO
    Serial.println("[RTC] Hobo mode, no time to waste :)");
    return false;
  #else
    // RTC wired to SDA, SCL (26,27 on Wrover Kit)
    // using Wire.begin() instead of RTC.begin()
    // because, as usual, Adafruit library tries to sel
    // Adafruit hardware by not letting you choose the pins
    Wire.begin(RTC_SDA/*26*/, RTC_SCL/*27*/);
    /*
    if(!RTC.begin()) { 
      Serial.println("[RTC] begin() failed");
      return false;
    }*/
    delay(100); // why the fsck is this needed
    if (!RTC.isrunning()) { // false positives here, why ??
      Serial.println("[RTC] NOT running, will try to adjust from hardcoded value");
      RTC.adjust(DateTime(__DATE__, __TIME__));
      logTimeActivity(SOURCE_COMPILER);
      #if RTC_PROFILE == CHRONOMANIAC // chronomaniac mode
        if (RTC.isrunning()) {
          Serial.println("[RTC] alive, will load the NTP Sync binary and adjust time");
          rollBackOrUpdateFromFS( SD_MMC, "/NTPMenu.bin" );
          ESP.restart(); // TODO: adjust from a BLE characteristic or a GPS
        } else {
          Serial.println("[RTC] broken or bad wiring, adjusting from NTP is futile.");
        }
      #endif
      RTC_is_running = RTC.isrunning();
    } else {
      Serial.println("[RTC] running :-)");
      RTC_is_running = true;
    }
    return RTC_is_running;
  #endif
}

static bool sd_mounted = false;

bool SDSetup() {
  if(sd_mounted) return true;
  unsigned long max_wait = 500;
  byte attempts = 10;
  while ( sd_mounted == false && attempts>0) {
    if (SD_MMC.begin() ) {
      sd_mounted = true;
    } else {
      Serial.println("[SD] Mount Failed");
      delay(max_wait);
    }
    attempts--;
  }
  return sd_mounted;
}



#ifndef BUILD_NTPMENU_BIN

  #if RTC_PROFILE==CHRONOMANIAC
    /*
    #warning "Scan Mode enabled !!"
    #warning "This sketch binary should be named '" BLE_MENU_FILENAME "' and saved on the SD Card"
    #warning "If you need to compile the NTP-adjust version, set 'RTC_PROFILE' to 'NTP_MENU' in settings.h"
    */
  #endif

  void timeSetup() {
    #if RTC_PROFILE==HOBO
      return;
    #else 
      if(!RTCSetup()) {
        // RTC failure ....
        Serial.println("RTC Failure, switching to hobo mode");
      }
      if(!SDSetup()) {
        // SD Card failure
        Out.println("SD Card Failure, insert SD or check wiring, halting");
        while(1) { ; }
      }
      TimeActivity lastTimeSync = getTimeActivity();
    #endif
  }

#else

  /*
  #warning "NTP Sync Mode enabled !!"
  #warning "This sketch binary should be named '" NTP_MENU_FILENAME "' and saved on the SD Card"
  #warning "It will only adjust time from NTP and reload the '" BLE_MENU_FILENAME "' sketch afterwards"
  #warning "If you need to compile the BLECollector, set 'RTC_PROFILE' to 'NTP_MENU' in settings.h"
  */
  #include <WiFi.h>
  #include <HTTPClient.h>
  
  const char* NTP_SERVER = "europe.pool.ntp.org";
  const char* TZ_INFO = "CET-1CEST,M3.3.0,M10.5.0/3"; // build your TZ String here e.g. https://phpsecu.re/tz/get.php?timezone=Europe/Paris
  struct tm timeinfo;
  bool WiFiConnected = false;
  bool NTPTimeSet = false;
  
  #ifdef MYSSID // see Settings.h
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
        Out.println("[WiFi] Restarting ("+String(attempts)+" attempts left)");
        WiFi.mode(WIFI_OFF);
        WiFi_Begin();
      }
      Serial.print(".");
      delay(500);
    }
    Serial.println();
    if(WiFi.status() == WL_CONNECTED) {
      Out.println();
      Out.println("[WiFi] Got an IP Address:" + WiFi.localIP().toString());
    } else {
      Out.println();
      Out.println("[WiFi] No IP Address");
    }
    return WiFi.status() == WL_CONNECTED;
  }


  void timeSetup() {
    if(!RTCSetup()) {
      // RTC failure ....
      Out.println("[RTC] Failure, check the wiring and battery, halting");
      while(1) { ; }
    }
    if(!SDSetup()) {
      // SD Card failure
      Out.println("[SD] Failure, insert SD or check wiring, halting");
      while(1) { ; }
    }
    TimeActivity lastTimeSync = getTimeActivity();
    
    configTzTime(TZ_INFO, NTP_SERVER);
    if(WiFiConnect()) {
      WiFiConnected = true;
      if (getLocalTime(&timeinfo, 10000)) {  // wait up to 10sec to sync
        Serial.println(&timeinfo, "Time set: %B %d %Y %H:%M:%S (%A)");
        NTPTimeSet = true;
        RTC.adjust(DateTime(timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
        if( RTC.isrunning() ) {
          Out.println("[RTC] Adjusted from NTP \\o/");
          logTimeActivity(SOURCE_NTP);
        } else {
          Out.println("[NTP] OK but RTC died :-(");
        }
      } else {
        Out.println("[NTP] Unreachable. Time not set");
        ESP.restart();
      }
    } else {
      // give up
      Out.println("[WiFi] No NTP Sync, regressing to ROGUE mode");
      // regress to ROGUE mode
      logTimeActivity(SOURCE_NONE);
    }
    rollBackOrUpdateFromFS( SD_MMC, BLE_MENU_FILENAME );
    ESP.restart();
  }

#endif
