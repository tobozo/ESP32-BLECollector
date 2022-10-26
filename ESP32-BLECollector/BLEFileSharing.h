/*

  ESP32 BLE Collector - A BLE scanner with sqlite data persistence on the SD Card
  Source: https://github.com/tobozo/ESP32-BLECollector

  MIT License

  Copyright (c) 2019 tobozo

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

#pragma GCC diagnostic ignored "-Wunused-function"


#define TICKS_TO_DELAY 1000

static BLEUUID timeServiceUUID(        (uint16_t)0x1805 ); // gatt "Current Time Service", "org.bluetooth.service.current_time"
static BLEUUID timeCharacteristicUUID( (uint16_t)0x2a2b ); // gatt "Current Time", "org.bluetooth.characteristic.current_time"

BLEServer*         TimeSharingServer;
BLEClient*         TimeSharingClient;
BLEService*        TimeSharingService;
BLERemoteService*  TimeSharingRemoteService;
BLEAdvertising*    TimeSharingAdvertising;
BLECharacteristic* TimeServerChar;
BLERemoteCharacteristic* TimeRemoteChar;

std::string timeServerBLEAddress;

uint8_t timeServerClientType;


typedef struct
{
  uint16_t year;
  uint8_t  month;
  uint8_t  day;
  uint8_t  hour;
  uint8_t  minutes;
  uint8_t  seconds;
  uint8_t  wday;
  uint8_t  fraction;
  uint8_t  adjust = 0;
  uint8_t  tz;
} bt_time_t;

bt_time_t BLERemoteTime;
bt_time_t BLELocalTime;


static bool timeServerIsRunning = false;
static bool timeServerStarted = false;
static bool TimeServerSignalSent = false;
static bool timeClientisRunning = false;
static bool timeClientisStarted = false;



/******************************************************
  BLE Time Client methods
******************************************************/


static void setBLETime()
{
  DateTime UTCTime   = DateTime(BLERemoteTime.year, BLERemoteTime.month, BLERemoteTime.day, BLERemoteTime.hour, BLERemoteTime.minutes, BLERemoteTime.seconds);
  DateTime LocalTime = UTCTime.unixtime() + BLERemoteTime.tz * 3600;

  dumpTime("UTC:", UTCTime);
  dumpTime("Local:", LocalTime);

  setTime( LocalTime.unixtime() );

  timeval epoch = {(time_t)LocalTime.unixtime(), 0};
  const timeval *tv = &epoch;
  settimeofday(tv, NULL);

  struct tm now;
  getLocalTime(&now,0);

  Serial.printf("[Heap: %06d] Time has been set to: %04d-%02d-%02d %02d:%02d:%02d\n",
   freeheap,
   LocalTime.year(),
   LocalTime.month(),
   LocalTime.day(),
   LocalTime.hour(),
   LocalTime.minute(),
   LocalTime.second()
  );
#if HAS_EXTERNAL_RTC
  RTC.adjust(LocalTime);
#endif
  logTimeActivity(SOURCE_BLE, LocalTime.unixtime() );
  lastSyncDateTime = LocalTime;
  timeHousekeeping();
  HasBTTime = true;
  DayChangeTrigger = true;
  TimeIsSet = true;
}

/*
static void TimeClientNotifyCallback( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify )
{
  //pBLERemoteCharacteristic->getHandle();
  log_w("Received time");
  memcpy( &BLERemoteTime, pData, length );
  setBLETime();
};
*/

class TimeClientCallbacks : public BLEClientCallbacks
{
    void onConnect(BLEClient* pC) {
      log_w("[Heap: %06d] Connect!!", freeheap);
    }
    void onDisconnect(BLEClient* pC) {
      if ( !HasBTTime ) {
        foundTimeServer = false;
        log_w("[Heap: %06d] Disconnect without time!!", freeheap);
        // oh that's dirty
        //ESP.restart();
      } else {
        foundTimeServer = true;
        log_w("[Heap: %06d] Disconnect with time!!", freeheap);
      }
    }
    /*
    void onNotify(BLECLient *pC) {
      log_w("Received time");
      memcpy( &BLERemoteTime, pC->getRemoteCharacteristic()->getData(), pC->getRemoteCharacteristic()->getDataLength() );
      setBLETime();
    }
    */
};

TimeClientCallbacks *TimeClientCallback;


static void stopTimeClient()
{
  if ( TimeSharingClient != NULL ) {
    if ( TimeSharingClient->isConnected() ) {
      TimeSharingClient->disconnect();
    }
  }
  foundTimeServer = false;
  timeClientisRunning = false;
}


static void TimeClientTask( void * param )
{
  timeClientisRunning = true;
  if ( TimeSharingClient == NULL ) {
    TimeSharingClient = BLEDevice::createClient();
  }
  if ( TimeClientCallback == NULL ) {
    TimeClientCallback = new TimeClientCallbacks();
  }
  TimeSharingClient->setClientCallbacks( TimeClientCallback );

  HasBTTime = false;
  log_w("[Heap: %06d] Will connect to address %s", freeheap, timeServerBLEAddress.c_str());
  if ( !TimeSharingClient->connect( timeServerBLEAddress, timeServerClientType ) ) {
    log_e("[Heap: %06d] Failed to connect to address %s", freeheap, timeServerBLEAddress.c_str());
    stopTimeClient();
    vTaskDelete( NULL ); return;
  }
  log_w("[Heap: %06d] Connected to address %s", freeheap, timeServerBLEAddress.c_str());
  TimeSharingRemoteService = TimeSharingClient->getService( timeServiceUUID );
  if (TimeSharingRemoteService == nullptr) {
    log_e("Failed to find our service UUID: %s", timeServiceUUID.toString().c_str());
    stopTimeClient();
    vTaskDelete( NULL ); return;
  }
  TimeRemoteChar = TimeSharingRemoteService->getCharacteristic( timeCharacteristicUUID );
  if (TimeRemoteChar == nullptr) {
    log_e("Failed to find our characteristic timeCharacteristicUUID: %s, disconnecting", timeCharacteristicUUID.toString().c_str());
    stopTimeClient();
    vTaskDelete( NULL ); return;
  }
  log_w("[Heap: %06d] registering for notification", freeheap);
  TimeRemoteChar->subscribe( true/*TimeClientNotifyCallback*/ );
  TickType_t last_wake_time;
  last_wake_time = xTaskGetTickCount();

  while (TimeSharingClient->isConnected()) {
    vTaskDelayUntil(&last_wake_time, TICKS_TO_DELAY / portTICK_PERIOD_MS);
    // TODO: max wait time before force exit
    if ( HasBTTime ) {
      break;
    }
  }
  log_w("[Heap: %06d] client disconnected", freeheap);
  stopTimeClient();
  vTaskDelete( NULL );
}




/******************************************************
  BLE Time Server methods
******************************************************/


class TimeServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer, ble_gap_conn_desc *param)
  {
      TimeServerSignalSent = false;
      BLEDevice::getAdvertising()->stop();
  }
  void onDisconnect(BLEServer *pServer) {
      BLEDevice::startAdvertising();
      //TimeServerSignalSent = true;
  }
};

TimeServerCallbacks *TimeServerCallback;


static void stopTimeServer()
{
  TimeSharingAdvertising->stop();
  timeServerIsRunning = false;
  timeServerStarted   = false;
  log_w("Stopped time server");
}


uint8_t* getBLETime()
{
  DateTime LocalTime, UTCTime;
  //struct timeval tv;
  //struct tm* _t;
  // because it's not enough maintaining those:
  //   1) internal rtc clock
  //   2) external rtc clock
  //   3) external gps clock
  // let's use the ESP32 recommended example and throw an extra snpm clock ?
  // ... nah, fuck this
  // gettimeofday(&tv, nullptr);
  // _t = localtime(&(tv.tv_sec));
  LocalTime = DateTime(year(), month(), day(), hour(), minute(), second());
  UTCTime   = LocalTime.unixtime() - (int(timeZone*100)*36) -  + (summerTime ? 3600 : 0);
  BLELocalTime.year     = UTCTime.year();   // 1900 + _t->tm_year;
  BLELocalTime.month    = UTCTime.month();  // _t->tm_mon + 1;
  BLELocalTime.wday     = 0;        // _t->tm_wday == 0 ? 7 : _t->tm_wday;
  BLELocalTime.day      = UTCTime.day();    // _t->tm_mday;
  BLELocalTime.hour     = UTCTime.hour();   // _t->tm_hour;
  BLELocalTime.minutes  = UTCTime.minute(); // _t->tm_min;
  BLELocalTime.seconds  = UTCTime.second(); // _t->tm_sec;
  BLELocalTime.fraction = 0;        // tv.tv_usec * 256 / 1000000;
  BLELocalTime.tz       = timeZone; // wat
  return (uint8_t*)&BLELocalTime;
}




static void TimeServerTaskNotify( void * param )
{
  TickType_t lastWaketime;
  lastWaketime = xTaskGetTickCount();
  timeServerStarted = true;
  while ( !TimeServerSignalSent ) {
    TimeServerChar->setValue( getBLETime(), sizeof(bt_time_t));
    TimeServerChar->notify();
    // send notification with date/time exactly every TICKS_TO_DELAY ms
    vTaskDelayUntil(&lastWaketime, TICKS_TO_DELAY / portTICK_PERIOD_MS);
  }
  timeServerStarted = false;
  vTaskDelete(NULL);
}



static void TimeServerTask( void * param )
{
  Serial.println("Starting BLE Time Server");

  BLEDevice::setMTU(50);

  log_w("MTU set");

  TimeServerCallback = new TimeServerCallbacks();

  if ( TimeSharingAdvertising == NULL ) {
    TimeSharingAdvertising = BLEDevice::getAdvertising();
  }
  TimeSharingServer = BLEDevice::createServer();

  TimeSharingServer->setCallbacks( TimeServerCallback );

  TimeSharingService = TimeSharingServer->createService( timeServiceUUID );

  TimeServerChar = TimeSharingService->createCharacteristic(
    timeCharacteristicUUID,
    NIMBLE_PROPERTY::READ  |
    NIMBLE_PROPERTY::NOTIFY
  );

  //log_w("Will create descriptor");
  //TimeServerChar->createDescriptor("2902" /** , NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE **/);
  //log_w("Descriptor created");

  TimeSharingService->start();

  TimeSharingAdvertising->addServiceUUID( timeServiceUUID );
  TimeSharingAdvertising->setMinInterval( 0x100 );
  TimeSharingAdvertising->setMaxInterval( 0x200 );
  log_w("Starting advertising");
  BLEDevice::startAdvertising();
  log_w("TimeServer Advertising started");

  TimeServerSignalSent = false;

  xTaskCreate( TimeServerTaskNotify, "TimeServerTaskNotify", 2560, NULL, 6, NULL );

  while ( timeServerIsRunning ) {
    if( TimeServerSignalSent ) {
      break;
    } else {
      vTaskDelay(100);
    }
  }
  stopTimeServer();
  vTaskDelete( NULL );
}

