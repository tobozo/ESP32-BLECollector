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

#define TICKS_TO_DELAY 1000

// French "StopCodid" app's Service UUID
// TODO: add more of those
//static BLEUUID StopCovidServiceUUID("910c7798-9f3a-11ea-bb37-0242ac130002"); // test UUID

static BLEUUID Covid19RadarTestesUUID("7822fa0f-ce38-48ea-a7e8-e72af4e42c1c"); // https://github.com/Covid-19Radar/Covid19Radar/blob/master/doc/Tester/Tester-Instructions.md
static BLEUUID Covid19RadarUUID("550e8400-e29b-41d4-a716-446655440000"); // International Covid19 Radar contact tracing app

static BLEUUID StopCovidServiceUUID("0000fd64-0000-1000-8000-00805f9b34fb"); // French stop covid app
static BLEUUID StopCovidCharUUID("a8f12d00-ee67-478b-b95f-65d599407756");




static BLEUUID FileSharingServiceUUID( "f59f6622-1540-0001-8d71-362b9e155667" ); // generated UUID for the service
static BLEUUID FileSharingWriteUUID(   "f59f6622-1540-0002-8d71-362b9e155667" ); // characteristic to write file_chunk locally
static BLEUUID FileSharingRouteUUID(   "f59f6622-1540-0003-8d71-362b9e155667" ); // characteristic to manage routing
static BLEUUID timeServiceUUID(        (uint16_t)0x1805 ); // gatt "Current Time Service", "org.bluetooth.service.current_time"
static BLEUUID timeCharacteristicUUID( (uint16_t)0x2a2b ); // gatt "Current Time", "org.bluetooth.characteristic.current_time"

BLEServer*         TimeSharingServer;
BLEServer*         FileSharingServer;

BLEClient*         TimeSharingClient;
BLEClient*         FileSharingClient;

BLEService*        TimeSharingService;
BLEService*        FileSharingService;

BLERemoteService*  BLESharingRemoteService;
BLERemoteService*  TimeSharingRemoteService;

BLEAdvertising*    FileSharingAdvertising;
BLEAdvertising*    TimeSharingAdvertising;

BLECharacteristic* FileSharingWriteChar;
BLECharacteristic* FileSharingRouteChar;
BLECharacteristic* TimeServerChar;

BLERemoteCharacteristic* FileSharingReadRemoteChar;
BLERemoteCharacteristic* FileSharingRouterRemoteChar;
BLERemoteCharacteristic* TimeRemoteChar;

//BLE2902* BLESharing2902Descriptor;

std::string timeServerBLEAddress;
std::string fileServerBLEAddress;

uint8_t timeServerClientType;
uint8_t fileServerClientType;

typedef struct {
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

static File   FileReceiver;
//static int    binary_file_length = 0;
static size_t FileReceiverExpectedSize = 0;
static size_t FileReceiverReceivedSize = 0;
static size_t FileReceiverProgress = 0;

static bool isFileSharingClientConnected = false;
static bool fileSharingServerTaskIsRunning = false;
static bool fileSharingServerTaskShouldStop = false;
static bool fileSharingSendFileError = false;
static bool fileSharingClientTaskIsRunning = false;
static bool fileSharingClientStarted = false;
static bool timeServerIsRunning = false;
static bool timeServerStarted = false;
static bool TimeServerSignalSent = false;
static bool timeClientisRunning = false;
static bool timeClientisStarted = false;
static bool fileSharingEnabled = false;
static bool fileDownloadingEnabled = false;
static bool lsDone = false;

static bool checkVendorResponded = false;
static int checkVendorResponse = 0;
static bool checkMacResponded = false;
static int checkMacResponse = 0;

const char* sizeMarker             = "size:";
const char* dateTimeMarker         = "dateTime:";
const char* dateTimeMarkerTpl      = "dateTime:%04d-%02d-%02d %02d:%03d:%03d %d %d %d %d";
const char* closeMessage           = "close";
const char* restartMessage         = "restart";
const char* lsMessage              = "ls";
const char* lsDoneMessage          = "lsdone";
const char* checkVendorFileMessage = "checkBLEOUI";
const char* checkMacFileMessage    = "checkMACOUI";
const char* fileMarker             = "file:";


/******************************************************
  BLE Time Client methods
******************************************************/


static void setBLETime() {
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


static void TimeClientNotifyCallback( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify ) {
  //pBLERemoteCharacteristic->getHandle();
  log_w("Received time");
  memcpy( &BLERemoteTime, pData, length );
  setBLETime();
};

class TimeClientCallbacks : public BLEClientCallbacks {
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
};

TimeClientCallbacks *TimeClientCallback;


static void stopTimeClient() {
  if ( TimeSharingClient != NULL ) {
    if ( TimeSharingClient->isConnected() ) {
      TimeSharingClient->disconnect();
    }
  }
  foundTimeServer = false;
  timeClientisRunning = false;
}


static void TimeClientTask( void * param ) {
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
  TimeRemoteChar->registerForNotify( TimeClientNotifyCallback );
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


class TimeServerCallbacks : public BLEServerCallbacks {
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


static void stopTimeServer() {
  TimeSharingAdvertising->stop();
  timeServerIsRunning = false;
  timeServerStarted   = false;
  log_w("Stopped time server");
}


uint8_t* getBLETime() {
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
  UTCTime   = LocalTime.unixtime() - timeZone * 3600;
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




static void TimeServerTaskNotify( void * param ) {
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



static void TimeServerTask( void * param ) {
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

  log_w("Will create descriptor");
  //TimeServerChar->createDescriptor("2902" /** , NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE **/);
  log_w("Descriptor created");

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





/******************************************************
  BLE File Receiver Methods
******************************************************/


byte receivedFiles = 0;


void FileSharingReceiveFile( const char* filename ) {
  FileReceiverReceivedSize = 0;
  FileReceiverProgress = 0;
  log_w("Will create %s", filename);
  FileReceiver = BLE_FS.open( filename, FILE_WRITE );
  // receivedFiles
  if( FileReceiverExpectedSize == FileReceiver.size() ) {
    log_w("Files are identical, transferring is useless");
  }
  if ( !FileReceiver ) {
    log_e("Failed to create %s", filename);
  }
  log_d("Successfully opened %s for writing", filename);
}


void FileSharingCloseFile() {
  if ( !FileReceiver ) {
    log_e("Nothing to close!");
    return;
  }
  takeMuxSemaphore();
  const char* filename = FileReceiver.name(); // store filename for reopening
  FileReceiver.close();
  FileReceiver = BLE_FS.open( filename ); // r/w mode gives bogus size, reopen r/o

  if ( FileReceiverReceivedSize != FileReceiverExpectedSize ) {
    log_e("Total size != expected size ( %d != %d )", FileReceiver.size(), FileReceiverExpectedSize);
    Out.println( "Copy Failed, please try again." );
    FileReceiver.close();
    BLE_FS.remove( filename );
  } else {
    FileReceiver.close();
    Out.println( "Copy successful!" );
  }
  giveMuxSemaphore();
  //TODO: sha256_sum
  FileReceiverExpectedSize = 0;
  FileReceiverReceivedSize = 0;
  FileReceiverProgress = 0;
}


class FileSharingWriteCallbacks : public BLECharacteristicCallbacks {
    void onWrite( BLECharacteristic* WriterAgent ) {
      size_t len = WriterAgent->getDataLength();
      if ( FileReceiverExpectedSize == 0 ) {
        // no size was previously sent, can't calculate
        log_e("Ignored %d bytes", len);
        return;
      }
      size_t progress = (((float)FileReceiverReceivedSize / (float)FileReceiverExpectedSize) * 100.00);
      if ( FileReceiver ) {
        FileReceiver.write( (const uint8_t*)(WriterAgent->getValue().c_str()), len );
        log_w("Wrote %d bytes", len);
        FileReceiverReceivedSize += len;
      } else {
        // file write problem ?
        log_e("Ignored %d bytes", len);
      }
      if ( FileReceiverProgress != progress ) {
        FileReceiverProgress = progress;
      }
    }
};


class FileSharingRouteCallbacks : public BLECharacteristicCallbacks {
    void onWrite( BLECharacteristic* RouterAgent ) {
      size_t strLenRouting = RouterAgent->getDataLength();

      if( strLenRouting < 1 ) {
        log_e("getDataLength() == 0");
        return;
      }

      /*
      char routing[strLenRouting+1] = {0};
      memcpy( &routing, RouterAgent->getData(), strLenRouting+1 );
      routing[strLenRouting]= '\0'; // null terminate
      */
      //std::string dscVal((char*)RouterAgent->getValue(), RouterAgent->getDataLength());

      const char* routing = RouterAgent->getValue().c_str();

      log_w("Received copy routing query (len=%d): %s", strLenRouting, routing);

      if ( strstr(routing, sizeMarker) ) { // messages starting with "size:"
        log_w("Received sizeMarker");
        if ( strLenRouting > strlen( sizeMarker ) ) {
          char* lenStr = substr( routing, strlen(sizeMarker), strLenRouting - strlen(sizeMarker) );
          FileReceiverExpectedSize = atoi( lenStr );
          log_w( "Assigned size_t %d", FileReceiverExpectedSize );
          free( lenStr );
        }
      } else if( strstr(routing, dateTimeMarker ) ) {
        log_w("Received dateTimeMarker");
        if ( strlen( routing ) > strlen( dateTimeMarker ) ) {
          char* lenStr = substr( routing, strlen(dateTimeMarker), strlen(routing) - strlen(dateTimeMarker) );
          log_w("Decoding time");
/*
          char _dtMarker[20];
          int year, month, day, hour, minutes, seconds, wday, fraction, adjust, tz;
          //const char* myMarker = dateTimeMarker "%d-%d-%d %d:%d:%d %d %d %d %d";
          int res = sscanf( routing, dateTimeMarker,
            &_dtMarker,
            &year,
            &month,
            &day,
            &hour,
            &minutes,
            &seconds,
            &wday,
            &fraction,
            &adjust,
            &tz
          );
*/

          char* timestampChar = new char( strlen( lenStr ) +1 );
          memcpy( &timestampChar, lenStr, strlen( lenStr ) );
          timestampChar[strlen( lenStr )] = '\0';

          Serial.printf("Copied routing: %s\n", timestampChar );

          struct tm time;
          strptime(routing, "dateTime:%Y-%m-%d %H:%M:%S %w 0 0 %z", &time);
          time_t loctime = mktime( &time );
          struct tm * now = localtime( & loctime );
          BLERemoteTime.year     = now->tm_year+1900;
          BLERemoteTime.month    = now->tm_mon+1;
          BLERemoteTime.day      = now->tm_mday;
          BLERemoteTime.hour     = now->tm_hour;
          BLERemoteTime.minutes  = now->tm_min;
          BLERemoteTime.seconds  = now->tm_sec;
          BLERemoteTime.wday     = now->tm_wday;
          //BLERemoteTime.fraction = 0;
          //BLERemoteTime.adjust   = 0;
          //BLERemoteTime.tz       = time.__tm_zone;

/*
          Serial.printf("scanf res=%d (scanned: %s)----- \n", res, routing);

          Serial.printf( dateTimeMarkerTpl,
            "sscanf result=",
            year,
            month,
            day,
            hour,
            minutes,
            seconds,
            wday,
            fraction,
            adjust,
            tz
          );
          Serial.println();

          BLERemoteTime.year     = year;
          BLERemoteTime.month    = month;
          BLERemoteTime.day      = day;
          BLERemoteTime.hour     = hour;
          BLERemoteTime.minutes  = minutes;
          BLERemoteTime.seconds  = seconds;
          BLERemoteTime.wday     = wday;
          BLERemoteTime.fraction = fraction;
          BLERemoteTime.adjust   = adjust;
          BLERemoteTime.tz       = tz;

          //memcpy( &BLERemoteTime, lenStr, strlen( lenStr ) );*/
          setBLETime();
          free( lenStr );
        }
      } else if ( strcmp( routing, closeMessage ) == 0 ) { // file end
        log_w("Closing file");
        FileSharingCloseFile();
      } else if ( strcmp( routing, restartMessage ) == 0 ) { // transfert finished
        ESP.restart();
      } else if ( strcmp( routing, checkVendorFileMessage ) == 0 ) {
        if( !DB.checkVendorFile() ) {
          log_w("Asking for Vendor DB update (%s)", BLE_VENDOR_NAMES_DB_FS_PATH);
          RouterAgent->setValue( (uint8_t*)(BLE_VENDOR_NAMES_DB_FS_PATH), strlen(BLE_VENDOR_NAMES_DB_FS_PATH));
          RouterAgent->notify();
          log_w("Notification sent!");
        } else {
          const char* resp = String( String(checkVendorFileMessage) + String("0") ).c_str();
          log_w("Vendor DB file is fine, will send resp: %s", resp);
          RouterAgent->setValue( (uint8_t*)resp, strlen(resp) );
          RouterAgent->notify();
        }
      } else if ( strcmp( routing, checkMacFileMessage ) == 0 ) {
        if( !DB.checkOUIFile() ) {
          log_w("Asking for OUI DB update ( %s )", MAC_OUI_NAMES_DB_FS_PATH);
          RouterAgent->setValue( (uint8_t*)(MAC_OUI_NAMES_DB_FS_PATH), strlen(MAC_OUI_NAMES_DB_FS_PATH));
          RouterAgent->notify();
          log_w("Notification sent!");
        } else {
          const char* resp = String( String(checkMacFileMessage) + String("0") ).c_str();
          log_w("OUI DB file is fine, will send resp: %s", resp);
          RouterAgent->setValue( (uint8_t*)resp, strlen(resp) );
          RouterAgent->notify();
        }
      } else if (strcmp(routing, lsMessage ) == 0) {
        int filesCount = 0;
        char strOut[32+64];
        File root = BLE_FS.open("/");
        if( root && root.isDirectory() ) {
          File file = root.openNextFile();
          while(file) {
            if(!file.isDirectory()) {
              if( String( file.name() ).endsWith(".db") ) {
                *strOut = {0};
                sprintf( strOut, "%s%s;%d", fileMarker, file.name(), file.size() );
                log_w("Sending ls response[%d]: %s%s;%d", filesCount++, fileMarker, file.name(), file.size());
                RouterAgent->setValue( (uint8_t*)strOut, strlen(strOut));
                RouterAgent->notify();
                if( filesCount > 30 ) break;
              }
            }
            file.close();
            file = root.openNextFile();
          }
        }
        RouterAgent->setValue( (uint8_t*)lsDoneMessage, strlen(lsDoneMessage));
        RouterAgent->notify();
      } else { // filenames
        if ( strcmp( routing, BLE_VENDOR_NAMES_DB_FS_PATH ) == 0 ) {
          log_w( "FileSharingReceiveFile( %s )", BLE_VENDOR_NAMES_DB_FS_PATH );
          FileSharingReceiveFile( BLE_VENDOR_NAMES_DB_FS_PATH );
        } else  if ( strcmp( routing, MAC_OUI_NAMES_DB_FS_PATH ) == 0 ) {
          log_w( "FileSharingReceiveFile( %s )", MAC_OUI_NAMES_DB_FS_PATH );
          FileSharingReceiveFile( MAC_OUI_NAMES_DB_FS_PATH);
        } else {
          log_e( "No filename matching !");
        }
      }
      takeMuxSemaphore();
      Out.println( routing );
      Out.println();
      giveMuxSemaphore();

    }
};


class FileSharingCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *SharingServer, ble_gap_conn_desc *param)
    {
      log_w("A client is connected, stopping advertising");
      isFileSharingClientConnected = true;
      UI.headerStats("Connected :-)");
      takeMuxSemaphore();
      Out.println( "Client connected!" );
      Out.println();
      giveMuxSemaphore();
      BLEDevice::getAdvertising()->stop();
      // do some voodo on remote MTU for transfert perfs (thanks @chegewara)
      //esp_ble_gap_set_prefer_conn_params(param->connect.remote_bda, 6, 6, 0, 500);
      //SharingServer->updatePeerMTU( SharingServer->getConnId(), 500 );
    }
    void onDisconnect(BLEServer* SharingServer) {
      log_w("A client disconnected, restarting advertising");
      isFileSharingClientConnected = false;
      UI.headerStats("Advertising (_x_)");
      takeMuxSemaphore();
      Out.println( "Client disconnected" );
      Out.println();
      giveMuxSemaphore();
      //BLEDevice::startAdvertising();
      fileSharingServerTaskShouldStop = true;
    }
};


FileSharingCallbacks *FileSharingCallback;
FileSharingRouteCallbacks *FileSharingRouteCallback;
FileSharingWriteCallbacks *FileSharingWriteCallback;

void stopFileSharingServer() {
  FileSharingAdvertising->stop();
  fileSharingServerTaskIsRunning = false;
  fileSharingServerTaskShouldStop = false;
  fileDownloadingEnabled = false;
  receivedFiles = 0;
}

// server as a slave service: wait for an upload signal
static void FileSharingServerTask(void* p) {

  BLEDevice::setMTU(517);

  if ( FileSharingAdvertising == NULL ) {
    FileSharingAdvertising = BLEDevice::getAdvertising();
  }
  if ( FileSharingCallback == NULL ) {
    FileSharingCallback = new FileSharingCallbacks();
  }
  if ( FileSharingRouteCallback == NULL ) {
    FileSharingRouteCallback = new FileSharingRouteCallbacks();
  }
  if ( FileSharingWriteCallback == NULL ) {
    FileSharingWriteCallback = new FileSharingWriteCallbacks();
  }
  if ( FileSharingServer == NULL ) {
    FileSharingServer = BLEDevice::createServer();
  }
  FileSharingServer->setCallbacks( FileSharingCallback );
  FileSharingService = FileSharingServer->createService( FileSharingServiceUUID );

  FileSharingWriteChar = FileSharingService->createCharacteristic(
    FileSharingWriteUUID,
    NIMBLE_PROPERTY::WRITE_NR
  );
  FileSharingRouteChar = FileSharingService->createCharacteristic(
    FileSharingRouteUUID,
    NIMBLE_PROPERTY::NOTIFY |
    NIMBLE_PROPERTY::READ |
    NIMBLE_PROPERTY::WRITE
  );

  FileSharingRouteChar->setCallbacks( FileSharingRouteCallback );
  FileSharingWriteChar->setCallbacks( FileSharingWriteCallback );

  //BLE2902* pRoute2902 = (BLE2902*)FileSharingRouteChar->createDescriptor("2902", NIMBLE_PROPERTY::NOTIFY/** | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE **/);
  //pRoute2902->setCallbacks(FileSharingRouteCallback);

  FileSharingService->start();

  FileSharingAdvertising->addServiceUUID( FileSharingServiceUUID );
  FileSharingAdvertising->setMinInterval(0x100);
  FileSharingAdvertising->setMaxInterval(0x200);

  BLEDevice::startAdvertising();

  Serial.println("FileSharingServerTask up an advertising");
  UI.headerStats("Advertising (_x_)");
  takeMuxSemaphore();
  Out.println();
  Out.println( "Waiting for a BLE peer to send the files" );
  Out.println();
  giveMuxSemaphore();

  size_t progress = 0;

  while (1) {
    if ( FileReceiverProgress != progress ) {
      UI.PrintProgressBar( (Out.width * FileReceiverProgress) / 100 );
      progress = FileReceiverProgress;
      //vTaskDelay(10);
    }
    if ( fileSharingServerTaskShouldStop ) { // stop signal from outside the task
      stopFileSharingServer();
      vTaskDelete( NULL );
    } else {
      vTaskDelay(10);
    }
  }
} // FileSharingServerTask








/******************************************************
  BLE File Sender Methods
******************************************************/

bool fileTransferInProgress = false;
unsigned long fileSharingClientLastActivity = millis();
unsigned long fileSharingClientTimeout = 10000;
char myDateTimeMarker[50] = {0};
//char dateTimeAsChar[sizeof(bt_time_t)+1] = {0};

void FileSharingSendFile( BLERemoteCharacteristic* RemoteChar, const char* filename ) {
  while( fileTransferInProgress ) {
    log_w("Waiting for current transfert to finish");
    vTaskDelay( 1000 );
  }

  /*
  // send local datetime as a binary string
  uint8_t* dateTime = getBLETime();
  sprintf( myDateTimeMarker, dateTimeMarkerTpl,
    BLELocalTime.year,
    BLELocalTime.month,
    BLELocalTime.day,
    BLELocalTime.hour,
    BLELocalTime.minutes,
    BLELocalTime.seconds,
    BLELocalTime.wday,
    BLELocalTime.fraction,
    BLELocalTime.adjust,
    BLELocalTime.tz
  );
  log_w("RemoteChar->writevalue(%d bytes): \"%s\"", strlen(myDateTimeMarker), myDateTimeMarker );
  //sprintf( myDateTimeMarker, "%s%s", dateTimeMarker, (const char*)dateTime );
  //log_w("RemoteChar->writevalue( %s, %d )", (const char*)myDateTimeMarker, strlen(myDateTimeMarker)+1 );
  if( !RemoteChar->writeValue( (uint8_t*)myDateTimeMarker, strlen(myDateTimeMarker)+1 ) ) {
    log_e("Failed to send datetime");
    fileSharingSendFileError = true;
    return;
  }
  log_w("Time sent !");
  vTaskDelay( 200 );
*/

  log_w("RemoteChar->writeValue( %s,  %d)", filename, strlen(filename)+1 );
  if ( !RemoteChar->writeValue((uint8_t*)filename, strlen(filename)+1, false) ) {
    log_e("Remote is unable to comply to %s filename query", filename);
    return;
  }
  log_w("Filename sent !");
  vTaskDelay( 200 );

  fileSharingSendFileError = false;
  fileTransferInProgress = true;
  File fileToTransfer = BLE_FS.open( filename );

  if ( !fileToTransfer ) {
    log_e("Can't open %s for reading", filename);
    fileSharingSendFileError = true;
    fileTransferInProgress = false;
    return;
  }
  size_t totalsize = fileToTransfer.size();
  size_t progress = totalsize;

  // send file size as string
  char myTotalSize[32];
  sprintf( myTotalSize, "%s%d", sizeMarker, totalsize );
  log_w("RemoteChar->writevalue( myTotalSize: '%s' )", (const char*)myTotalSize);
  if( !RemoteChar->writeValue((uint8_t*)myTotalSize, strlen(myTotalSize)+1, false) ) {
    log_e("Failed to send file size");
    fileSharingSendFileError = true;
    fileTransferInProgress = false;
    fileToTransfer.close();
    return;
  }
  log_w("Size sent !");

  //vTaskDelay( 1000 );


  #define BLE_FILECOPY_BUFFSIZE 512
  uint8_t buff[BLE_FILECOPY_BUFFSIZE];
  uint32_t len = fileToTransfer.read( buff, BLE_FILECOPY_BUFFSIZE ); // fill buffer
  log_w("Starting transfert...");
  UI.headerStats(filename);
  UI.PrintProgressBar( 0 );
  int lastpercent = 0;

  if( !FileSharingReadRemoteChar->canWriteNoResponse() ) {
    log_w("FileSharingReadRemoteChar can't WRITE_NORESPONSE");
  }

  while ( len > 0 ) {
    fileSharingClientLastActivity = millis();
    progress -= len;
    int percent = 100 - (((float)progress / (float)totalsize) * 100.00);
    if ( !FileSharingReadRemoteChar->writeValue((uint8_t*)&buff, len, false) ) {
      // transfert failed !
      log_e("Failed to send %d bytes (%d percent done) %d / %d", len, percent, progress, totalsize);
      fileSharingSendFileError = true;
      break;
    } else {
      log_v("SUCCESS sending %d bytes (%d percent done) %d / %d", len, percent, progress, totalsize);
    }
    if ( lastpercent != percent ) {
      UI.PrintProgressBar( (Out.width * percent) / 100 );
      lastpercent = percent;
      vTaskDelay(10);
    }
    len = fileToTransfer.read( buff, BLE_FILECOPY_BUFFSIZE );
    vTaskDelay(10);
  }
  UI.PrintProgressBar( 0 );
  UI.headerStats("[OK]");
  if( fileSharingSendFileError ) {
    log_e("Transfer aborted!");
  } else {
    log_w("Transfer finished!");
  }
  fileToTransfer.close();

  if( !RemoteChar->writeValue((uint8_t*)closeMessage, strlen(closeMessage)+1, true) ) {
    log_e("Failed to send file close message");
  }

  fileTransferInProgress = false;
}


static void FileSharingRouterCallbacks( BLERemoteCharacteristic* RemoteChar, uint8_t* pData, size_t length, bool isNotify ) {
  //char routing[512] = {0};
  const char* pRoutingData = (const char*)pData;
  char* blah = substr( pRoutingData, 0, length );
  const char* routing = (const char*)blah;
  if( length > 128 ) {
    log_w("Notified (%d bytes)", length);
  } else {
    log_w("Notified (%d bytes): %s / %s", length, blah, routing);
  }
  //memcpy( &routing, pData, length );
  fileSharingClientLastActivity = millis();
  if (strcmp(routing, BLE_VENDOR_NAMES_DB_FS_PATH) == 0) {
    log_w("FileSharingSendFile( %s )", BLE_VENDOR_NAMES_DB_FS_PATH);
    FileSharingSendFile( RemoteChar, BLE_VENDOR_NAMES_DB_FS_PATH );
    checkVendorResponded = true;
  } else if (strcmp(routing, MAC_OUI_NAMES_DB_FS_PATH) == 0) {
    log_w("FileSharingSendFile( %s )", MAC_OUI_NAMES_DB_FS_PATH);
    FileSharingSendFile( RemoteChar, MAC_OUI_NAMES_DB_FS_PATH );
    checkMacResponded = true;
  } else if ( strstr(routing, fileMarker ) ) {
    if ( strlen( routing ) > strlen( fileMarker ) ) {
      char* fileNameSize = substr( routing, strlen(fileMarker), strlen(routing) - strlen(fileMarker) );
      int pos = strpos(fileNameSize, ";", 0);
      char* fileNameStr = substr( fileNameSize, 0, pos );
      char* fileSizeStr = substr( fileNameSize, pos+1,  strlen(fileNameSize) - (pos+1) );
      //size_t fileSize = atoi( fileSizeStr );
      log_w( "remote file: %s (%s bytes)", fileNameStr, fileSizeStr );
      free( fileNameSize );
      free( fileNameStr );
      free( fileSizeStr );
    }
    // TODO: add to array
  } else if (strcmp(routing, lsDoneMessage ) == 0) {
    log_w("remote ls done");
    lsDone = true;
  } else if ( strstr(routing, checkMacFileMessage) ) {
    char* resp = substr( routing, strlen(routing)-1, 1 );
    log_w("checkMacFileMessage response: %s", resp);
    checkMacResponded = true;
    checkMacResponse = atoi( resp );
    free( resp );
  } else if ( strstr(routing, checkVendorFileMessage) ) {
    char* resp = substr( routing, strlen(routing)-1, 1 );
    log_w("checkVendorFileMessage response: %s", resp);
    checkVendorResponded = true;
    checkVendorResponse = atoi( resp );
    free( resp );
  } else if (strcmp(routing, "quit" ) == 0) {
    fileSharingClientTaskIsRunning = false;
  } else {
    log_w("Received unknown routing query: %s", routing);
  }
  free( blah );
  log_w("exiting notification");
};


class FileSharingClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* pC) {
      log_d("[Heap: %06d] Connect!!", freeheap);
      fileSharingClientLastActivity = millis();
    }
    void onDisconnect(BLEClient* pC) {
      log_d("[Heap: %06d] Disconnect!!", freeheap);
      fileSharingClientTaskIsRunning = false;
      fileSharingClientLastActivity = millis();
    }
};

FileSharingClientCallbacks* FileSharingClientCallback;


void stopFileSharingClient() {
  if ( FileSharingClient != NULL ) {
    if ( FileSharingClient->isConnected() ) {
      FileSharingClient->disconnect();
    }
  }
  if ( FileSharingClientCallback != NULL ) {
    log_w("Deleting FileSharingClientCallback");
    delete FileSharingClientCallback; FileSharingClientCallback = NULL;
  }
  //fileSharingClientTaskIsRunning = false;
  fileSharingClientStarted = false;
}


static void FileSharingClientTask( void * param ) {
  fileSharingClientLastActivity = millis();

  if ( FileSharingClientCallback == NULL ) {
    FileSharingClientCallback = new FileSharingClientCallbacks();
  }

  BLEDevice::setMTU(517);

  if ( FileSharingClient == NULL ) {
    FileSharingClient  = BLEDevice::createClient();
  }

  FileSharingClient->setClientCallbacks( FileSharingClientCallback );

  //HasBTTime = false;
  log_v("[Heap: %06d] Will connect to address %s", freeheap, fileServerBLEAddress.c_str());
  if ( !FileSharingClient->connect( fileServerBLEAddress, fileServerClientType ) ) {
    log_e("[Heap: %06d] Failed to connect to address %s", freeheap, fileServerBLEAddress.c_str());
    UI.headerStats("Connect failed :-(");
    stopFileSharingClient();
    vTaskDelete( NULL );
    return;
  }
  log_w("[Heap: %06d] Connected to address %s", freeheap, fileServerBLEAddress.c_str());
  BLESharingRemoteService = FileSharingClient->getService( FileSharingServiceUUID );
  if (BLESharingRemoteService == nullptr) {
    log_e("Failed to find our FileSharingServiceUUID: %s", FileSharingServiceUUID.toString().c_str());
    FileSharingClient->disconnect();
    UI.headerStats("Bounding failed :-(");
    stopFileSharingClient();
    vTaskDelete( NULL );
    return;
  }
  FileSharingReadRemoteChar = BLESharingRemoteService->getCharacteristic( FileSharingWriteUUID );
  if (FileSharingReadRemoteChar == nullptr) {
    log_e("Failed to find our characteristic FileSharingWriteUUID: %s, disconnecting", FileSharingWriteUUID.toString().c_str());
    FileSharingClient->disconnect();
    UI.headerStats("Bad char. :-(");
    stopFileSharingClient();
    vTaskDelete( NULL );
    return;
  }

  FileSharingRouterRemoteChar = BLESharingRemoteService->getCharacteristic( FileSharingRouteUUID );
  if (FileSharingRouterRemoteChar == nullptr) {
    log_e("Failed to find our characteristic FileSharingRouteUUID: %s, disconnecting", FileSharingRouteUUID.toString().c_str());
    FileSharingClient->disconnect();
    UI.headerStats("Bad char. :-(");
    stopFileSharingClient();
    vTaskDelete( NULL );
    return;
  }

  //FileSharingRouterRemoteChar->registerForNotify( FileSharingRouterCallbacks, true, false );
  FileSharingRouterRemoteChar->subscribe( true, false, FileSharingRouterCallbacks );

  UI.headerStats("Connected :-)");

  /*
  lsDone = false;
  if ( FileSharingRouterRemoteChar->writeValue((uint8_t*)lsMessage, strlen(lsMessage), true) ) {
    UI.headerStats("Discussing :-)");
  }
  while( ! lsDone ) {
    vTaskDelay(1000);
  }
  */
  log_w("Sending checkdb query");
  checkVendorResponded = false;
  if( FileSharingRouterRemoteChar->writeValue((uint8_t*)checkMacFileMessage, strlen(checkMacFileMessage), true) ) {
    log_w("Sent checkMacFileMessage query");
    while( !checkVendorResponded ) {
      vTaskDelay(100);
      if( fileSharingClientLastActivity + fileSharingClientTimeout < millis() ) {
        log_e("checkVendorResponded timeout !");
        break;
      }
    }
    //log_w("Vendor response: %d", checkVendorResponse);
    FileSharingSendFile( FileSharingRouterRemoteChar, BLE_VENDOR_NAMES_DB_FS_PATH );
    log_w("Sent checkMacFileMessage query");
  } else {
    log_e("Failed to send checkdb query");
  }

  checkMacResponded = false;
  if( FileSharingRouterRemoteChar->writeValue((uint8_t*)checkVendorFileMessage, strlen(checkVendorFileMessage), true) ) {
    log_w("Sent checkVendorFileMessage query");
    while( !checkMacResponded ) {
      vTaskDelay(100);
      if( fileSharingClientLastActivity + fileSharingClientTimeout < millis() ) {
        log_e("checkMacResponded timeout !");
        break;
      }
    }
    //log_w("Mac response: %d", checkMacResponse);
    FileSharingSendFile( FileSharingRouterRemoteChar, MAC_OUI_NAMES_DB_FS_PATH );
  } else {
    log_e("Failed to send checkdb query");
  }

  while( fileSharingClientTaskIsRunning ) {
    if( fileSharingClientLastActivity + fileSharingClientTimeout < millis() ) {
      log_e("fileSharingClientTimeout timeout !");
      break;
    } else {
      log_w("fileSharingClientLastActivity: %d", fileSharingClientLastActivity);
    }
    vTaskDelay( 1000 );
  }

  stopFileSharingClient();
  log_e("Deleting FileSharingClientTask" );
  vTaskDelete( NULL );

} // FileSharingClientTask
