/*\
 * NTP Helpers
\*/

// returns true if time has been updated
static bool checkForTimeUpdate( DateTime &internalDateTime ) {
  //DateTime externalDateTime = internalDateTime;
  int64_t seconds_since_last_ntp_update = abs( internalDateTime.unixtime() - lastSyncDateTime.unixtime() );
  if ( seconds_since_last_ntp_update >= 3600 ) { // GPS sync every hour
    log_e("seconds_since_last_ntp_update = now(%d) - last(%d) = %d seconds", internalDateTime.unixtime(), lastSyncDateTime.unixtime(), seconds_since_last_ntp_update);
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
  #if HAS_EXTERNAL_RTC
    if(clockUpdateSource==SOURCE_NONE) {
      if(RTC.isrunning()) {
        log_d("[RTC] Forcing source to RTC and rebooting");
        logTimeActivity(SOURCE_RTC, 0);
        ESP.restart();
      } else {
        log_d("[RTC] isn't running!");
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
      dumpTime("RTC DateTime:", nowDateTime);
    } else {
      log_d("Failed to get system time after setting RTC");
    }


    TimeIsSet = true;
  #endif
}
