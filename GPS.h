/*\
 * GPS Helpers
\*/


HardwareSerial GPS(1); // uart 1
#define GPS_RX 39 // io pin number
#define GPS_TX 35 // io pin number
#define GPS_BAUDRATE 9600

static unsigned long LastGPSChange = millis();
static unsigned long NoGPSSignalSince = 0;
static bool GPSHasFix = false;
static bool GPSHasDateTime = false;
static float GPSLat = 0.00;
static float GPSLng = 0.00;

static void GPSInit() {
  GPS.begin(GPS_BAUDRATE, SERIAL_8N1, GPS_RX, GPS_TX);
  GPS.flush();
  // todo: launch a task to check for GPS health
}

#include <TinyGPS++.h> // https://github.com/mikalhart/TinyGPSPlus
TinyGPSPlus gps;

static void GPSRead() {
  while(GPS.available()) {
    gps.encode( GPS.read() );
  }
  if( gps.location.isValid() ) {
    GPSLat = gps.location.lat();
    GPSLng = gps.location.lng();
    GPSHasFix = true;
  } else {
    GPSLat = 0.00;
    GPSLng = 0.00;
  }
  if(gps.date.isUpdated() && gps.date.isValid() && gps.time.isValid()) {
    LastGPSChange = millis();
    GPSHasDateTime = true;
    NoGPSSignalSince = 0;
  } else {
    NoGPSSignalSince = millis() - LastGPSChange;
  }
}



static bool setGPSTime() {
  if( !GPSHasDateTime ) {
    Serial.println("GPS has no valid DateTime, cowardly aborting");
    return false;
  }

  if(gps.date.isValid() && gps.time.isValid() && gps.date.year() > 2000) {
    // fetch GPS Time
    DateTime GPS_UTC_Time = DateTime(gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());
    // apply timeZone
    DateTime GPS_Local_Time = GPS_UTC_Time.unixtime() + (int(timeZone*100)*36) + (summerTime ? 3600 : 0);
    #if HAS_EXTERNAL_RTC
      RTC.adjust( GPS_Local_Time );
      // TODO: check if RTC.adjust worked
      Serial.printf("External RTC adjusted from GPS Time (GMT%s%f [%s]): %04d-%02d-%02d %02d:%02d:%02d\n",
        timeZone>0 ? "+" : "",
        timeZone,
        summerTime ? "CEST" : "CET",
        GPS_Local_Time.year(),
        GPS_Local_Time.month(),
        GPS_Local_Time.day(),
        GPS_Local_Time.hour(),
        GPS_Local_Time.minute(),
        GPS_Local_Time.second()
      );
    #endif
    setTime( GPS_Local_Time.unixtime() );
    timeval epoch = {(time_t)GPS_Local_Time.unixtime(), 0};
    const timeval *tv = &epoch;
    settimeofday(tv, NULL);

    struct tm now;
    if( !getLocalTime(&now,0) ) {
      log_e("Failed to getLocalTime() after setTime() && settimeofday()");
      return false;
    } else {
      dumpTime( "Internal RTC adjusted from GPS Time", &now );
      logTimeActivity(SOURCE_GPS, GPS_Local_Time.unixtime());
      lastSyncDateTime = GPS_Local_Time;
      return true;
    }

  } else {
    Serial.printf("Can't set GPS Time yet (no signal since %ld seconds)\n", NoGPSSignalSince/1000);
    return false;
  }
}

// task wrapper
static void setGPSTime( void * param ) {
  setGPSTime();
}


/*

#include <TinyGPS.h>
TinyGPS gps;



DateTime GPSTime;

static void GPSRead() {
  while(GPS.available()) {
    gps.encode( GPS.read() );
  }
  int year;
  byte month, day, hour, minutes, second, hundredths;
  unsigned long fix_age;
  gps.crack_datetime(&year, &month, &day, &hour, &minutes, &second, &hundredths, &fix_age);

  if( year > 2000 ) {
    GPSTime = DateTime( year, month, day, hour, minutes, second );
    LastGPSChange = millis();
    GPSHasDateTime = true;
    NoGPSSignalSince = 0;
  } else {
    NoGPSSignalSince = millis() - LastGPSChange;
  }
}

static void setGPSTime( void * param ) {
  if( !GPSHasDateTime ) {
    Serial.println("GPS has no valid DateTime, cowardly aborting");
    return;
  }

  if( GPSTime.year() > 2000 ) {
    //DateTime UTCTime = DateTime(gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());
    long gap = millis() - LastGPSChange;
    DateTime LocalTime = GPSTime.unixtime() + gap + timeZone*3600;
    #if HAS_EXTERNAL_RTC
      RTC.adjust( LocalTime );
    #endif
    setTime( LocalTime.unixtime() );
    Serial.printf("Time adjusted to: %04d-%02d-%02d %02d:%02d:%02d\n",
      LocalTime.year(),
      LocalTime.month(),
      LocalTime.day(),
      LocalTime.hour(),
      LocalTime.minute(),
      LocalTime.second()
    );
    logTimeActivity(SOURCE_GPS, LocalTime.unixtime());
    lastSyncDateTime = LocalTime;
  } else {
    Serial.printf("Can't set GPS Time (no signal since %d seconds)\n", NoGPSSignalSince/1000);
  }
}

*/
