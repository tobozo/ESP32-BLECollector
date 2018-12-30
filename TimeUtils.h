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



static void timeHousekeeping() {
  unsigned long seconds_since_boot = millis() / 1000;
  unsigned long  minutes_since_boot = seconds_since_boot / 60;
  unsigned long  mm = minutes_since_boot % 60;
  unsigned long  hh = minutes_since_boot / 60;
  unsigned long  ss = seconds_since_boot % 60;
  
  DateTime internalDateTime = DateTime(year(), month(), day(), hour(), minute(), second());

  #if RTC_PROFILE == HOBO

    if( abs( seconds_since_boot - internalDateTime.unixtime() ) > 2 ) {
      // safe to assume internal RTC is running
      sprintf(hhmmString, hhmmStringTpl, internalDateTime.hour(), internalDateTime.minute());
      Time_is_set = true;
      nowDateTime = internalDateTime;
      if( current_hour != internalDateTime.hour() ) {
        hourChangeTrigger = true;
      }
    }
    sprintf(hhmmssString, hhmmssStringTpl, hh, mm, ss);

  #else // RTC_PROFILE != HOBO

    #ifdef BUILD_NTPMENU_BIN // NTPMENU mode

      nowDateTime = internalDateTime;

    #else // CHRONOMANIAC mode

      if( current_hour != internalDateTime.hour() ) {
        hourChangeTrigger = true;
      }
      // - get the time from the internal clock (saves I2C calls)
      // - compare internal RTC time and external RTC time every hour
      if( hourChangeTrigger ) {
        checkForTimeUptade( internalDateTime ); // and update internal clock if necessary
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

  #endif // RTC_PROFILE != HOBO

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


void timeSetup() {
  #if RTC_PROFILE==HOBO
    return;
  #else 
    if(!RTCSetup()) { // RTC failure ....
      log_e("RTC Failure, switching to hobo mode");
    }
    #ifdef BUILD_NTPMENU_BIN
      NTPSetup();
    #else
      TimeInit();
    #endif
  #endif
  timeHousekeeping();
}
