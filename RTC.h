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

  #define BM8563_ADDR 0x51 // M5Core2 RTC I2C address
  #define BLE_RTC BLE_RTC_BM8563 // alias for BLECollector

  class BLE_RTC_BM8563 {
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

  static uint8_t Bcd2ToByte(uint8_t Value) {
    uint8_t tmp = 0;
    tmp = ((uint8_t)(Value & (uint8_t)0xF0) >> (uint8_t)0x4) * 10;
    return (tmp + (Value & (uint8_t)0x0F));
  }

  static uint8_t ByteToBcd2(uint8_t Value) {
    uint8_t bcdhigh = 0;
    while (Value >= 10) {
      bcdhigh++;
      Value -= 10;
    }
    return  ((uint8_t)(bcdhigh << 4) | Value);
  }

  void BLE_RTC_BM8563::GetTime(tmElements_t &RTC_TimeStruct) {
    uint8_t buf[3] = {0};
    Wire1.beginTransmission(0x51);
    Wire1.write(0x02);
    Wire1.endTransmission();
    Wire1.requestFrom(0x51,3);
    while(Wire1.available()){
      buf[0] = Wire1.read();
      buf[1] = Wire1.read();
      buf[2] = Wire1.read();
    }
    RTC_TimeStruct.Second  = Bcd2ToByte(buf[0]&0x7f);
    RTC_TimeStruct.Minute  = Bcd2ToByte(buf[1]&0x7f);
    RTC_TimeStruct.Hour    = Bcd2ToByte(buf[2]&0x3f);
  }

  void BLE_RTC_BM8563::GetDate(tmElements_t &RTC_DateStruct) {
    uint8_t buf[4] = {0};
    Wire1.beginTransmission(0x51);
    Wire1.write(0x05);
    Wire1.endTransmission();
    Wire1.requestFrom(0x51,4);
    while(Wire1.available()){
        buf[0] = Wire1.read();
        buf[1] = Wire1.read();
        buf[2] = Wire1.read();
        buf[3] = Wire1.read();
    }
    RTC_DateStruct.Day   = Bcd2ToByte(buf[0]&0x3f);
    RTC_DateStruct.Wday  = Bcd2ToByte(buf[1]&0x07);
    RTC_DateStruct.Month = Bcd2ToByte(buf[2]&0x1f);

    RTC_DateStruct.Year = Bcd2ToByte(buf[3]&0xff);

  }

  void BLE_RTC_BM8563::SetTime(const tmElements_t *RTC_TimeStruct) {
    if(RTC_TimeStruct == NULL)
      return;
    Wire1.beginTransmission(0x51);
    Wire1.write(0x02);
    Wire1.write(ByteToBcd2(RTC_TimeStruct->Second));
    Wire1.write(ByteToBcd2(RTC_TimeStruct->Minute));
    Wire1.write(ByteToBcd2(RTC_TimeStruct->Hour));
    Wire1.endTransmission();
  }

  void BLE_RTC_BM8563::SetDate(const tmElements_t *RTC_DateStruct) {

    if(RTC_DateStruct == NULL)
      return;
    Wire1.beginTransmission(0x51);
    Wire1.write(0x05);
    Wire1.write(ByteToBcd2(RTC_DateStruct->Day));
    Wire1.write(ByteToBcd2(RTC_DateStruct->Wday));

    if(RTC_DateStruct->Year < 2000){
      Wire1.write(ByteToBcd2(RTC_DateStruct->Month) | 0x80);
      Wire1.write(ByteToBcd2((uint8_t)(RTC_DateStruct->Year % 100)));
    } else {
      Wire1.write(ByteToBcd2(RTC_DateStruct->Month) | 0x00);
      Wire1.write(ByteToBcd2((uint8_t)(RTC_DateStruct->Year %100)));
    }
    Wire1.endTransmission();
  }

  bool BLE_RTC_BM8563::begin(uint8_t sdaPin, uint8_t sclPin) {
    Wire1.begin(sdaPin, sclPin);
    return true;
  }

  void BLE_RTC_BM8563::adjust(const DateTime& dt) {
    dumpTime("RTC Will adjust from DateTime ", dt.get_tm());
    adjust( dt.get_tm() );
  }

  void BLE_RTC_BM8563::adjust(const time_t& dt) {
    tmElements_t dateTimeNow;
    breakTime(dt, dateTimeNow);
    dumpTime("RTC Will adjust from time_t ", dateTimeNow );
    adjust( dateTimeNow );
  }

  uint32_t BLE_RTC_BM8563::unixtime() {
    return DateTime::tm2unixtime( now() );
  }

  uint8_t BLE_RTC_BM8563::isrunning(void) {
    Wire1.beginTransmission(BM8563_ADDR);
    Wire1.write(0x02);
    Wire1.endTransmission();
    Wire1.requestFrom(BM8563_ADDR, 1);
    uint8_t ss = Wire1.read();
    return !(ss>>7);
  }

  void BLE_RTC_BM8563::adjust(const tmElements_t& dt) {
    SetTime( &dt );
    SetDate( &dt );
  }

  tmElements_t BLE_RTC_BM8563::now() {
    tmElements_t thisTime;
    GetTime( thisTime );
    GetDate( thisTime );
    return thisTime;
  }




#else // default to DS1307


  #define DS1307_ADDR 0x68 // I2C address
  #define BLE_RTC BLE_RTC_DS1307 // alias for BLECollector


  class BLE_RTC_DS1307 {
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
  static uint8_t BLE_RTC_bcd2bin (uint8_t val) { return val - 6 * (val >> 4); }
  static uint8_t BLE_RTC_bin2bcd (uint8_t val) { return val + 6 * (val / 10); }


  bool BLE_RTC_DS1307::begin(uint8_t sdaPin, uint8_t sclPin) {
    Wire.begin(sdaPin, sclPin);
    return true;
  }
  void BLE_RTC_DS1307::adjust(const DateTime& dt) {
    //dumpTime("RTC Will adjust from DateTime ", dt.get_tm());
    adjust( dt.get_tm() );
  }
  void BLE_RTC_DS1307::adjust(const time_t& dt) {
    tmElements_t dateTimeNow;
    breakTime(dt, dateTimeNow);
    //dumpTime("RTC Will adjust from time_t ", dateTimeNow );
    adjust( dateTimeNow );
  }
  uint32_t BLE_RTC_DS1307::unixtime() {
    return DateTime::tm2unixtime( now() );
  }
  uint8_t BLE_RTC_DS1307::isrunning(void) {
    Wire.beginTransmission(DS1307_ADDR);
    Wire.write(ZEROINT);
    Wire.endTransmission();
    Wire.requestFrom(DS1307_ADDR, 1);
    uint8_t ss = Wire.read();
    return !(ss>>7);
  }
  void BLE_RTC_DS1307::adjust(const tmElements_t& dt) {
    //dumpTime("RTC Will adjust from tmElements_t ", dt );
    Wire.beginTransmission(DS1307_ADDR);
    Wire.write(ZEROINT);
    Wire.write(BLE_RTC_bin2bcd(dt.Second));
    Wire.write(BLE_RTC_bin2bcd(dt.Minute));
    Wire.write(BLE_RTC_bin2bcd(dt.Hour));
    Wire.write(BLE_RTC_bin2bcd(0));
    Wire.write(BLE_RTC_bin2bcd(dt.Day));
    Wire.write(BLE_RTC_bin2bcd(dt.Month));
    Wire.write(BLE_RTC_bin2bcd(dt.Year)); // 2000 to 1970 offset
    Wire.write(ZEROINT);
    Wire.endTransmission();
  }
  tmElements_t BLE_RTC_DS1307::now() {
    Wire.beginTransmission(DS1307_ADDR);
    Wire.write(ZEROINT);
    Wire.endTransmission();
    Wire.requestFrom(DS1307_ADDR, 7);
    uint8_t ss = BLE_RTC_bcd2bin(Wire.read() & 0x7F);
    uint8_t mm = BLE_RTC_bcd2bin(Wire.read());
    uint8_t hh = BLE_RTC_bcd2bin(Wire.read());
    Wire.read();
    uint8_t d = BLE_RTC_bcd2bin(Wire.read());
    uint8_t m = BLE_RTC_bcd2bin(Wire.read());
    uint8_t y = BLE_RTC_bcd2bin(Wire.read()); // 2000 to 1970 offset
    return tmElements_t {ss, mm, hh, 0, d, m, y};
  }

#endif
