/*\
 * NTP Helpers
\*/

// returns true if time has been updated
static bool checkForTimeUpdate( DateTime &internalDateTime ) {
  //DateTime externalDateTime = internalDateTime;
  int64_t seconds_since_last_ntp_update = abs( internalDateTime.unixtime() - lastSyncDateTime.unixtime() );
  if ( seconds_since_last_ntp_update >= 3500 ) { // GPS sync every hour +/- 3% precision
    log_d("seconds_since_last_ntp_update = now(%d) - last(%d) = %d seconds", internalDateTime.unixtime(), lastSyncDateTime.unixtime(), seconds_since_last_ntp_update);
    #if TIME_UPDATE_SOURCE==TIME_UPDATE_BLE // will trigger bletime if any BLETimeServer is found
      ForceBleTime = true;
      HasBTTime = false;
      return false;
    #elif HAS_GPS==true && TIME_UPDATE_SOURCE==TIME_UPDATE_GPS
      return setGPSTime();
    #else
      #if HAS_EXTERNAL_RTC // adjust internal RTC accordingly, needed for filesystem operations
        DateTime externalDateTime = RTC.now(); // this may return some shit when I2C fails
        setTime( externalDateTime.unixtime() );
        return true;
      #else
        // no reliable time source to update from
        return false;
      #endif
    #endif
  } else {
    // no need to update time
    return false;
  }
}


void TimeInit() {
  preferences.begin("BLEClock", true);
  lastSyncDateTime = preferences.getUInt("epoch", millis());
  byte clockUpdateSource = preferences.getUChar("source", 0);
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
  #if HAS_EXTERNAL_RTC
    if(clockUpdateSource==SOURCE_NONE) {
      if(RTC.isrunning()) {
        log_w("[RTC] Forcing source to RTC and rebooting");
        logTimeActivity(SOURCE_RTC, 0);
        ESP.restart();
      } else {
        log_e("[RTC] isn't running!");
        return;
      }
    }
    nowDateTime = RTC.now(); // fetch time from external RTC
    setTime( nowDateTime.unixtime() ); // sync to local clock
    timeval epoch = {(time_t)nowDateTime.unixtime(), 0};
    const timeval *tv = &epoch;
    settimeofday(tv, NULL);
    struct tm now;
    if( getLocalTime(&now,0) ) {
      dumpTime("System RTC setTime Success", nowDateTime);
    } else {
      #ifdef WITH_WIFI
      log_e("System RTC setTime from External RTC failed, run stopBLE command to sync from NTP");
      #endif
      dumpTime("RTC.now()", nowDateTime);
    }
    TimeIsSet = true;
  #endif
}





#ifdef WITH_WIFI


  #include <sys/time.h>
  #include "lwip/apps/sntp.h"

  //const char* NTP_SERVER = "europe.pool.ntp.org";
  //static bool getNTPTime(void);
  //static void initNTP(void);

  static void initNTP(void) {
    Serial.println("Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char*)NTP_SERVER);
    sntp_init();
  }


  static bool getNTPTime(void) {
    initNTP();
    time_t now = 0;
    struct tm timeinfo = {};
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
      Serial.printf("Waiting for system time to be set... (%d/%d)\n", retry, retry_count);
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      time(&now);
      localtime_r(&now, &timeinfo);
    }
    if( retry == retry_count ) {
      Serial.println("Failed to set system time from NTP...");
      return false;
    } else {
      Serial.println("[[NTP] System Local time adjusted!");

      struct timeval tv;
      //bt_time_t _time;
      struct tm* _t;
      gettimeofday(&tv, nullptr);
      _t = localtime(&(tv.tv_sec));
      DateTime NTP_UTC_Time( _t->tm_year-70/*1900 to 1970 offset*/, _t->tm_mon + 1, _t->tm_mday, _t->tm_hour, _t->tm_min, _t->tm_sec );
      DateTime NTP_Local_Time = NTP_UTC_Time.unixtime() + timeZone*3600;

      dumpTime("[NTP] UTC Time", NTP_UTC_Time );
      dumpTime("[NTP] Local Time", NTP_Local_Time );
      #if HAS_EXTERNAL_RTC
        RTC.adjust( NTP_Local_Time );
        Serial.println("[[NTP] RTC Local time adjusted!");
        dumpTime("RTC.now()", RTC.now() );
      #endif
      nowDateTime = NTP_Local_Time;
      return true;
    }
  }




#endif // WITH_WIFI
