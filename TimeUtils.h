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

static unsigned long forcedUptime = 0;

enum TimeUpdateSources {
  SOURCE_NONE = 0,
  SOURCE_COMPILER = 1,
  SOURCE_RTC = 2,
  SOURCE_NTP = 3,
  SOURCE_BLE = 4,
  SOURCE_GPS = 5
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

/*
 * Time Update Situations
 *
 * External RTC update sources: BLE NTP, WiFi NTP
 * Internal RTC update sources: BLE NTP or External RTC
 *
 * External RTC needs update:
 *  - when time is not set
 *  - every 24h
 *
 * Internal RTC needs update:
 *  - when time is not set
 *  - every hour if external RTC, otherwise every 24h
 *
 * 
 *                 External RTC | No External RTC
 *                 -------------|----------------
 *        WiFi NTP      X       | Not applicable/useless
 *         BLE NTP      X       |      X
 *    Internal RTC      X       |
 *      GPS Module      X       |      X
 * 
 * */

#if HAS_GPS
  #include "GPS.h"
#endif
#include "NTP.h"


void uptimeSet() {
  unsigned long seconds_since_boot = millis() / 1000;
  unsigned long minutes_since_boot = seconds_since_boot / 60;
  unsigned long hours_since_boot   = minutes_since_boot / 60;
  unsigned long days_since_boot    = hours_since_boot / 24;
  unsigned long mm = minutes_since_boot % 60;
  unsigned long hh = minutes_since_boot / 60;
  unsigned long ss = seconds_since_boot % 60;
  unsigned long forcedUptimes = forcedUptime + hh;
  if( forcedUptimes < 24 ) {
    sprintf( UpTimeString, UpTimeStringTpl, forcedUptimes, mm );
  } else if( forcedUptimes <= 48 ) {
    sprintf( UpTimeString, UpTimeStringTplDays, forcedUptimes, "hours" );
  } else {
    sprintf( UpTimeString, UpTimeStringTplDays, forcedUptimes / 24, "days" );
  }
}


static void timeHousekeeping() {
  unsigned long seconds_since_boot = millis() / 1000;
  unsigned long minutes_since_boot = seconds_since_boot / 60;
  unsigned long hours_since_boot   = minutes_since_boot / 60;
  unsigned long days_since_boot    = hours_since_boot / 24;
  unsigned long mm = minutes_since_boot % 60;
  unsigned long hh = minutes_since_boot / 60;
  unsigned long ss = seconds_since_boot % 60;
  
  DateTime internalDateTime = DateTime(year(), month(), day(), hour(), minute(), second());
  // before adjustment checks
  if( current_hour != internalDateTime.hour() ) {
    if( current_hour != -1 ) {
      log_e("hourchangeTrigger=true (%02dh => %02dh)", current_hour, internalDateTime.hour());
      HourChangeTrigger = true;
    }
    current_hour = internalDateTime.hour();
  } else {
    HourChangeTrigger = false;
  }

  if( current_day != internalDateTime.day() ) {
    if( current_day != -1 ) {
      // day changed! update bool so DB.maintain() and BLE know what to do
      log_e("DayChangeTrigger=true (%02d => %02d)", current_day, internalDateTime.day());
      DayChangeTrigger = true;
      HourChangeTrigger = false;
    }
    current_day = internalDateTime.day();
  } else {
    DayChangeTrigger = false;
  }
  
  // - get the time from the internal clock
  // - compare with external clocks sources if applicable
  if( HourChangeTrigger || DayChangeTrigger ) {
    if( checkForTimeUpdate( internalDateTime ) ) { // and update internal clock if necessary
      internalDateTime = DateTime(year(), month(), day(), hour(), minute(), second());
      current_hour = internalDateTime.hour();
    }
  }
  sprintf(hhmmString, hhmmStringTpl, internalDateTime.hour(), internalDateTime.minute());
    
  #if HAS_EXTERNAL_RTC
  
    if( abs( seconds_since_boot - internalDateTime.unixtime() ) > 2 ) { // internal datetime is set
      // safe to assume internal RTC is running
      TimeIsSet = true;
    } 
    sprintf(hhmmssString, hhmmssStringTpl, hh, mm, ss);
    
  #else // HAS_EXTERNAL_RTC=false

    sprintf(hhmmssString, hhmmssStringTpl, internalDateTime.hour(), internalDateTime.minute(), internalDateTime.second());

  #endif // HAS_EXTERNAL_RTC

  nowDateTime = internalDateTime;
  uptimeSet();
  log_d("Time: %s, Uptime: %s", hhmmString, UpTimeString );
}



bool RTCSetup() {
  #if HAS_EXTERNAL_RTC
    RTC.begin(RTC_SDA/*26*/, RTC_SCL/*27*/);
    delay(100);
    if (!RTC.isrunning()) { // first run use case (or dead RTC battery)
      log_e("[RTC] NOT running, will try to adjust from hardcoded value");
      RTC.adjust(DateTime(__DATE__, __TIME__));
      logTimeActivity(SOURCE_COMPILER, DateTime(__DATE__, __TIME__).unixtime() );
      RTCisRunning = RTC.isrunning();
    } else {
      log_d("[RTC] running :-)");
      RTCisRunning = true;
    }
    return RTCisRunning;
  #else
    log_d("[RTC] Hobo mode, no time to waste :)");
    return false;
  #endif
}


void timeSetup() {
  #if HAS_EXTERNAL_RTC
    if(!RTCSetup()) { // RTC failure ....
      log_e("RTC Failure, switching to hobo mode");
    }
    TimeInit();
    timeHousekeeping();
  #else
    uptimeSet();
  #endif
}
