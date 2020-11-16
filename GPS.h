/*\
 * GPS Helpers
\*/


HardwareSerial GPS(1); // uart 1
//#define GPS_RX 33 // io pin number
//#define GPS_TX 32 // io pin number
#define GPS_BAUDRATE 9600

static unsigned long LastGPSChange = 0; // holds time when last GPS state change or data collection occured
static unsigned long NoGPSSignalSince = 1000; // milliseconds since the last GPS signal or state change
static bool GPSHasFix = false;
unsigned long GPSLastFix = 0;
static bool GPSHasDateTime = false;
static bool GPSDebugToSerial = false; // set this to true to debug, disables GPS decoding and prints to serial instead
static double GPSLat = 0.00;
static double GPSLng = 0.00;
static int GPSFailCounter = 0;
static int GPSFailThreshold = 10; // 2..10
static unsigned long GPSFailCheckDelay = 4000; // check for GPS health every 4s (unit=millis)

static void GPSInit()
{
  LastGPSChange    = millis();
  NoGPSSignalSince = 1000; // pretend there was a signal 1 second ago

  GPS.begin(GPS_BAUDRATE, SERIAL_8N1, GPS_RX, GPS_TX);
  GPS.flush();

}

#include <TinyGPS++.h> // https://github.com/mikalhart/TinyGPSPlus
TinyGPSPlus gps;

static void GPSRead()
{

  if( GPS.available() ) {
    do {
      if( GPSDebugToSerial ) {
        Serial.write( GPS.read() );
      } else {
        gps.encode( GPS.read() );
      }

    } while(GPS.available());

    if( gps.location.isValid() ) {
      GPSLat = gps.location.lat();
      GPSLng = gps.location.lng();
      GPSHasFix = true;
      GPSLastFix = millis();
      log_w("[GPS] FIX -> LAT: %f LNG: %f", GPSLat, GPSLng );
      //GPSFailCounter = 0;
    } else {
      GPSLat = 0.00f;
      GPSLng = 0.00f;
    }
    if(gps.date.isUpdated() && gps.date.isValid() && gps.time.isValid()) {
      LastGPSChange = millis();
      GPSHasDateTime = true; // time is valid but date may still be incomplete at this stage
      NoGPSSignalSince = 0;
      GPSFailCounter = 0;
    } else {
      // NMEA sent garbage or has no fix yet
      NoGPSSignalSince = millis() - LastGPSChange;
    }
  } else {
    // no signal on Serial2 ?
    NoGPSSignalSince = millis() - LastGPSChange;
  }

  // check for GPS health
  // no data in 30 seconds = something's wrong
  if (NoGPSSignalSince > GPSFailCheckDelay && gps.charsProcessed() < 10 ) {
    if( GPSFailCounter < GPSFailThreshold ) { // don't be spammy
      Serial.printf("[GPS] Did not get any data for %ld seconds\n", NoGPSSignalSince/1000);
    } else {
      // reset fail counter but double the delay
      if( GPSFailCheckDelay < 3600000 ) { // max 1h
        GPSFailCounter = 0;
        GPSFailCheckDelay *=2;
        Serial.printf("New GPSFailCheckDelay: %d\n", int(GPSFailCheckDelay) );
      }
      if( GPSFailThreshold > 2 ) { // don't go below 2
        GPSFailCounter = 0;
        GPSFailThreshold--;
        Serial.printf("New GPSFailThreshold: %d\n", int(GPSFailThreshold) );
      }
    }
    LastGPSChange = millis(); // reset timer to reduce spamming in the console
    GPSFailCounter++;
  }

}


static void getLatLng( void * param )
{
  if( GPSHasFix ) {
    Serial.printf("[GPS] Last fix %d seconds ago -> LAT: %f LNG: %f", int( (millis()-GPSLastFix)/1000 ), GPSLat, GPSLng );
  } else {
    Serial.println("[GPS] had no fix yet");
  }
}


static bool setGPSTime()
{
  if( GPSFailCounter > 0 ) {
    Serial.printf("GPS is not available (%d errors during last %d seconds)\n", int(GPSFailCounter), int(GPSFailCheckDelay/1000) );
    return false;
  }
  if( !GPSHasDateTime ) {
    Serial.println("GPS has no valid DateTime yet");
    return false;
  }


  if(gps.date.isValid() && gps.time.isValid() && gps.date.year() > 2000) {
    // TODO: check when this date was collected before trusting it
    // e.g. (millis() - LastGPSChange) or NoGPSSignalSince

    // fetch GPS Time
    DateTime GPS_UTC_Time = DateTime(gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());
    // apply timeZone
    DateTime GPS_Local_Time = GPS_UTC_Time.unixtime() + (int(timeZone*100)*36) + (summerTime ? 3600 : 0);
    #if HAS_EXTERNAL_RTC
      RTC.adjust( GPS_Local_Time );
      // TODO: check if RTC.adjust worked
      Serial.printf("External RTC adjusted from GPS Time (GMT%s%.2g [%s]): %04d-%02d-%02d %02d:%02d:%02d\n",
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
      GPSFailCounter = 0;
      return true;
    }

  } else {
    if (NoGPSSignalSince > GPSFailCheckDelay && gps.charsProcessed() < 10) {
      Serial.printf("[GPS] is unavailable, check the config: GPS_RX=%d, GPS_TX=%d, GPS_BAUDRATE=%d\n", GPS_RX, GPS_TX, GPS_BAUDRATE );
    } else {
      Serial.println("[GPS] Can't set GPS Time yet, check antenna or wait for a fix?");
    }
    return false;
  }
}

// task wrapper
static void setGPSTime( void * param )
{
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
