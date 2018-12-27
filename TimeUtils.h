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

static bool RTC_is_running = false;
// some date/time formats used in this app
const char* hhmmStringTpl = "  %02d:%02d  ";
static char hhmmString[13] = "  --:--  ";
const char* hhmmssStringTpl = "%02d:%02d:%02d";
static char hhmmssString[13] = "--:--:--"; 
const char* UpTimeStringTpl = "  %02d:%02d  ";
static char UpTimeString[13] = "  --:--  ";
const char* YYYYMMDD_HHMMSS_Tpl = "%04d-%02d-%02d %02d:%02d:%02d";
static char YYYYMMDD_HHMMSS_Str[32] = "YYYY-MM-DD HH:MM:SS";
static bool dayChangeTrigger = false;


#if RTC_PROFILE > HOBO // all profiles manage time except HOBO

  int current_day = -1;
  int current_hour = -1;

  enum TimeUpdateSources {
    SOURCE_NONE = 0,
    SOURCE_COMPILER = 1,
    SOURCE_RTC = 2,
    SOURCE_NTP = 3
  };

  void logTimeActivity(TimeUpdateSources source, int epoch) {
    preferences.begin("BLEClock", false);
    preferences.clear();
    //DateTime epoch = RTC.now();
    preferences.putUInt("epoch", epoch);
    preferences.putUChar("source", source);
    preferences.end();
  }

  void resetTimeActivity(TimeUpdateSources source) {
    preferences.begin("BLEClock", false);
    preferences.clear();
    preferences.putUInt("epoch", 0);
    preferences.putUChar("source", source);
    preferences.end();
  }

  struct TimeActivity {
    DateTime epoch;
    byte source;
  };

  void TimeInit() {
    preferences.begin("BLEClock", true);
    lastSyncDateTime = preferences.getUInt("epoch", millis());
    byte clockUpdateSource   = preferences.getUChar("source", 0);
    preferences.end();
    sprintf(YYYYMMDD_HHMMSS_Str, YYYYMMDD_HHMMSS_Tpl, 
      lastSyncDateTime.year(),
      lastSyncDateTime.month(),
      lastSyncDateTime.day(),
      lastSyncDateTime.hour(),
      lastSyncDateTime.minute(),
      lastSyncDateTime.second()
    );
    log_e("Defrosted lastSyncDateTime from NVS (may be bogus) : %s - source : %d", YYYYMMDD_HHMMSS_Str, clockUpdateSource);
    if(clockUpdateSource==SOURCE_NONE) {
      if(RTC.isrunning()) {
        log_d("[RTC] Forcing source to RTC and rebooting");
        logTimeActivity(SOURCE_RTC, 0);
        ESP.restart();
      } else {
        log_d("[RTC] isn't running!");
      }
    }
    nowDateTime = RTC.now();
  }

#endif // RTC_PROFILE > HOBO



static void timeHousekeeping(bool checkNTP=false) {
  unsigned long seconds_since_boot = millis() / 1000;
  unsigned long  minutes_since_boot = seconds_since_boot / 60;
  unsigned long  mm = minutes_since_boot % 60;
  unsigned long  hh = minutes_since_boot / 60;
  unsigned long  ss = seconds_since_boot % 60;

  #if RTC_PROFILE > HOBO

    #ifdef BUILD_NTPMENU_BIN // NTPMENU mode
      DateTime internalDateTime = DateTime(year(), month(), day(), hour(), minute(), second());
      nowDateTime = internalDateTime;
    #else // CHRONOMANIAC mode
      // - get the time from the internal clock (saves I2C calls)
      DateTime internalDateTime = DateTime(year(), month(), day(), hour(), minute(), second());
      // - compare internal RTC time and external RTC time every hour
      if( current_hour != internalDateTime.hour() || checkNTP) {
        DateTime externalDateTime = RTC.now();
        if( checkNTP ) {
          int64_t seconds_since_last_ntp_update = abs( externalDateTime.unixtime() - lastSyncDateTime.unixtime() );
          log_e("seconds_since_last_ntp_update = now(%d) - last(%d) = %d seconds", externalDateTime.unixtime(), lastSyncDateTime.unixtime(), seconds_since_last_ntp_update);
          if ( seconds_since_last_ntp_update > 86400 ) {
            Serial.println("Stalling");
            rollBackOrUpdateFromFS( BLE_FS, NTP_MENU_FILENAME );
          } else {
            setTime( externalDateTime.unixtime() );
          }
        } else {
          int64_t drift = abs( externalDateTime.unixtime() - internalDateTime.unixtime() );
          Serial.printf("[Hourly Synching Clocks] : %d seconds drift", drift);
          // - adjust internal RTC
          setTime( externalDateTime.unixtime() );
          //internalDateTime = DateTime(year(), month(), day(), hour(), minute(), second());
          //drift = abs( internalDateTime.unixtime() - externalDateTime.unixtime() );
          if(drift > 1) {
            log_e("[***** WTF Clocks don't agree after adjustment] %d - %d = %d\n", internalDateTime.unixtime(), externalDateTime.unixtime(), drift);  
          } else {
            log_e("[***** OK Clocks agree] %d - %d = %d\n", internalDateTime.unixtime(), externalDateTime.unixtime(), drift);
          }
        }
        internalDateTime = DateTime(year(), month(), day(), hour(), minute(), second());
        current_hour = internalDateTime.hour();
      }
  
      if( current_day!= internalDateTime.day() ) {
        // day changed! update bool so DB.maintain() and BLE know what to do
        dayChangeTrigger = true;
        current_day = internalDateTime.day();
      }
      nowDateTime = internalDateTime;
      
    #endif // ifndef BUILD_NTPMENU_BIN

    sprintf(hhmmString, hhmmStringTpl, internalDateTime.hour(), internalDateTime.minute());
    sprintf(hhmmssString, hhmmssStringTpl, internalDateTime.hour(), internalDateTime.minute(), internalDateTime.second());
    #if RTC_PROFILE == CHRONOMANIAC  // chronomaniac mode
      /*
      if (checkNTP && RTC.isrunning()) {
        int64_t deltaInSeconds = abs( internalDateTime.unixtime() - lastSyncDateTime.unixtime() );
        log_e("now(%d) - last(%d) = %d seconds", internalDateTime.unixtime(), lastSyncDateTime.unixtime(), deltaInSeconds);
        if ( deltaInSeconds > 86400) {
          log_e("[CHRONOMANIAC] Last Time Sync: %s (%d seconds ago). Time isn't fresh anymore, should reload NTP menu !!", YYYYMMDD_HHMMSS_Str, deltaInSeconds);
          Serial.println("Stalling");
          rollBackOrUpdateFromFS( BLE_FS, NTP_MENU_FILENAME );
          ESP.restart();
        }
      }*/
    #endif // RTC_PROFILE == CHRONOMANIAC
  #else // RTC_PROFILE <= HOBO
    sprintf(hhmmssString, hhmmssStringTpl, hh, mm, ss);
  #endif // RTC_PROFILE > HOBO

  sprintf(UpTimeString, UpTimeStringTpl, hh, mm);
  log_d("Time:%s, Uptime:", hhmmString, UpTimeString );
}


bool RTCSetup() {
  #if RTC_PROFILE == HOBO
    log_d("[RTC] Hobo mode, no time to waste :)");
    return false;
  #else
    RTC.begin(RTC_SDA/*26*/, RTC_SCL/*27*/);
    delay(100);
    if (!RTC.isrunning()) { // first run use case (or dead RTC battery)
      log_e("[RTC] NOT running, will try to adjust from hardcoded value");
      RTC.adjust(DateTime(__DATE__, __TIME__));
      logTimeActivity(SOURCE_COMPILER, DateTime(__DATE__, __TIME__).unixtime() );
      RTC_is_running = RTC.isrunning();
    } else {
      log_d("[RTC] running :-)");
      RTC_is_running = true;
    }
    return RTC_is_running;
  #endif
}



#ifdef BUILD_NTPMENU_BIN
  #include "NTP.h"
#endif

void timeSetup() {
  #if RTC_PROFILE==HOBO
    return;
  #else 
    //setenv("TZ", TZ_INFO, 1);
    //tzset();
    if(!RTCSetup()) {
      // RTC failure ....
      log_e("RTC Failure, switching to hobo mode");
    }
    #ifdef BUILD_NTPMENU_BIN
      NTPSetup();
    #else
      TimeInit();
    #endif
  #endif
  timeHousekeeping( true );
}


/*
 * 
 * 

#define UNIX_TIME 1537627296
#define TZ_INFO "MST7MDT6,M3.2.0/02:00:00,M11.1.0/02:00:00" //"USA_Mountain/DST"

#include <sys/time.h>

int setUnixtime(int32_t unixtime) {
  timeval epoch = {unixtime, 0};
  return settimeofday((const timeval*)&epoch, 0);
}

void setup() {
  Serial.begin(115200);
  setUnixtime(UNIX_TIME);
  setenv("TZ", TZ_INFO, 1);
  tzset();
}

void loop() {
  struct tm now;
  getLocalTime(&now,0);
  Serial.println(&now," %B %d %Y %H:%M:%S (%A)");
  delay(1000);
}

*/
