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
  unsigned long minutes_since_boot = seconds_since_boot / 60;
  unsigned long hours_since_boot   = minutes_since_boot / 60;
  unsigned long days_since_boot    = hours_since_boot / 24;
  unsigned long mm = minutes_since_boot % 60;
  unsigned long hh = minutes_since_boot / 60;
  unsigned long ss = seconds_since_boot % 60;
  
  DateTime internalDateTime = DateTime(year(), month(), day(), hour(), minute(), second());

  // before adjustment checks
  if( current_hour != internalDateTime.hour() ) {
    log_e("hourchangeTrigger=true (%d vs %d)", current_hour, internalDateTime.hour());
    HourChangeTrigger = true;
    current_hour = internalDateTime.hour();
  }

  if( current_day!= internalDateTime.day() ) {
    // day changed! update bool so DB.maintain() and BLE know what to do
    log_e("DayChangeTrigger=true (%d vs %d)", current_day, internalDateTime.day());
    DayChangeTrigger = true;
    current_day = internalDateTime.day();
  }

  #if SKETCH_MODE==SKETCH_MODE_BUILD_DEFAULT
  
    // - get the time from the internal clock
    // - compare with external clocks sources if applicable
    if( HourChangeTrigger ) {
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

  #endif // SKETCH_MODE==SKETCH_MODE_BUILD_DEFAULT

  nowDateTime = internalDateTime;
  sprintf(UpTimeString, UpTimeStringTpl, hh, mm);
  log_d("Time:%s, Uptime:", hhmmString, UpTimeString );
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
    #if SKETCH_MODE==SKETCH_MODE_BUILD_DEFAULT
      TimeInit();
    #else
      NTPSetup();
    #endif
  #endif
  timeHousekeeping();
}
