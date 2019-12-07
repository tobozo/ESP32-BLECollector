

// returns true if time has been updated
static bool checkForTimeUpdate( DateTime &internalDateTime ) {
  bool checkNTP = false;
  #if HAS_EXTERNAL_RTC
    DateTime externalDateTime = RTC.now();
  #else // only have internal RTC
    DateTime externalDateTime = internalDateTime;
  #endif
  int64_t seconds_since_last_ntp_update = abs( externalDateTime.unixtime() - lastSyncDateTime.unixtime() );
  if ( seconds_since_last_ntp_update > 86400 ) {
    log_e("seconds_since_last_ntp_update = now(%d) - last(%d) = %d seconds", externalDateTime.unixtime(), lastSyncDateTime.unixtime(), seconds_since_last_ntp_update);
    checkNTP = true;
  } else {
    checkNTP = false;
  }
  if( checkNTP ) {
    #if TIME_UPDATE_SOURCE==TIME_UPDATE_BLE // will trigger bletime if any BLETimeServer is found
      ForceBleTime = true;
      HasBTTime = false;
      return false;
    #elif HAS_GPS==true && TIME_UPDATE_SOURCE==TIME_UPDATE_GPS
      setGPSTime( NULL );
      return true;
    #else
      return false;
    #endif
  } else { // just calculate the drift
    int64_t drift = abs( externalDateTime.unixtime() - internalDateTime.unixtime() );
    if(drift>0) {
      Serial.printf("[Clocks drift] : %d seconds\n", drift);
    }
    // - adjust internal RTC
    #if HAS_EXTERNAL_RTC // have external RTC, adjust internal RTC accordingly
      setTime( externalDateTime.unixtime() );
      if(drift > 5) {
        log_e("[***** WTF Clocks don't agree after adjustment] %d - %d = %d", internalDateTime.unixtime(), externalDateTime.unixtime(), drift);  
      } else {
        log_i("[***** OK Clocks agree-ish] %d - %d = %d", internalDateTime.unixtime(), externalDateTime.unixtime(), drift);
      }
      return true;
    #else
      return false;
    #endif
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
    dumpTime("RTC DateTime:", nowDateTime);
    TimeIsSet = true;
  #endif
}
