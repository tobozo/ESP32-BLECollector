/*

  ESP32 DateTime wrapper for the BLE Collector
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

  This wrapper mainly exists to supply friend methods to the RTC Library based on JeeLabs's code http://news.jeelabs.org/code/
  The changes are added dependencies to PaulStoffregen's Time library  https://github.com/PaulStoffregen/Time/

*/

#include <sys/time.h> // needed by BLETimeServer
#include <TimeLib.h> // https://github.com/PaulStoffregen/Time

// helper
static uint8_t DateTimeConv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9')
    v = *p - '0';
  return 10 * v + *++p - '0';
}

static bool TimeIsSet = false;

// Simple general-purpose date/time class (no TZ / DST / leap second handling!)
class DateTime {
  public:
    DateTime( uint32_t t=0 );
    DateTime( tmElements_t dateTimeNow );
    DateTime( uint16_t year, uint8_t month, uint8_t day,
                 uint8_t hour=0, uint8_t min=0, uint8_t sec=0 );
    DateTime( const char* date, const char* time );
    uint16_t year() const       { return 1970 + yOff; }
    uint8_t month() const       { return m; }
    uint8_t day() const         { return d; }
    uint8_t hour() const        { return hh; }
    uint8_t minute() const      { return mm; }
    uint8_t second() const      { return ss; }
    tmElements_t get_tm() const     { return tm; }
    uint32_t unixtime() const; // 32-bit times as seconds since 1/1/1970
    static uint32_t tm2unixtime(tmElements_t tm); // conversion utility
  protected:
    uint8_t yOff=0, m=0, d=0, hh=0, mm=0, ss=0;
    tmElements_t tm;
};

DateTime::DateTime(uint32_t unixtime) {
  breakTime(unixtime, tm);
  m = tm.Month;
  d = tm.Day;
  hh = tm.Hour;
  mm = tm.Minute;
  ss = tm.Second;
  yOff = tm.Year; // offset from 1970;
};
DateTime::DateTime(tmElements_t dateTimeNow) {
  tm = dateTimeNow;
  m = tm.Month;
  d = tm.Day;
  hh = tm.Hour;
  mm = tm.Minute;
  ss = tm.Second;
  yOff = tm.Year; // offset from 1970;
};
DateTime::DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec) {
  if (year >= 1970)
      year -= 1970; // year to offset
  yOff = year;
  m = month;
  d = day;
  hh = hour;
  mm = min;
  ss = sec;
  tm = {ss, mm, hh, 0, d, m, yOff};
};
DateTime::DateTime (const char* date, const char* time) {
  // A convenient constructor for using "the compiler's time":
  //   DateTime now (__DATE__, __TIME__);
  // sample input: date = "Dec 26 2009", time = "12:34:56"
  yOff = DateTimeConv2d(date + 9) + 30; // 2000 offset to 1970 offset
  // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
  switch (date[0]) {
      case 'J': m = (date[1] == 'a') ? 1 : (date[2] == 'n') ? 6 : 7; break;
      case 'F': m = 2; break;
      case 'A': m = date[2] == 'r' ? 4 : 8; break;
      case 'M': m = date[2] == 'r' ? 3 : 5; break;
      case 'S': m = 9; break;
      case 'O': m = 10; break;
      case 'N': m = 11; break;
      case 'D': m = 12; break;
      default:  m = 0;
  }
  d = DateTimeConv2d(date + 4);
  hh = DateTimeConv2d(time);
  mm = DateTimeConv2d(time + 3);
  ss = DateTimeConv2d(time + 6);
  tm = {ss, mm, hh, 0, d, m, yOff};
};
uint32_t DateTime::unixtime() const {
  return tm2unixtime( tm );
}
uint32_t DateTime::tm2unixtime(tmElements_t tm_)  {
  uint32_t unixtime = makeTime(tm_); // convert time elements into time_t
  return unixtime;
}



// for debug

void dumpTime(const char* message, DateTime dateTime) {
   Serial.printf("[%s]: %04d-%02d-%02d %02d:%02d:%02d\n",
    message,
    dateTime.year(),
    dateTime.month(),
    dateTime.day(),
    dateTime.hour(),
    dateTime.minute(),
    dateTime.second()
  );
}

void dumpTime(const char* message, tmElements_t tm) {
  Serial.printf("[%s]: %04d-%02d-%02d %02d:%02d:%02d\n",
    message,
    tm.Year + 1970,
    tm.Month,
    tm.Day,
    tm.Hour,
    tm.Minute,
    tm.Second
  );
}
void dumpTime(const char* message, struct tm *info) {
  Serial.printf("%s (GMT%s%d) : %04d-%02d-%02d %02d:%02d:%02d\n",
    message,
    timeZone>0 ? "+" : "",
    timeZone,
    info->tm_year + 1900,
    info->tm_mon+1,
    info->tm_mday,
    info->tm_hour,
    info->tm_min,
    info->tm_sec
  );
}

void dumpTime(const char* message, time_t epoch) {
  tmElements_t nowUnixDateTime;
  breakTime( epoch, nowUnixDateTime );
  dumpTime( message, nowUnixDateTime );
}
