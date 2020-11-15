/*

  ESP32 RTC DS1307/BM8563 implementations for the BLE Collector
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

  This driver was baked from different drivers (M5Core2, Adafruit and other forks)
  in order to support different time formats as inpout and output, and play well
  with other time libraries while supporting a standard syntax for easy
  substitution.

  It implements most methods from JeeLabs's library http://news.jeelabs.org/code/
  with added dependencies to PaulStoffregen's Time library  https://github.com/PaulStoffregen/Time/

*/

#if defined( ARDUINO_M5STACK_Core2 ) // M5Core2 uses BM8563

  //#define BM8563_ADDR 0x51 // M5Core2 RTC I2C address
  #define BLE_RTC BLE_RTC_BM8563 // alias for BLECollector
  #define M5_RTC M5.Rtc // syntax sugar

  class BLE_RTC_BM8563
  {
    public:
      static bool begin(uint8_t sdaPin=SDA, uint8_t sclPin=SCL);
      static void adjust(const tmElements_t& dt);
      static void adjust(const time_t& dt);
      static void adjust(const DateTime& dt);
      uint8_t isrunning(void);
      static tmElements_t now();
      static uint32_t unixtime();

      static void GetTime(tmElements_t &RTC_TimeStruct);
      static void GetDate(tmElements_t &RTC_DateStruct);

      static void SetTime(const tmElements_t *RTC_TimeStruct);
      static void SetDate(const tmElements_t *RTC_DateStruct);
  };

  // but but but why u no put code in cpp file ??? :-))

  void BLE_RTC_BM8563::GetTime(tmElements_t &RTC_TimeStruct)
  {
    RTC_TimeTypeDef tmpTime;
    M5_RTC.GetTime( &tmpTime );
    RTC_TimeStruct.Second  = tmpTime.Seconds;
    RTC_TimeStruct.Minute  = tmpTime.Minutes;
    RTC_TimeStruct.Hour    = tmpTime.Hours;
  }


  void BLE_RTC_BM8563::GetDate(tmElements_t &RTC_DateStruct)
  {
    RTC_DateTypeDef tmpDate;
    M5_RTC.GetDate( &tmpDate );
    RTC_DateStruct.Day   = tmpDate.Date;
    RTC_DateStruct.Wday  = tmpDate.WeekDay;
    RTC_DateStruct.Month = tmpDate.Month;
    RTC_DateStruct.Year  = tmpDate.Year - 1970;
  }


  void BLE_RTC_BM8563::SetTime(const tmElements_t *RTC_TimeStruct)
  {
    if(RTC_TimeStruct == NULL)
      return;

    RTC_TimeTypeDef tmpTime;
    tmpTime.Hours   = RTC_TimeStruct->Hour;
    tmpTime.Minutes = RTC_TimeStruct->Minute;
    tmpTime.Seconds = RTC_TimeStruct->Second;
    M5_RTC.SetTime( &tmpTime );
  }


  void BLE_RTC_BM8563::SetDate(const tmElements_t *RTC_DateStruct)
  {
    if(RTC_DateStruct == NULL)
      return;

    RTC_DateTypeDef tmpDate;
    tmpDate.Date    = RTC_DateStruct->Day;
    tmpDate.WeekDay = RTC_DateStruct->Wday;
    tmpDate.Month   = RTC_DateStruct->Month;
    tmpDate.Year    = RTC_DateStruct->Year + 1970;
    M5_RTC.SetDate( &tmpDate );
  }


  bool BLE_RTC_BM8563::begin(uint8_t sdaPin, uint8_t sclPin)
  {
    M5_RTC.begin();
    return true;
  }


  void BLE_RTC_BM8563::adjust(const DateTime& dt)
  {
    dumpTime("RTC Will adjust from DateTime ", dt.get_tm());
    adjust( dt.get_tm() );
  }


  void BLE_RTC_BM8563::adjust(const time_t& dt)
  {
    tmElements_t dateTimeNow;
    breakTime(dt, dateTimeNow);
    dumpTime("RTC Will adjust from time_t ", dateTimeNow );
    adjust( dateTimeNow );
  }


  uint32_t BLE_RTC_BM8563::unixtime()
  {
    return DateTime::tm2unixtime( now() );
  }


  uint8_t BLE_RTC_BM8563::isrunning(void)
  {
    return M5_RTC.isrunning();
  }


  void BLE_RTC_BM8563::adjust(const tmElements_t& dt)
  {
    SetTime( &dt );
    SetDate( &dt );
  }


  tmElements_t BLE_RTC_BM8563::now()
  {
    tmElements_t thisTime;
    GetTime( thisTime );
    GetDate( thisTime );
    return thisTime;
  }



#else // default to DS1307


  #define DS1307_ADDR 0x68 // I2C address
  #define BLE_RTC BLE_RTC_DS1307 // alias for BLECollector


  class BLE_RTC_DS1307
  {
    public:
      static bool begin(uint8_t sdaPin=SDA, uint8_t sclPin=SCL);
      static void adjust(const tmElements_t& dt);
      static void adjust(const time_t& dt);
      static void adjust(const DateTime& dt);
      uint8_t isrunning(void);
      static tmElements_t now();
      static uint32_t unixtime();
  };

  int ZEROINT = 0;
  /*
  static uint8_t BLE_RTC_bcd2bin (uint8_t val) { return val - 6 * (val >> 4); }
  static uint8_t BLE_RTC_bin2bcd (uint8_t val) { return val + 6 * (val / 10); }
  */

  /** Fonction de conversion BCD -> decimal */
  static uint8_t BLE_RTC_bcd2bin (uint8_t bcd)
  {
    return (bcd / 16 * 10) + (bcd % 16);
  }

/** Fonction de conversion decimal -> BCD */
  static uint8_t BLE_RTC_bin2bcd (uint8_t decimal)
  {
    return (decimal / 10 * 16) + (decimal % 10);
  }


  bool BLE_RTC_DS1307::begin(uint8_t sdaPin, uint8_t sclPin)
  {
    Wire.begin(sdaPin, sclPin);
    return true;
  }
  void BLE_RTC_DS1307::adjust(const DateTime& dt)
  {
    //dumpTime("RTC Will adjust from DateTime ", dt.get_tm());
    adjust( dt.get_tm() );
  }
  void BLE_RTC_DS1307::adjust(const time_t& dt)
  {
    tmElements_t dateTimeNow;
    breakTime(dt, dateTimeNow);
    //dumpTime("RTC Will adjust from time_t ", dateTimeNow );
    adjust( dateTimeNow );
  }
  uint32_t BLE_RTC_DS1307::unixtime()
  {
    return DateTime::tm2unixtime( now() );
  }

  uint8_t BLE_RTC_DS1307::isrunning(void)
  {
    Wire.beginTransmission(DS1307_ADDR);
    Wire.write(ZEROINT);
    Wire.endTransmission();
    Wire.requestFrom(DS1307_ADDR, 1);
    uint8_t ss = Wire.read();
    return !(ss>>7);
  }

  void BLE_RTC_DS1307::adjust(const tmElements_t& dt)
  {
    dumpTime("RTC Will adjust from tmElements_t ", dt );
    Wire.beginTransmission(DS1307_ADDR);
    Wire.write(ZEROINT);
    Wire.write(BLE_RTC_bin2bcd(dt.Second) );
    Wire.write(BLE_RTC_bin2bcd(dt.Minute));
    Wire.write(BLE_RTC_bin2bcd(dt.Hour) ); // mode 24h
    Wire.write(BLE_RTC_bin2bcd(dt.Wday)); // day of week ?
    Wire.write(BLE_RTC_bin2bcd(dt.Day));
    Wire.write(BLE_RTC_bin2bcd(dt.Month));
    Wire.write(BLE_RTC_bin2bcd(tmYearToY2k(dt.Year))); // 2000 to 1970 offset
    Wire.write(ZEROINT);
    if ( Wire.endTransmission() != 0 ) log_e( "[RTC] Unexpected end of transmission" );
  }


  tmElements_t BLE_RTC_DS1307::now()
  {
    tmElements_t tm;
    uint8_t sec;
    Wire.beginTransmission( DS1307_ADDR );
    Wire.write( (int)0 );
    if ( Wire.endTransmission() != 0 ) {
      log_e( "[RTC] Unexpected end of transmission" );
      return tm;
    }
    Wire.requestFrom( DS1307_ADDR, 7 );
    sec = Wire.read();
    tm.Second = BLE_RTC_bcd2bin( sec & 0x7f );
    tm.Minute = BLE_RTC_bcd2bin( Wire.read() );
    tm.Hour   = BLE_RTC_bcd2bin( Wire.read() & 0x3f );  // mask assumes 24hr clock
    tm.Wday   = BLE_RTC_bcd2bin( Wire.read() );
    tm.Day    = BLE_RTC_bcd2bin( Wire.read() );
    tm.Month  = BLE_RTC_bcd2bin( Wire.read() );
    tm.Year   = y2kYearToTm( BLE_RTC_bcd2bin( Wire.read() ) ); // 2000 to 1970 offset
    if ( sec & 0x80 ) log_e("[RTC] Module is DOWN");
    return tm;
  }

#endif
