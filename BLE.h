/*

  ESP32 BLE Collector - A BLE scanner with sqlite data persistence on the SD Card
  Source: https://github.com/tobozo/ESP32-BLECollector

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

*/


#include <string>
#include <sstream>
#include <sys/time.h>
#define TICKS_TO_DELAY 1000

#ifdef BUILD_NTPMENU_BIN
class BLEScanUtils {
  public:
    void init() {
      UI.init();
    };
    void scan() { };
};
#else


const char* processTemplateLong = "%s%d%s%d";
const char* processTemplateShort = "%s%d";
static char processMessage[20];

static bool onScanProcessed = true;
static bool onScanPopulated = true;
static bool onScanPropagated = true;
static bool onScanPostPopulated = true;
static bool onScanRendered = true;
static bool onScanDone = true;
static bool scanTaskRunning = false;
static bool scanTaskStopped = true;

static char* serialBuffer = NULL;
static char* tempBuffer = NULL;
#define SERIAL_BUFFER_SIZE 32

unsigned long lastheap = 0;
uint16_t lastscanduration = SCAN_DURATION;
char heapsign[5]; // unicode sign terminated
char scantimesign[5]; // unicode sign terminated
BLEScanResults bleresults;
BLEScan *pBLEScan;
BLEClient *pClient;
BLEAddress *pClientAddress = (BLEAddress*)calloc(1, sizeof(BLEAddress));
char* charBleAddress = (char*)calloc(17, sizeof(char));
std::string stdBLEAddress;

esp_ble_addr_type_t pClientType;

static uint16_t processedDevicesCount = 0;
bool foundDeviceToggler = true;

enum AfterScanSteps {
  POPULATE  = 0,
  IFEXISTS  = 1,
  RENDER    = 2,
  PROPAGATE = 3
};

/*
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID    charUUID1("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID    charUUID2("beb5483e-36e2-4688-b7f5-ea07361b26a8");
static BLEUUID    charUUID3("beb5483e-36e3-4688-b7f5-ea07361b26a8");
*/
static BLEUUID    timeServiceUUID((uint16_t)0x1805);
static BLEUUID    timeCharacteristicUUID((uint16_t)0x2a2b);

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t wday;
    uint8_t fraction;
    uint8_t adjust = 0;
} bt_time_t;

bt_time_t BLERemoteTime;// = calloc(0, sizeof(bt_time_t));
static bool hasBTTime = false;


static void TimeClientNotifyCallback( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify ) {
  memcpy( &BLERemoteTime, pData, length );
  log_e("[Heap: %06d] The characteristic value was: %04d-%02d-%02d %02d:%02d:%02d\n",
    freeheap,
    BLERemoteTime.year,
    BLERemoteTime.month,
    BLERemoteTime.day,
    BLERemoteTime.hour,
    BLERemoteTime.minutes,
    BLERemoteTime.seconds
  );
  hasBTTime = true;
};


class TimeClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pC){
    // xTaskCreate(scan1, "scan", 4048, NULL, 5, NULL);
    log_e("[Heap: %06d] Connect!!", freeheap);
  }
  void onDisconnect(BLEClient* pC) {
    log_e("[Heap: %06d] Disconnect!!", freeheap);
    //pBLEScan->erase(pC->getPeerAddress());
  }
};

class FoundDeviceCallback: public BLEAdvertisedDeviceCallbacks {

  void onResult(BLEAdvertisedDevice advertisedDevice) {

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService( timeServiceUUID )) {
      stdBLEAddress = advertisedDevice.getAddress().toString();
      pClientType = advertisedDevice.getAddressType();
      foundTimeServer = true;
      // log_e("[Heap: %06d] Found TimeServer !!", freeheap);
    }

    if( onScanDone  ) return;

    if( scan_cursor < MAX_DEVICES_PER_SCAN ) {
      BLEDevHelper.store( BLEDevTmpCache[scan_cursor], advertisedDevice );
      bool is_random = strcmp( BLEDevTmpCache[scan_cursor]->ouiname, "[random]" ) == 0;
      if( UI.filterVendors && is_random ) {
        log_e( "Filtering %s", BLEDevTmpCache[scan_cursor]->address );
      } else {
        if( DB.hasPsram ) {
          if( !is_random ) {
            DB.getOUI( BLEDevTmpCache[scan_cursor]->address, BLEDevTmpCache[scan_cursor]->ouiname );
          }
          if( BLEDevTmpCache[scan_cursor]->manufid > -1 ) {
            DB.getVendor( BLEDevTmpCache[scan_cursor]->manufid, BLEDevTmpCache[scan_cursor]->manufname );
          }
          BLEDevTmpCache[scan_cursor]->is_anonymous = BLEDevHelper.isAnonymous( BLEDevTmpCache[scan_cursor] );
          log_i(  "  stored and populated #%02d : %s", scan_cursor, advertisedDevice.toString().c_str());
        } else {
          log_i(  "  stored #%02d : %s", scan_cursor, advertisedDevice.toString().c_str());
        }
        scan_cursor++;
        processedDevicesCount++;
      }
      if( scan_cursor == MAX_DEVICES_PER_SCAN ) {
        onScanDone = true;
      }
    } else {
      onScanDone = true;
    }
    if( onScanDone ) {
      advertisedDevice.getScan()->stop();
      scan_cursor = 0;
      if( SCAN_DURATION-1 >= MIN_SCAN_DURATION ) {
        SCAN_DURATION--;
      }
    }
    foundDeviceToggler = !foundDeviceToggler;
    if(foundDeviceToggler) {
      UI.BLEStateIconSetColor(BLE_GREEN);
    } else {
      UI.BLEStateIconSetColor(BLE_DARKGREEN);
    }
    vTaskDelay(100);
  }
};

auto pDeviceCallback = new FoundDeviceCallback(); // collect/store BLE data
auto pTimeClientCallback = new TimeClientCallback();


struct SerialCallback {
  SerialCallback(void (*f)(void *) = 0, void *d = 0)
      : function(f), data(d) {}
  void (*function)(void *);
  void *data;
};

struct CommandTpl {
  const char* command;
  SerialCallback cb;
  const char* description;
};

CommandTpl* SerialCommands;
uint16_t Csize = 0;



class BLEScanUtils {

  public:

    void init() {
      BLEDevice::init("");
      BLEDevice::setMTU(100);
      pClient  = BLEDevice::createClient();
      pClient->setClientCallbacks( pTimeClientCallback );
      UI.init(); // launch all UI tasks
      DB.init(); // mount DB
      WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
      getPrefs();
      startScanCB();
      startSerialTask();
    }

    static void startScanCB( void * param = NULL ) {
      if( !scanTaskRunning ) {
        log_d("Starting scan" );
        xTaskCreatePinnedToCore(scanTask, "scanTask", 10000, NULL, 5, NULL, 1); /* last = Task Core */
        while(scanTaskStopped) {
          log_d("Waiting for scan to start...");
          vTaskDelay(1000);
        }
        Serial.println("Scan started...");
      }
    }
    static void stopScanCB( void * param = NULL) {
      if( scanTaskRunning ) {
        log_d("Stopping scan" );
        scanTaskRunning = false;
        BLEDevice::getScan()->stop();
        while(!scanTaskStopped) {
          log_d("Waiting for scan to stop...");
          vTaskDelay(1000);
        }
        Serial.println("Scan stopped...");
      }
    }
    static void restartCB( void * param = NULL ) {
      ESP.restart();
    }
    #if RTC_PROFILE > ROGUE //
      static void updateCB( void * param = NULL ) {
        stopScanCB();
        xTaskCreatePinnedToCore(loadNTPMenu, "loadNTPMenu", 16000, NULL, 5, NULL, 1); /* last = Task Core */
      }
      static void loadNTPMenu( void * param = NULL ) {
        rollBackOrUpdateFromFS( BLE_FS, NTP_MENU_FILENAME );
        ESP.restart();
      }
    #endif
    static void resetCB( void * param = NULL ) {
      DB.needsReset = true;
      Serial.println("DB Scheduled for reset");
      stopScanCB();
      delay(1);
      startScanCB();
    }
    static void pruneCB( void * param = NULL ) {
      DB.needsPruning = true;
      Serial.println("DB Scheduled for pruning");
      stopScanCB();
      delay(1);
      startScanCB();
    }
    static void toggleFilterCB( void * param = NULL ) {
      UI.filterVendors = ! UI.filterVendors;
      setPrefs();
      Serial.printf("UI.filterVendors = %s\n", UI.filterVendors ? "true" : "false" );
    }
    static void startDumpCB( void * param = NULL ) {
      xTaskCreatePinnedToCore(dumpTask, "dumpTask", 10000, NULL, 5, NULL, 1); /* last = Task Core */
    }
    static void toggleEchoCB( void * param = NULL ) {
      Out.serialEcho = !Out.serialEcho;
      setPrefs();
      Serial.printf("Out.serialEcho = %s\n", Out.serialEcho ? "true" : "false" );
    }
    static void listDirCB( void * param = NULL ) {
      stopScanCB();
      xTaskCreatePinnedToCore(listDirTask, "listDirTask", 5000, NULL, 2, NULL, 1); /* last = Task Core */      
    }
    static void listDirTask( void * param = NULL ) {
      listDir(BLE_FS, "/", 0);
      startScanCB();
      vTaskDelete( NULL );      
    }
    static void nullCB( void * param = NULL ) {
      // zilch, niente, nada, que dalle, nothing
    }
    static void startSerialTask() {
      serialBuffer = (char*)calloc( SERIAL_BUFFER_SIZE, sizeof(char) );
      tempBuffer   = (char*)calloc( SERIAL_BUFFER_SIZE, sizeof(char) );
      xTaskCreatePinnedToCore(serialTask, "serialTask", 2048, NULL, 0, NULL, 0); /* last = Task Core */
    }

    static void serialTask( void * parameter ) {
      CommandTpl Commands[] = {
        { "help",         nullCB,         "Print this list" },
        { "start",        startScanCB,    "Start/resume scan" },
        { "stop",         stopScanCB,     "Stop scan" },
        { "toggleFilter", toggleFilterCB, "Toggle vendor filter (persistent)" },
        { "toggleEcho",   toggleEchoCB,   "Toggle BLECards in serial console (persistent)" },
        { "dump",         startDumpCB,    "Dump returning BLE devices to the display" },
        { "ls",           listDirCB,      "Show the SD root dir Content" },
        { "restart",      restartCB,      "Restart BLECollector" },
        { "bletime",      startTimeClient,"Get time from another BLE Device" },
        #if RTC_PROFILE > ROGUE
        { "update",       updateCB,       "Update time and DB files (requires pre-flashed NTPMenu.bin)" },
        #endif
        { "resetDB",      resetCB,        "Hard Reset DB + forced restart" },
        { "pruneDB",      pruneCB,        "Soft Reset DB without restarting (hopefully)" },
      };
      SerialCommands = Commands;
      Csize = (sizeof(Commands) / sizeof(Commands[0]));
      runCommand( (char*)"help" );
      static byte idx = 0;
      char lf = '\n';
      char cr = '\r';
      char c;
      while( 1 ) {
        while (Serial.available() > 0) {
          c = Serial.read();
          if (c != cr && c!=lf) {
            serialBuffer[idx] = c;
            idx++;
            if (idx >= SERIAL_BUFFER_SIZE) {
              idx = SERIAL_BUFFER_SIZE - 1;
            }
          } else {
            serialBuffer[idx] = '\0'; // null terminate
            memcpy( tempBuffer, serialBuffer, idx+1 );
            runCommand( tempBuffer );
            idx = 0;            
          }
          delay(1);
        }
        delay(1);
      }
    }


    static void runCommand( char* command ) {
      if( isEmpty( command ) ) return;
      if( strcmp( command, "help" ) == 0 ) {
        Serial.println("\nAvailable Commands:\n");
        for( uint16_t i=0; i<Csize; i++ ) {
          Serial.printf("  %02d) %16s : %s\n", i+1, SerialCommands[i].command, SerialCommands[i].description);
        }
        Serial.println();
      } else {
        for( uint16_t i=0; i<Csize; i++ ) {
          if( strcmp( SerialCommands[i].command, command )==0 ) {
            Serial.printf( "Running '%s' command\n", SerialCommands[i].command );
            SerialCommands[i].cb.function( NULL );
            sprintf(command, "%s", "");
            return;
          }
        }
        Serial.printf( "Command '%s' not found\n", command );
      }
    }


    static void dumpTask( void * parameter ) {
      bool dumpTaskRunning = true;
      stopScanCB();
      while( dumpTaskRunning ) {
        for( int16_t i=0; i< BLEDEVCACHE_SIZE; i++ ) {
          if( !isEmpty( BLEDevCache[i]->address ) ) {
            UI.BLECardTheme.setTheme( IN_CACHE_ANON );
            BLEDevTmp = BLEDevCache[i];
            UI.printBLECard( BLEDevTmp ); // render                      
          }
        }
        dumpTaskRunning = false;
      }
      startScanCB();
      vTaskDelete( NULL );
    }


    static void startTimeClient( void * param) {
      if( foundTimeServer ) {
        stopScanCB();
        xTaskCreatePinnedToCore(TimeClientTask, "TimeClientTask", 12000, NULL, 0, NULL, 0); /* last = Task Core */
      } else {
        Serial.println("Sorry, no time server available at the moment");
      }
    }

    static void TimeClientTask( void * param ) {

      log_e("[Heap: %06d] Will connect to address %s", freeheap, stdBLEAddress.c_str());
      
      //charBleAddress
      //pClient->connect( *(BLEAddress*)pClientAddress, pClientType );
      pClient->connect( /* *(BLEAddress*) */stdBLEAddress, pClientType );
      BLERemoteService* pRemoteService = pClient->getService( timeServiceUUID );
      if (pRemoteService == nullptr) {
        log_e("Failed to find our service UUID: %s", timeServiceUUID.toString().c_str());
        pClient->disconnect();
        startScanCB();
        vTaskDelete( NULL );
        return;
      }

      BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic( timeCharacteristicUUID );
      if (pRemoteCharacteristic == nullptr) {
        log_e("Failed to find our characteristic timeCharacteristicUUID: %s, disconnecting", timeCharacteristicUUID.toString().c_str());
        pClient->disconnect();
        startScanCB();
        vTaskDelete( NULL );
        return;
      }
      log_e("[Heap: %06d] registering for notification", freeheap);
      pRemoteCharacteristic->registerForNotify( TimeClientNotifyCallback );
      TickType_t last_wake_time;
      last_wake_time = xTaskGetTickCount();
      log_e("[Heap: %06d] while connected", freeheap);
      hasBTTime = false;
      while(pClient->isConnected()) {
        vTaskDelayUntil(&last_wake_time, TICKS_TO_DELAY/portTICK_PERIOD_MS);
        if( hasBTTime ) {
          pClient->disconnect();
        }
      }
      log_e("[Heap: %06d] client disconnected", freeheap);
      startScanCB();
      vTaskDelete( NULL );
    }


    static void scanTask( void * parameter ) {
      scanTaskRunning = true;
      scanTaskStopped = false;
      pBLEScan = BLEDevice::getScan(); //create new scan
      pBLEScan->setAdvertisedDeviceCallbacks( pDeviceCallback );
      pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
      pBLEScan->setInterval(0x50); // 0x50
      pBLEScan->setWindow(0x30); // 0x30
      byte onAfterScanStep = 0;
      while( scanTaskRunning ) {
        if( onAfterScanSteps( onAfterScanStep, scan_cursor ) ) continue;
        dumpStats("BeforeScan::");
        onBeforeScan();
        pBLEScan->start(SCAN_DURATION);
        onAfterScan();
        UI.update(); // run after-scan display stuff
        DB.maintain();
        dumpStats("AfterScan:::");
        scan_rounds++;
      }
      scanTaskStopped = true;
      vTaskDelete( NULL );
    }


    static bool onAfterScanSteps( byte &onAfterScanStep, uint16_t &scan_cursor ) {
      switch( onAfterScanStep ) {
        case POPULATE: // 0
          if(! DB.hasPsram ) {
            onScanPopulate( scan_cursor ); // OUI / vendorname / isanonymous
          }
          onAfterScanStep++;
          return true;
        break;
        case IFEXISTS: // 1
          onScanIfExists( scan_cursor ); // exists + hits
          onAfterScanStep++;
          return true;
        break;
        case RENDER: // 2
          onScanRender( scan_cursor ); // ui work
          onAfterScanStep++;
          return true;
        break;
        case PROPAGATE: // 3
          onAfterScanStep = 0;
          if( onScanPropagate( scan_cursor ) ) { // copy to DB / cache
            scan_cursor++;
            return true;
          }
        break;
        default:
          log_w("Exit flat loop on afterScanStep value : %d", onAfterScanStep);
          onAfterScanStep = 0;
        break;
      }
      return false;
    }


    static bool onScanPopulate( uint16_t _scan_cursor ) {
      if( onScanPopulated ) {
        log_v("%s", " onScanPopulated = true ");
        return false;
      }
      if( _scan_cursor >= devicesCount) {
        onScanPopulated = true;
        log_d("%s", "done all");
        return false;
      }
      uint16_t BLEDevTmpCacheIndex = _scan_cursor;
      if( isEmpty( BLEDevTmpCache[_scan_cursor]->address ) ) {
        log_w("empty addess");
        return true; // end of cache
      }
      populate( BLEDevTmpCache[BLEDevTmpCacheIndex] );
      return true;
    }


    static bool onScanIfExists( int _scan_cursor ) {
      if( onScanPostPopulated ) {
        log_v("onScanPostPopulated = true");
        return false;
      }
      if( _scan_cursor >= devicesCount) {
        log_d("done all");
        onScanPostPopulated = true;
        return false;
      }
      if( BLEDevTmpCache[scan_cursor]->hits > 0 ) {
        log_v("skipping, has hits");
        return true;
      }
      int deviceIndexIfExists = -1;
      deviceIndexIfExists = getDeviceCacheIndex( BLEDevTmpCache[_scan_cursor]->address );
      if( deviceIndexIfExists > -1 ) {
        inCacheCount++;
        BLEDevCache[deviceIndexIfExists]->hits++;
        #if RTC_PROFILE > HOBO // all profiles manage time except HOBO
        BLEDevCache[deviceIndexIfExists]->updated_at = nowDateTime;
        BLEDevTmpCache[_scan_cursor]->updated_at = BLEDevCache[deviceIndexIfExists]->updated_at;
        BLEDevTmpCache[_scan_cursor]->created_at = BLEDevCache[deviceIndexIfExists]->created_at;
        #endif
        BLEDevTmpCache[_scan_cursor]->hits = BLEDevCache[deviceIndexIfExists]->hits;
        BLEDevTmpCache[_scan_cursor]->in_db = BLEDevCache[deviceIndexIfExists]->in_db;
        log_d( "Device %d exists in cache with %d hits", _scan_cursor, BLEDevTmpCache[_scan_cursor]->hits );
      } else {
        if( BLEDevTmpCache[_scan_cursor]->is_anonymous ) {
          // won't land in DB but can land in cache
          uint16_t nextCacheIndex = BLEDevHelper.getNextCacheIndex( BLEDevCache, BLEDevCacheIndex );
          BLEDevTmpCache[_scan_cursor]->hits = 1;
          BLEDevCache[nextCacheIndex] = BLEDevTmpCache[_scan_cursor];
          log_d( "Device %d is anonymous, won't be inserted, set %d hits to 1 and copied", _scan_cursor, BLEDevTmpCache[scan_cursor]->hits );
        } else {
          deviceIndexIfExists = DB.deviceExists( BLEDevTmpCache[_scan_cursor]->address ); // will load returning devices from DB if necessary
          if(deviceIndexIfExists>-1) {
            BLEDevTmpCache[_scan_cursor]->in_db = true;
            BLEDevCache[deviceIndexIfExists]->hits++;
            #if RTC_PROFILE > HOBO // all profiles manage time except HOBO
            BLEDevCache[deviceIndexIfExists]->updated_at = nowDateTime;
            #endif
            BLEDevTmpCache[_scan_cursor]->hits = BLEDevCache[deviceIndexIfExists]->hits;
            BLEDevTmpCache[_scan_cursor]->in_db = BLEDevCache[deviceIndexIfExists]->in_db;
            log_d( "Device %d is already in DB, increased hits to %d", _scan_cursor, BLEDevTmpCache[scan_cursor]->hits );
          } else {
            log_d( "Device %d is not in DB", _scan_cursor );
          }
        }
      }
      return true;
    }


    static bool onScanRender( uint16_t _scan_cursor ) {
      if( onScanRendered ) {
        log_v("onScanRendered = true");
        return false;
      }
      if( _scan_cursor >= devicesCount) {
        log_d("done all");
        onScanRendered = true;
        return false;
      }
      UI.BLECardTheme.setTheme( IN_CACHE_ANON );
      BLEDevTmp = BLEDevTmpCache[_scan_cursor];
      UI.printBLECard( BLEDevTmp ); // render
      delay(1);
      sprintf( processMessage, processTemplateLong, "Rendered ", _scan_cursor+1, " / ", devicesCount );
      UI.headerStats( processMessage );
      delay(1);
      UI.cacheStats();
      delay(1);
      UI.footerStats();
      delay(1);
      return true;
    }


    static bool onScanPropagate( uint16_t &_scan_cursor ) {
      if( onScanPropagated ) {
        log_v("onScanPropagated = true");
        return false;
      }
      if( _scan_cursor >= devicesCount) {
        log_d("done all");
        onScanPropagated = true;
        _scan_cursor = 0;
        return false;
      }
      BLEDevTmpCacheIndex = _scan_cursor;
      if( isEmpty( BLEDevTmpCache[_scan_cursor]->address ) ) {
        return true;
      }
      if( BLEDevTmpCache[_scan_cursor]->is_anonymous || BLEDevTmpCache[_scan_cursor]->in_db ) { // don't DB-insert anon or duplicates
        sprintf( processMessage, processTemplateLong, "Released ", _scan_cursor+1, " / ", devicesCount );
        if( BLEDevTmpCache[_scan_cursor]->is_anonymous ) AnonymousCacheHit++;
      } else {
        if( DB.insertBTDevice( BLEDevTmpCache, BLEDevTmpCacheIndex ) == DBUtils::INSERTION_SUCCESS ) {
          sprintf( processMessage, processTemplateLong, "Saved ", _scan_cursor+1, " / ", devicesCount );
          log_d( "Device %d successfully inserted in DB", _scan_cursor );
          entries++;
        } else {
          log_e( "  [!!! BD INSERT FAIL !!!] Device %d could not be inserted", _scan_cursor );
          sprintf( processMessage, processTemplateLong, "Failed ", _scan_cursor+1, " / ", devicesCount );
        }
      }
      BLEDevHelper.reset( BLEDevTmpCache[_scan_cursor] ); // discard
      UI.headerStats( processMessage );
      return true;
    }


    static void onBeforeScan() {
      UI.headerStats("  Scan in progress");
      UI.startBlink();
      processedDevicesCount = 0;
      devicesCount = 0;
      scan_cursor = 0;
      onScanProcessed = false;
      onScanDone = false;
      onScanPopulated = false;
      onScanPropagated = false;
      onScanPostPopulated = false;
      onScanRendered = false;
      foundTimeServer = false;
    }

    static void onAfterScan() {
      UI.stopBlink();
      UI.headerStats("Showing results ...");
      devicesCount = processedDevicesCount;
      BLEDevice::getScan()->clearResults();
      if( devicesCount < MAX_DEVICES_PER_SCAN ) {
        if( SCAN_DURATION+1 < MAX_SCAN_DURATION ) {
          SCAN_DURATION++;
        }
      } else if( devicesCount > MAX_DEVICES_PER_SCAN ) {
        if( SCAN_DURATION-1 >= MIN_SCAN_DURATION ) {
          SCAN_DURATION--;
        }
        log_w("Cache overflow (%d results vs %d slots), truncating results...", devicesCount, MAX_DEVICES_PER_SCAN);
        devicesCount = MAX_DEVICES_PER_SCAN;
      } else {
        // same amount
      }
      sessDevicesCount += devicesCount;
      notInCacheCount = 0;
      inCacheCount = 0;
      onScanDone = true;
      scan_cursor = 0;
    }


    static int getDeviceCacheIndex(const char* address) {
      if( isEmpty( address ) )  return -1;
      for(int i=0; i<BLEDEVCACHE_SIZE; i++) {
        if( strcmp(address, BLEDevCache[i]->address )==0  ) {
          BLEDevCacheHit++;
          log_v("[CACHE HIT] BLEDevCache ID #%s has %d cache hits", address, BLEDevCache[i]->hits);
          return i;
        }
        delay(1);
      }
      return -1;
    }

    // used for serial debugging
    static void dumpStats(const char* prefixStr) {
      if(lastheap > freeheap) {
        // heap decreased
        sprintf(heapsign, "%s", "↘");
      } else if(lastheap < freeheap) {
        // heap increased
        sprintf(heapsign, "%s", "↗");
      } else {
        // heap unchanged
        sprintf(heapsign, "%s", "⇉");
      }
      if(lastscanduration > SCAN_DURATION) {
        sprintf(scantimesign, "%s", "↘");
      } else if(lastscanduration < SCAN_DURATION) {
        sprintf(scantimesign, "%s", "↗");
      } else {
        sprintf(scantimesign, "%s", "⇉");
      }

      lastheap = freeheap;
      lastscanduration = SCAN_DURATION;

      log_i("%s[Scan#%02d][%s][Duration%s%d][Processed:%d of %d][Heap%s%d / %d] [Cache hits][Screen:%d][BLEDevCards:%d][Anonymous:%d][Oui:%d][Vendor:%d]\n", 
        prefixStr,
        scan_rounds,
        hhmmssString,
        scantimesign,
        lastscanduration,
        processedDevicesCount,
        devicesCount,
        heapsign,
        lastheap,
        freepsheap,
        SelfCacheHit,
        BLEDevCacheHit,
        AnonymousCacheHit,
        OuiCacheHit,
        VendorCacheHit
      );
    }

  private:

    static void getPrefs() {
      preferences.begin("BLEPrefs", true);
      Out.serialEcho = preferences.getBool("serialEcho", true);
      UI.filterVendors = preferences.getBool("filterVendors", false);
      preferences.end();
    }
    static void setPrefs() {
      preferences.begin("BLEPrefs", false); 
      preferences.putBool("serialEcho", Out.serialEcho);
      preferences.putBool("filterVendors", UI.filterVendors );
      preferences.end();
    }

    // completes unpopulated fields of a given entry by performing DB oui/vendor lookups
    static void populate( BlueToothDevice *CacheItem ) {
      if( strcmp( CacheItem->ouiname, "[unpopulated]" )==0 ){
        log_d("  [populating OUI for %s]", CacheItem->address);
        DB.getOUI( CacheItem->address, CacheItem->ouiname );
      }
      if( strcmp( CacheItem->manufname, "[unpopulated]" )==0 ) {
        if( CacheItem->manufid!=-1 ) {
          log_d("  [populating Vendor for :%d]", CacheItem->manufid );
          DB.getVendor( CacheItem->manufid, CacheItem->manufname );
        } else {
          BLEDevHelper.set( CacheItem, "manufname", '\0');
        }
      }
      CacheItem->is_anonymous = BLEDevHelper.isAnonymous( CacheItem );
      log_d("[populated :%s]", CacheItem->address);
    }

};


#endif

BLEScanUtils BLECollector;
