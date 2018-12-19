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


#ifdef BUILD_NTPMENU_BIN
class BLEScanUtils {
  public:
    void init() {
      UI.init();
      SDSetup();
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

unsigned long lastheap = 0;
uint16_t lastscanduration = SCAN_DURATION;
char heapsign[5]; // unicode sign terminated
char scantimesign[5]; // unicode sign terminated
BLEScanResults bleresults;
BLEScan *pBLEScan;

static uint16_t processedDevicesCount = 0;
bool foundDeviceToggler = true;

enum AfterScanSteps {
  POPULATE  = 0,
  IFEXISTS  = 1,
  RENDER    = 2,
  PROPAGATE = 3
};

class FoundDeviceCallback: public BLEAdvertisedDeviceCallbacks {

  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if( onScanDone  ) return;
    if( scan_cursor < MAX_DEVICES_PER_SCAN ) {
      BLEDevHelper.store( BLEDevTmpCache[scan_cursor], advertisedDevice );
      if( DB.hasPsram ) {
        DB.getOUI( BLEDevTmpCache[scan_cursor]->address, BLEDevTmpCache[scan_cursor]->ouiname );
        if( BLEDevTmpCache[scan_cursor]->manufid > -1 ) {
          DB.getVendor( BLEDevTmpCache[scan_cursor]->manufid, BLEDevTmpCache[scan_cursor]->manufname );
        }
        BLEDevTmpCache[scan_cursor]->is_anonymous = BLEDevHelper.isAnonymous( BLEDevTmpCache[scan_cursor] );
        Serial.printf(  "  [%s] stored and populated #%02d : %s\n", __func__, scan_cursor, advertisedDevice.toString().c_str());
      } else {
        Serial.printf(  "  [%s] stored #%02d : %s\n", __func__, scan_cursor, advertisedDevice.toString().c_str());
      }
      scan_cursor++;
      processedDevicesCount++;
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

static bool scanTaskRunning = false;
static bool scanTaskStopped = true;

static char* serialBuffer = NULL;
static char* tempBuffer = NULL;
#define SERIAL_BUFFER_SIZE 32

class BLEScanUtils {

  public:

    void init() {
      UI.init(); // launch all UI tasks
      SDSetup(); // health check before mounting DB
      DB.init(); // mount DB
      WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
      startScanTask();
      startSerialTask();
    }

    static void startScanTask() {
      xTaskCreatePinnedToCore(scanTask, "scanTask", 10000, NULL, 5, NULL, 1); /* last = Task Core */
      while(scanTaskStopped) {
        Serial.printf("[%s] Waiting for scan to start...\n", __func__);
        vTaskDelay(1000);
      }
      Serial.printf("[%s] Scan started...\n", __func__);
    }
    static void stopScanTask() {
      scanTaskRunning = false;
      BLEDevice::getScan()->stop();
      while(!scanTaskStopped) {
        Serial.printf("[%s] Waiting for scan to stop...\n", __func__);
        vTaskDelay(1000);
      }
      Serial.printf("[%s] Scan stopped...\n", __func__);
    }

    static void startDumpTask() {
      xTaskCreatePinnedToCore(dumpTask, "dumpTask", 10000, NULL, 5, NULL, 1); /* last = Task Core */
    }

    static void startSerialTask() {
      serialBuffer = (char*)calloc( SERIAL_BUFFER_SIZE, sizeof(char) );
      tempBuffer   = (char*)calloc( SERIAL_BUFFER_SIZE, sizeof(char) );
      xTaskCreatePinnedToCore(serialTask, "serialTask", 2048, NULL, 0, NULL, 0); /* last = Task Core */      
    }


    static void serialTask( void * parameter ) {
      static byte idx = 0;
      char eol = '\n';
      char c;
      Serial.printf("[%s] Listening to Serial...\n", __func__ );
      while( 1 ) {
        while (Serial.available() > 0) {
          c = Serial.read();
          if (c != eol) {
            serialBuffer[idx] = c;
            idx++;
            if (idx >= SERIAL_BUFFER_SIZE) {
              idx = SERIAL_BUFFER_SIZE - 1;
            }
          } else {
            serialBuffer[idx] = '\0'; // null terminate
            memcpy(tempBuffer, serialBuffer, idx+1);
            if( !serialParseBuffer() ) {
              Serial.printf("[%s] Serial data received and ignored: %s / %s\n", __func__, serialBuffer, tempBuffer );
            }
            idx = 0;            
          }
          delay(1);
        }
        delay(1);
      }
    }

    static bool serialParseBuffer() {
      if(strstr(tempBuffer, "stop")) {
        if( scanTaskRunning ) {
          Serial.printf("[%s] Stopping scan\n", __func__ );
          stopScanTask();
          return true;
        }
      } else if(strstr(tempBuffer, "restart")) {
        ESP.restart();
      } else if(strstr(tempBuffer, "start")) {
        if( !scanTaskRunning ) {
          Serial.printf("[%s] Starting scan\n", __func__ );
          startScanTask();
          return true;
        }
      #if RTC_PROFILE == CHRONOMANIAC  // chronomaniac mode
      } else if(strstr(tempBuffer, "update")) {
        resetTimeActivity( SOURCE_COMPILER ); // will eventually result in loading NTPMenu.bin
        ESP.restart();
      #endif
      } else if(strstr(tempBuffer, "dump")) {
        Serial.printf("[%s] Stopping scan\n", __func__ );
        startDumpTask();
        return true;
      } else if(strstr(tempBuffer, "resetDB")) {
        stopScanTask();
        BLE_FS.remove( DB.BLEMacsDbSQLitePath );
        ESP.restart();
        //DB.resetDB();
        return true;
      }
      return false;
    }


    static void dumpTask( void * parameter ) {
      bool dumpTaskRunning = true;
      stopScanTask();
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
      startScanTask();
      vTaskDelete( NULL );
    }

    static void scanTask( void * parameter ) {
      scanTaskRunning = true;
      scanTaskStopped = false;
      BLEDevice::init("");
      pBLEScan = BLEDevice::getScan(); //create new scan
      auto pDeviceCallback = new FoundDeviceCallback(); // collect/store BLE data
      pBLEScan->setAdvertisedDeviceCallbacks( pDeviceCallback ); // memory leak ?
      pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
      pBLEScan->setInterval(0x50); // 0x50
      pBLEScan->setWindow(0x30); // 0x30
      byte onAfterScanStep = 0;
      while( scanTaskRunning ) {
        if( onAfterScanSteps( onAfterScanStep, scan_cursor ) ) continue;
        Serial.print("BeforeScan::");dumpStats();
        onBeforeScan();
        pBLEScan->start(SCAN_DURATION);
        onAfterScan();
        UI.update(); // run after-scan display stuff
        DB.maintain();
        Serial.print("AfterScan:::");dumpStats();
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
          Serial.printf("[%s] Exit flat loop on afterScanStep value : %d\n", __func__, onAfterScanStep);
          onAfterScanStep = 0;
        break;
      }
      return false;
    }


    static bool onScanPopulate( uint16_t _scan_cursor ) {
      if( onScanPopulated ) {
        //Serial.printf("[%s] %s\n", __func__, " onScanPopulated = true ");
        return false;
      }
      if( scan_cursor >= devicesCount) {
        onScanPopulated = true;
        Serial.printf("[%s] %s\n", __func__, "done all");
        return false;
      }
      uint16_t BLEDevTmpCacheIndex = _scan_cursor;
      if( isEmpty( BLEDevTmpCache[_scan_cursor]->address ) ) {
        Serial.printf("  [%s] %s\n", __func__, "empty addess");
        return true; // end of cache
      }
      populate( BLEDevTmpCache[BLEDevTmpCacheIndex] );
      return true;
    }


    static bool onScanIfExists( int _scan_cursor ) {
      if( onScanPostPopulated ) {
        //Serial.printf("[%s] %s\n", __func__, " onScanPostPopulated = true ");
        return false;
      }
      if( scan_cursor >= devicesCount) {
        Serial.printf("[%s] %s\n", __func__, "done all");
        onScanPostPopulated = true;
        return false;
      }
      if( BLEDevTmpCache[scan_cursor]->hits > 0 ) {
        Serial.printf("  [%s] %s\n", __func__, "skipping, has hits");
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
        Serial.printf( "  [%s] Device %d exists in cache with %d hits\n", __func__, _scan_cursor, BLEDevTmpCache[_scan_cursor]->hits );
      } else {
        if( BLEDevTmpCache[_scan_cursor]->is_anonymous ) {
          // won't land in DB but can land in cache
          uint16_t nextCacheIndex = BLEDevHelper.getNextCacheIndex( BLEDevCache, BLEDevCacheIndex );
          BLEDevTmpCache[_scan_cursor]->hits = 1;
          BLEDevCache[nextCacheIndex] = BLEDevTmpCache[_scan_cursor];
          Serial.printf( "  [%s] Device %d is anonymous, won't be inserted, set %d hits to 1 and copied\n", __func__, _scan_cursor, BLEDevTmpCache[scan_cursor]->hits );
        } else {
          deviceIndexIfExists = DB.deviceExists( BLEDevTmpCache[_scan_cursor]->address ); // will load returning devices from DB if necessary
          if(deviceIndexIfExists>-1) {
            BLEDevTmpCache[_scan_cursor]->in_db = true;
            BLEDevCache[deviceIndexIfExists]->hits++;
            #if RTC_PROFILE > HOBO // all profiles manage time except HOBO
            BLEDevCache[deviceIndexIfExists]->updated_at = nowDateTime;
            //BLEDevTmpCache[_scan_cursor]->updated_at = BLEDevCache[deviceIndexIfExists]->updated_at;
            //BLEDevTmpCache[_scan_cursor]->created_at = BLEDevCache[deviceIndexIfExists]->created_at;
            #endif
            BLEDevTmpCache[_scan_cursor]->hits = BLEDevCache[deviceIndexIfExists]->hits;
            BLEDevTmpCache[_scan_cursor]->in_db = BLEDevCache[deviceIndexIfExists]->in_db;
            Serial.printf( "  [%s] Device %d is already in DB, increased hits to %d\n", __func__, _scan_cursor, BLEDevTmpCache[scan_cursor]->hits );
          } else {
            Serial.printf( "  [%s] Device %d is not in DB\n", __func__, _scan_cursor );
            //BLEDevTmpCache[_scan_cursor]->updated_at = BLEDevCache[deviceIndexIfExists]->updated_at;
            //BLEDevTmpCache[_scan_cursor]->created_at = BLEDevCache[deviceIndexIfExists]->created_at;
          }
        }
      }
      return true;
    }


    static bool onScanRender( uint16_t _scan_cursor ) {
      if( onScanRendered ) {
        //Serial.printf("[%s] %s\n", __func__, " onScanRendered = true ");
        return false;
      }
      if( scan_cursor >= devicesCount) {
        Serial.printf("[%s] %s\n", __func__, "done all");
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


    static bool onScanPropagate( uint16_t _scan_cursor ) {
      if( onScanPropagated ) {
        //Serial.printf("[%s] %s\n", __func__, " onScanPropagated = true ");
        return false;
      }
      if( scan_cursor >= devicesCount) {
        Serial.printf("[%s] %s\n", __func__, "done all");
        onScanPropagated = true;
        scan_cursor = 0;
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
          Serial.printf( "  [%s] Device %d successfully inserted in DB\n", __func__, _scan_cursor );
          entries++;
        } else {
          Serial.printf( "  [!!! BD INSERT FAIL !!!][%s] Device %d could not be inserted\n", __func__, _scan_cursor );
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
        //Serial.printf("[%s] Cache overflow (%d results vs %d slots), truncating results...\n", __func__, devicesCount, MAX_DEVICES_PER_SCAN);
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
          //BLEDevCache[i]->hits++;
          BLEDevCacheHit++;
          //Serial.printf("[CACHE HIT] BLEDevCache ID #%s has %d cache hits\n", address, BLEDevCache[i].hits);
          return i;
        }
        delay(1);
      }
      return -1;
    }

    // used for serial debugging
    static void dumpStats(uint8_t indent=0) {
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

      Serial.printf("[Scan#%02d][%s][Duration%s%d][Processed:%d of %d][Heap%s%d / %d] [Cache hits][Screen:%d][BLEDevCards:%d][Anonymous:%d][Oui:%d][Vendor:%d]\n", 
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

    // completes unpopulated fields of a given entry by performing DB oui/vendor lookups
    static void populate( BlueToothDevice *CacheItem ) {
      if( strcmp( CacheItem->ouiname, "[unpopulated]" )==0 ){
        Serial.printf("  [populating OUI for %s]\n", CacheItem->address);
        DB.getOUI( CacheItem->address, CacheItem->ouiname );
      }
      if( strcmp( CacheItem->manufname, "[unpopulated]" )==0 ) {
        if( CacheItem->manufid!=-1 ) {
          Serial.printf("  [populating Vendor for :%d]\n", CacheItem->manufid );
          DB.getVendor( CacheItem->manufid, CacheItem->manufname );
        } else {
          BLEDevHelper.set( CacheItem, "manufname", '\0');
        }
      }
      //Serial.printf("[populating anon state :%d]\n", CacheItemIndex);
      CacheItem->is_anonymous = BLEDevHelper.isAnonymous( CacheItem );
      //Serial.printf("[populated :%d]\n", CacheItemIndex);
    }

};


#endif

BLEScanUtils BLECollector;
