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

#define TICKS_TO_DELAY 1000




const char* processTemplateLong = "%s%d%s%d";
const char* processTemplateShort = "%s%d";
static char processMessage[20];

bool onScanProcessed = true;
bool onScanPopulated = true;
bool onScanPropagated = true;
bool onScanPostPopulated = true;
bool onScanRendered = true;
bool onScanDone = true;
bool scanTaskRunning = false;
bool scanTaskStopped = true;

#ifdef WITH_WIFI
static bool WiFiStarted = false;
static bool NTPDateSet = false;
#endif

extern size_t devicesStatCount;

static char* serialBuffer = NULL;
static char* tempBuffer = NULL;
#define SERIAL_BUFFER_SIZE 64

unsigned long lastheap = 0;
uint16_t lastscanduration = SCAN_DURATION;
char heapsign[5]; // unicode sign terminated
char scantimesign[5]; // unicode sign terminated
BLEScanResults bleresults;
BLEScan *pBLEScan;

TaskHandle_t TimeServerTaskHandle;
TaskHandle_t TimeClientTaskHandle;
TaskHandle_t FileServerTaskHandle;
TaskHandle_t FileClientTaskHandle;

static uint16_t processedDevicesCount = 0;
bool foundDeviceToggler = true;


enum AfterScanSteps {
  POPULATE  = 0,
  IFEXISTS  = 1,
  RENDER    = 2,
  PROPAGATE = 3
};


static void ESP_Restart() {
  ESP.restart();
}

/*
// work in progress: MAC blacklist/whitelist
const char MacList[3][MAC_LEN + 1] = {
  "aa:aa:aa:aa:aa:aa",
  "bb:bb:bb:bb:bb:bb",
  "cc:cc:cc:cc:cc:cc"
};


static bool AddressIsListed( const char* address ) {
  for ( byte i = 0; i < sizeof(MacList); i++ ) {
    if ( strcmp(address, MacList[i] ) == 0) {
      return true;
    }
  }
  return false;
}
*/

static bool deviceHasKnownPayload( BLEAdvertisedDevice *advertisedDevice ) {
  if ( !advertisedDevice->haveServiceUUID() ) return false;
  if( advertisedDevice->isAdvertisingService( timeServiceUUID ) ) {
    log_i( "Found Time Server %s : %s", advertisedDevice->getAddress().toString().c_str(), advertisedDevice->getServiceUUID().toString().c_str() );
    timeServerBLEAddress = advertisedDevice->getAddress().toString();
    timeServerClientType = advertisedDevice->getAddressType();
    foundTimeServer = true;
    if ( foundTimeServer && (!TimeIsSet || ForceBleTime) ) {
      return true;
    }
  }
  if( advertisedDevice->isAdvertisingService( FileSharingServiceUUID ) ) {
    log_i( "Found File Server %s : %s", advertisedDevice->getAddress().toString().c_str(), advertisedDevice->getServiceUUID().toString().c_str() );
    foundFileServer = true;
    fileServerBLEAddress = advertisedDevice->getAddress().toString();
    fileServerClientType = advertisedDevice->getAddressType();
    if ( fileSharingEnabled ) {
      log_w("Ready to connect to file server %s", fileServerBLEAddress.c_str());
      return true;
    }
  }

  if( advertisedDevice->isAdvertisingService( StopCovidServiceUUID ) ) {
    log_n("Found StopCovid Advertisement %s : %s", advertisedDevice->getAddress().toString().c_str(), advertisedDevice->getServiceUUID().toString().c_str() );
    uint8_t *payLoad = advertisedDevice->getPayload();
    size_t payLoadLen = advertisedDevice->getPayloadLength();
    Serial.printf("Payload (%d bytes): ", payLoadLen);
    for (size_t i=0; i<payLoadLen; i++ ) {
      Serial.printf("%02x ", payLoad[i] );
    }
    Serial.println();
  }

  return false;
}



class FoundDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult( BLEAdvertisedDevice *advertisedDevice )
    {
      devicesStatCount++; // raw stats for heapgraph

      bool scanShouldStop =  deviceHasKnownPayload( advertisedDevice );

      if ( onScanDone  ) return;

      if ( scan_cursor < MAX_DEVICES_PER_SCAN ) {
        log_i("will store advertisedDevice in cache #%d", scan_cursor);
        BLEDevHelper.store( BLEDevScanCache[scan_cursor], advertisedDevice );
        //bool is_random = strcmp( BLEDevScanCache[scan_cursor]->ouiname, "[random]" ) == 0;
        bool is_random = (BLEDevScanCache[scan_cursor]->addr_type == BLE_ADDR_RANDOM );
        //bool is_blacklisted = isBlackListed( BLEDevScanCache[scan_cursor]->address );
        if ( UI.filterVendors && is_random ) {
          //TODO: scan_cursor++
          log_i( "Filtering %s", BLEDevScanCache[scan_cursor]->address );
        } else {
          if ( DB.hasPsram ) {
            if ( !is_random ) {
              DB.getOUI( BLEDevScanCache[scan_cursor]->address, BLEDevScanCache[scan_cursor]->ouiname );
            }
            if ( BLEDevScanCache[scan_cursor]->manufid > -1 ) {
              DB.getVendor( BLEDevScanCache[scan_cursor]->manufid, BLEDevScanCache[scan_cursor]->manufname );
            }
            BLEDevScanCache[scan_cursor]->is_anonymous = BLEDevHelper.isAnonymous( BLEDevScanCache[scan_cursor] );
            log_i(  "  stored and populated #%02d : %s", scan_cursor, advertisedDevice->getName().c_str());
          } else {
            log_i(  "  stored #%02d : %s", scan_cursor, advertisedDevice->getName().c_str());
          }
          scan_cursor++;
          processedDevicesCount++;
        }
        if ( scan_cursor == MAX_DEVICES_PER_SCAN ) {
          onScanDone = true;
        }
      } else {
        onScanDone = true;
      }
      if ( onScanDone ) {
        advertisedDevice->getScan()->stop();
        scan_cursor = 0;
        if ( SCAN_DURATION - 1 >= MIN_SCAN_DURATION ) {
          SCAN_DURATION--;
        }
      }
      foundDeviceToggler = !foundDeviceToggler;
      if (foundDeviceToggler) {
        //UI.BLEStateIconSetColor(BLE_GREEN);
        BLEActivityIcon.setStatus( ICON_STATUS_ADV_WHITELISTED );
      } else {
        //UI.BLEStateIconSetColor(BLE_DARKGREEN);
        BLEActivityIcon.setStatus( ICON_STATUS_ADV_SCAN );
      }
      if( scanShouldStop ) {
        advertisedDevice->getScan()->stop();
      }
    }
};

FoundDeviceCallbacks *FoundDeviceCallback;// = new FoundDeviceCallbacks(); // collect/store BLE data


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

struct ToggleTpl {
  const char *name;
  bool &flag;
};

ToggleTpl* TogglableProps;
uint16_t Tsize = 0;


class BLEScanUtils {

  public:

    void init() {

      BLEDevice::init( PLATFORM_NAME " BLE Collector");
      getPrefs(); // load prefs from NVS
      UI.init(); // launch all UI tasks
      VendorFilterIcon.setStatus( UI.filterVendors ? ICON_STATUS_filter : ICON_STATUS_filter_unset );
      if ( ! DB.init() ) { // mount DB
        log_e("Error with .db files (not found or corrupted), starting BLE File Sharing");
        startSerialTask();
        takeMuxSemaphore();
        Out.scrollNextPage();
        Out.println();
        Out.scrollNextPage();
        UI.PrintFatalError( "[ERROR]: .db files not found" );
        giveMuxSemaphore();
        startFileSharingServer();

        #ifdef WITH_WIFI
          startAlternateSourceTask( NULL );
        #endif

      } else {
        WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
        startSerialTask();
        startScanCB();
        RamCacheReady = true;
      }
    }

    #ifdef WITH_WIFI

      static void setAlternateSource( void * param = NULL ) {
        unsigned long beggingStart = millis();
        while(! isFileSharingClientConnected ) {
          if( beggingStart + 60000 < millis() ) {
            // waited 1mn for db file via ble, nothing came out, try WiFi
            stopBLE();
            break;
          }
          vTaskDelay(100);
        }
        vTaskDelete( NULL );
      }

      static void startAlternateSourceTask( void * param = NULL ) {
        xTaskCreatePinnedToCore( setAlternateSource, "setAlternateSource", 16000, NULL, 2, NULL, TASKLAUNCHER_CORE );
      }

      static void setWiFiSSID( void * param = NULL ) {
        if( param == NULL ) return;
        sprintf( WiFi_SSID, "%s", (const char*)param);
      }

      static void setWiFiPASS( void * param = NULL ) {
        if( param == NULL ) return;
        sprintf( WiFi_PASS, "%s", (const char*)param);
      }

      static void stopBLECB( void * param = NULL ) {
        stopScanCB();
        xTaskCreatePinnedToCore( stopBLE, "stopBLE", 8192, NULL, 5, NULL, TASKLAUNCHER_CORE ); /* last = Task Core */
      }

      static void stopBLE( void * param = NULL ) {
        log_w("[Free Heap: %d] Deleting BLE Tasks", freeheap);

        stopBLETasks();

        log_w("[Free Heap: %d] Shutting Down BlueTooth LE", freeheap);

        log_w("[Free Heap: %d] esp_bt_controller_disable()", freeheap);
        esp_bt_controller_disable();

        log_w("[Free Heap: %d] esp_bt_controller_deinit()", freeheap);
        esp_bt_controller_deinit() ;

        log_w("[Free Heap: %d] esp_bt_mem_release(ESP_BT_MODE_BTDM)", freeheap);
        esp_bt_mem_release(ESP_BT_MODE_BTDM);

        log_w("[Free Heap: %d] BT Shutdown finished", freeheap);

        xTaskCreatePinnedToCore( startWifi, "startWifi", 16384, NULL, 16, NULL, TASKLAUNCHER_CORE ); /* last = Task Core */

        while( WiFiStarted == false ) {
          // TODO: timeout this
          vTaskDelay( 100 );
        }

        NTPDateSet = false;
        xTaskCreatePinnedToCore( startNTPUpdater, "startNTPUpdater", 16384, NULL, 16, NULL, TASKLAUNCHER_CORE ); /* last = Task Core */
        while( NTPDateSet == false ) {
          // TODO: timeout this
          vTaskDelay( 100 );
        }

        if( DB.initDone ) {
          if( fileSharingEnabled ) {
            log_w("DONOR mode: some db files will be shared via FTP");
            xTaskCreatePinnedToCore( startFtpServer, "startFtpServer", 16384, NULL, 16, NULL, TASKLAUNCHER_CORE ); /* last = Task Core */
          }
        } else {
          log_w("HOBO mode: some db files are missing, will download...");
          xTaskCreatePinnedToCore( runWifiDownloader, "runWifiDownloader", 16384, NULL, 16, NULL, WIFITASK_CORE ); /* last = Task Core */
        }

        xTaskCreatePinnedToCore( stopWiFi, "stopWiFi", 8192, NULL, 5, NULL, TASKLAUNCHER_CORE ); /* last = Task Core */
        while( WiFiStarted == true ) {
          // TODO: timeout this
          vTaskDelay( 100 );
        } // wait until wifi stopped
        log_w("Restarting");
        ESP.restart();
        vTaskDelete( NULL );
      }


      static void startWifi( void * param = NULL ) {
        WiFi.mode(WIFI_STA);
        Serial.println(WiFi.macAddress());
        if( String( WiFi_SSID ) !="" && String( WiFi_PASS ) !="" ) {
          WiFi.begin( WiFi_SSID, WiFi_PASS );
        } else {
          WiFi.begin();
        }
        while(WiFi.status() != WL_CONNECTED) {
          log_e("Not connected");
          delay(1000);
        }
        log_w("Connected!");
        Serial.print("Connected to ");
        Serial.println(WiFi_SSID);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.println("");

        WiFiStarted = true;

        vTaskDelete( NULL );
      }

      static void stopWiFi( void* param = NULL ) {
        log_w("Stopping WiFi");
        WiFi.mode(WIFI_OFF);
        WiFiStarted = false;
        vTaskDelete( NULL );
      }


      static void startNTPUpdater( void * param ) {
        NTPDateSet = getNTPTime();
        vTaskDelete( NULL );
      }

      static void startFtpServer( void * param ) {
        /*
          $ lftp ftp://esp32@esp32-blecollector
          Password: esp32
          lftp:~> set ftp:passive-mode on
          lftp:~> set ftp:use-feat false
          lftp:~> set ftp:ssl-allow false
          lftp:~> mirror /db
        */
        FtpServer ftpSrv( BLE_FS );
        ftpSrv.onDisconnect = ESP_Restart;
        ftpSrv.begin("esp32","esp32"); // username, password for ftp.  set ports in ESP32FtpServer.h  (default 21, 50009 for PASV)
        while (1) {
          ftpSrv.handleFTP();
          vTaskDelay(1);
        }
        vTaskDelete( NULL );
      }

      static void runWifiDownloader( void * param ) {

        vTaskDelay(100);
        BLE_FS.begin();

        if( !DB.checkOUIFile() ) {
          if( ! wget( MAC_OUI_NAMES_DB_URL, BLE_FS, MAC_OUI_NAMES_DB_FS_PATH ) ) {
            log_e("Failed to download %s from url %s", MAC_OUI_NAMES_DB_FS_PATH, MAC_OUI_NAMES_DB_URL );
          } else {
            log_w("Successfully downloaded %s from url %s", MAC_OUI_NAMES_DB_FS_PATH, MAC_OUI_NAMES_DB_URL );
          }
        } else {
          log_w("Skipping download for %s file ", MAC_OUI_NAMES_DB_FS_PATH );
        }

        if( !DB.checkVendorFile() ) {
          if( ! wget( BLE_VENDOR_NAMES_DB_URL, BLE_FS, BLE_VENDOR_NAMES_DB_FS_PATH ) ) {
            log_e("Failed to download %s from url %s", BLE_VENDOR_NAMES_DB_FS_PATH, BLE_VENDOR_NAMES_DB_URL );
          } else {
            log_w("Successfully downloaded %s from url %s", BLE_VENDOR_NAMES_DB_FS_PATH, BLE_VENDOR_NAMES_DB_URL );
          }
        } else {
          log_w("Skipping download for %s file ", MAC_OUI_NAMES_DB_FS_PATH );
        }

        ESP_Restart();
        vTaskDelete( NULL );

      }

      static bool /*yolo*/wget( const char* url, fs::FS &fs, const char* path ) {

        WiFiClientSecure *client = new WiFiClientSecure;
        client->setCACert( NULL ); // yolo security

        const char* UserAgent = "ESP32HTTPClient";

        http.setUserAgent( UserAgent );
        http.setConnectTimeout( 10000 ); // 10s timeout = 10000

        if( ! http.begin(*client, url ) ) {
          log_e("Can't open url %s", url );
          return false;
        }

        const char * headerKeys[] = {"location", "redirect"};
        const size_t numberOfHeaders = 2;
        http.collectHeaders(headerKeys, numberOfHeaders);

        log_w("URL = %s", url);

        int httpCode = http.GET();

        // file found at server
        if (httpCode == HTTP_CODE_FOUND || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String newlocation = "";
          for(int i = 0; i< http.headers(); i++) {
            String headerContent = http.header(i);
            if( headerContent !="" ) {
              newlocation = headerContent;
              Serial.printf("%s: %s\n", headerKeys[i], headerContent.c_str());
            }
          }

          http.end();
          if( newlocation != "" ) {
            log_w("Found 302/301 location header: %s", newlocation.c_str() );
            return wget( newlocation.c_str(), fs, path );
          } else {
            log_e("Empty redirect !!");
            return false;
          }
        }

        WiFiClient *stream = http.getStreamPtr();

        if( stream == nullptr ) {
          http.end();
          log_e("Connection failed!");
          return false;
        }

        File outFile = fs.open( path, FILE_WRITE );
        if( ! outFile ) {
          log_e("Can't open %s file to save url %s", path, url );
          return false;
        }

        uint8_t buff[512] = { 0 };
        size_t sizeOfBuff = sizeof(buff);
        int len = http.getSize();
        int bytesLeftToDownload = len;
        int bytesDownloaded = 0;

        while(http.connected() && (len > 0 || len == -1)) {
          size_t size = stream->available();
          if(size) {
            // read up to 512 byte
            int c = stream->readBytes(buff, ((size > sizeOfBuff) ? sizeOfBuff : size));
            outFile.write( buff, c );
            bytesLeftToDownload -= c;
            bytesDownloaded += c;
            Serial.printf("%d bytes left\n", bytesLeftToDownload );
            float progress = (((float)bytesDownloaded / (float)len) * 100.00);
            UI.PrintProgressBar( progress, 100.0 );
          }
        }
        outFile.close();
        return fs.exists( path );
      }


    #endif // ifdef WITH_WIFI


    static void stopBLETasks( void * param = NULL ) {
      if( timeServerIsRunning )            destroyTaskNow( TimeServerTaskHandle );
      if( timeClientisStarted )            destroyTaskNow( TimeClientTaskHandle );
      if( fileSharingServerTaskIsRunning ) destroyTaskNow( FileServerTaskHandle );
      if( fileSharingClientTaskIsRunning ) destroyTaskNow( FileClientTaskHandle );
    }

    static void startScanCB( void * param = NULL ) {

      if( timeClientisStarted )            destroyTaskNow( TimeClientTaskHandle );
      if( fileSharingServerTaskIsRunning ) destroyTaskNow( FileServerTaskHandle );
      if( fileSharingClientTaskIsRunning ) destroyTaskNow( FileClientTaskHandle );

      BLEDevice::setMTU(100);
      if ( !scanTaskRunning ) {
        log_d("Starting scan" );
        uint16_t stackSize = DB.hasPsram ? 5120 : 5120;
        xTaskCreatePinnedToCore( scanTask, "scanTask", stackSize, NULL, 8, NULL, SCANTASK_CORE ); /* last = Task Core */
        while ( scanTaskStopped ) {
          log_d("Waiting for scan to start...");
          vTaskDelay(1000);
        }
        Serial.println("Scan started...");
        UI.headerStats("Scan started...");
      }
    }

    static void stopScanCB( void * param = NULL) {
      if ( scanTaskRunning ) {
        log_d("Stopping scan" );
        scanTaskRunning = false;
        BLEDevice::getScan()->stop();
        while (!scanTaskStopped) {
          log_d("Waiting for scan to stop...");
          vTaskDelay(1000);
        }
        Serial.println("Scan stopped...");
        UI.headerStats("Scan stopped...");
      }
    }

    static void restartCB( void * param = NULL ) {
      // detach from this thread before it's destroyed
      xTaskCreatePinnedToCore( doRestart, "doRestart", 16384, param, 5, NULL, TASKLAUNCHER_CORE ); // last = Task Core
    }

    static void doRestart( void * param = NULL ) {
      // "restart now" command skips db replication
      stopScanCB();
      stopBLETasks();

      if ( param != NULL && strcmp( "now", (const char*)param ) != 0 ) {
        DB.updateDBFromCache( BLEDevRAMCache, false, false );
      }

      log_w("Will restart");
      //tft.writeCommand( 0x01 ); // force display reset
      //digitalWrite( 33, HIGH );
      delay( 50 );
      ESP.restart();
    }

    static void startFileSharingServer( void * param = NULL ) {
      if ( ! fileDownloadingEnabled ) {
        fileDownloadingEnabled = true;
        xTaskCreatePinnedToCore( startFileSharingServerTask, "startFileSharingServerTask", 2048, param, 0, NULL, TASKLAUNCHER_CORE ); // last = Task Core
      }
    }

    static void startFileSharingServerTask( void * param = NULL) {
      bool scanWasRunning = scanTaskRunning;
      int8_t oldrole = BLERoleIcon.status;
      if ( fileSharingServerTaskIsRunning ) {
        log_e("FileSharingClientTask already running!");
        vTaskDelete( NULL );
        return;
      }
      if ( scanTaskRunning ) stopScanCB();

      stopBLETasks();

      fileSharingServerTaskIsRunning = true;
      BLERoleIcon.setStatus( ICON_STATUS_ROLE_FILE_SEEKING );
      xTaskCreatePinnedToCore( FileSharingServerTask, "FileSharingServerTask", 12000, NULL, 5, &FileServerTaskHandle, FILESHARETASK_CORE );
      if ( scanWasRunning ) {
        while ( fileSharingServerTaskIsRunning ) {
          vTaskDelay( 1000 );
        }
        log_w("Resuming operations after FileSharingServerTask");
        fileDownloadingEnabled = false;
        startScanCB();
        BLERoleIcon.setStatus( oldrole );
      } else {
        log_w("FileSharingServerTask started with no held task");
      }
      vTaskDelete( NULL );
    }

    static void setFileSharingClientOn( void * param = NULL ) {
      fileSharingEnabled = true;
    }

    static void startFileSharingClient( void * param = NULL ) {
      bool scanWasRunning = scanTaskRunning;
      int8_t oldrole = BLERoleIcon.status;
      fileSharingClientStarted = true;
      if ( scanTaskRunning ) stopScanCB();

      stopBLETasks();

      fileSharingClientTaskIsRunning = true;
      BLERoleIcon.setStatus( ICON_STATUS_ROLE_FILE_SHARING );
      xTaskCreatePinnedToCore( FileSharingClientTask, "FileSharingClientTask", 12000, param, 5, &FileClientTaskHandle, FILESHARETASK_CORE ); // last = Task Core
      if ( scanWasRunning ) {
        while ( fileSharingClientTaskIsRunning ) {
          vTaskDelay( 1000 );
        }
        log_w("Resuming operations after FileSharingClientTask");
        fileSharingEnabled = false;
        startScanCB();
        BLERoleIcon.setStatus( oldrole );
      } else {
        log_w("FileSharingClientTask spawned with no held task");
      }
      vTaskDelete( NULL );
    }

    static void setTimeClientOn( void * param = NULL ) {
      if( !timeClientisStarted ) {
        timeClientisStarted = true;
        xTaskCreatePinnedToCore( startTimeClient, "startTimeClient", 2560, param, 0, NULL, TASKLAUNCHER_CORE ); // last = Task Core
      } else {
        log_w("startTimeClient already called, time is also about patience");
      }
    }

    static void startTimeClient( void * param = NULL ) {
      bool scanWasRunning = scanTaskRunning;
      int8_t oldrole = BLERoleIcon.status;
      ForceBleTime = false;
      while ( ! foundTimeServer ) {
        vTaskDelay( 100 );
      }
      if ( scanTaskRunning ) stopScanCB();
      BLERoleIcon.setStatus( ICON_STATUS_ROLE_CLOCK_SEEKING );
      xTaskCreatePinnedToCore( TimeClientTask, "TimeClientTask", 2560, NULL, 5, &TimeClientTaskHandle, TIMECLIENTTASK_CORE ); // TimeClient task prefers core 0
      if ( scanWasRunning ) {
        while ( timeClientisRunning ) {
          vTaskDelay( 1000 );
        }
        log_w("Resuming operations after TimeClientTask");
        timeClientisStarted = false;
        //DB.maintain();
        startScanCB();
        BLERoleIcon.setStatus( oldrole );
      } else {
        log_w("TimeClientTask started with no held task");
      }
      vTaskDelete( NULL );
    }

    static void setTimeServerOn( void * param = NULL ) {
      if( !timeServerStarted ) {
        timeServerStarted = true;
        xTaskCreatePinnedToCore( startTimeServer, "startTimeServer", 8192, param, 0, NULL, TASKLAUNCHER_CORE ); // last = Task Core
      }
    }

    static void startTimeServer( void * param = NULL ) {
      bool scanWasRunning = scanTaskRunning;
      if ( scanTaskRunning ) stopScanCB();
      // timeServer runs forever
      timeServerIsRunning = true;
      timeServerStarted   = false; // will be updated from task
      BLERoleIcon.setStatus(ICON_STATUS_ROLE_CLOCK_SHARING );
      UI.headerStats( "Starting Time Server" );
      vTaskDelay(1);
      xTaskCreatePinnedToCore( TimeServerTask, "TimeServerTask", 4096, NULL, 1, &TimeServerTaskHandle, TIMESERVERTASK_CORE ); // TimeServerTask prefers core 1
      log_w("TimeServerTask started");
      if ( scanWasRunning ) {
        while( ! timeServerStarted ) {
          vTaskDelay( 100 );
        }
        startScanCB();
      }
      vTaskDelete( NULL );
    }

    static void setBrightnessCB( void * param = NULL ) {
      if( param != NULL ) {
        UI.brightness = atoi( (const char*) param );
      }
      takeMuxSemaphore();
      tft_setBrightness( UI.brightness );
      giveMuxSemaphore();
      setPrefs();
      log_w("Brightness is now at %d", UI.brightness);
    }

    static void resetCB( void * param = NULL ) {
      DB.needsReset = true;
      Serial.println("DB Scheduled for reset");
      stopScanCB();
      DB.maintain();
      delay(100);
      //startScanCB();
    }

    static void pruneCB( void * param = NULL ) {
      DB.needsPruning = true;
      Serial.println("DB Scheduled for pruning");
      stopScanCB();
      DB.maintain();
      startScanCB();
    }

    static void toggleFilterCB( void * param = NULL ) {
      UI.filterVendors = ! UI.filterVendors;
      VendorFilterIcon.setStatus( UI.filterVendors ? ICON_STATUS_filter : ICON_STATUS_filter_unset );
      UI.cacheStats(); // refresh icon
      setPrefs(); // save prefs
      Serial.printf("UI.filterVendors = %s\n", UI.filterVendors ? "true" : "false" );
    }

    static void startDumpCB( void * param = NULL ) {
      DBneedsReplication = true;
      bool scanWasRunning = scanTaskRunning;
      if ( scanTaskRunning ) stopScanCB();
      DB.maintain();
      while ( DBneedsReplication ) {
        vTaskDelay(1000);
      }
      if ( scanWasRunning ) startScanCB();
    }

    static void toggleEchoCB( void * param = NULL ) {
      Out.serialEcho = !Out.serialEcho;
      setPrefs();
      Serial.printf("Out.serialEcho = %s\n", Out.serialEcho ? "true" : "false" );
    }

    static void rmFileCB( void * param = NULL ) {
      xTaskCreatePinnedToCore(rmFileTask, "rmFileTask", 5000, param, 2, NULL, TASKLAUNCHER_CORE ); /* last = Task Core */
    }

    static void rmFileTask( void * param = NULL ) {
      // YOLO style
      isQuerying = true;
      if ( param != NULL ) {
        if ( BLE_FS.remove( (const char*)param ) ) {
          Serial.printf("File %s deleted\n", (const char*)param );
        } else {
          Serial.printf("File %s could not be deleted\n", (const char*)param );
        }
      } else {
        Serial.println("Nothing to delete");
      }
      isQuerying = false;
      vTaskDelete( NULL );
    }

    static void screenShowCB( void * param = NULL ) {
      xTaskCreate(screenShowTask, "screenShowTask", 16000, param, 2, NULL);
    }

    static void screenShowTask( void * param = NULL ) {
      UI.screenShow( param );
      vTaskDelete(NULL);
    }

    static void screenShotCB( void * param = NULL ) {
      xTaskCreate(screenShotTask, "screenShotTask", 16000, NULL, 2, NULL);
    }

    static void screenShotTask( void * param = NULL ) {
      if( !UI.ScreenShotLoaded ) {
        log_w("Cold ScreenShot");
        M5.ScreenShot.init( &tft, BLE_FS );
        if( M5.ScreenShot.begin() ) {
          UI.ScreenShotLoaded = true;
          UI.screenShot();
        } else {
          log_e("Sorry, ScreenShot is not available");
        }
      } else {
        log_w("Hot ScreenShot");
        UI.screenShot();
      }
      vTaskDelete(NULL);
    }

    static void listDirCB( void * param = NULL ) {
      xTaskCreatePinnedToCore(listDirTask, "listDirTask", 5000, param, 8, NULL, TASKLAUNCHER_CORE ); /* last = Task Core */
    }

    static void listDirTask( void * param = NULL ) {
      isQuerying = true;
      bool scanWasRunning = scanTaskRunning;
      if ( scanTaskRunning ) stopScanCB();
      if( param != NULL ) {
        if(! BLE_FS.exists( (const char*)param ) ) {
          Serial.printf("Directory %s does not exist\n", (const char*)param );
        } else {
          takeMuxSemaphore();
          listDir(BLE_FS, (const char*)param, 0, DB.BLEMacsDbFSPath);
          giveMuxSemaphore();
        }
      } else {
        takeMuxSemaphore();
        listDir(BLE_FS, "/", 0, DB.BLEMacsDbFSPath);
        giveMuxSemaphore();
      }
      if ( scanWasRunning ) startScanCB();
      isQuerying = false;
      vTaskDelete( NULL );
    }

    static void toggleCB( void * param = NULL ) {
      bool setbool = true;
      if ( param != NULL ) {
        //
      } else {
        setbool = false;
        Serial.println("\nCurrent property values:");
      }
      for ( uint16_t i = 0; i < Tsize; i++ ) {
        if ( setbool ) {
          if ( strcmp( TogglableProps[i].name, (const char*)param ) == 0 ) {
            TogglableProps[i].flag = !TogglableProps[i].flag;
            Serial.printf("Toggled flag %s to %s\n", TogglableProps[i].name, TogglableProps[i].flag ? "true" : "false");
          }
        } else {
          Serial.printf("  %24s : [%s]\n", TogglableProps[i].name, TogglableProps[i].flag ? "true" : "false");
        }
      }
    }

    static void nullCB( void * param = NULL ) {
      if ( param != NULL ) {
        Serial.printf("nullCB param: %s\n", (const char*)param);
      }
      // zilch, niente, nada, que dalle, nothing
    }
    static void startSerialTask() {
      serialBuffer = (char*)calloc( SERIAL_BUFFER_SIZE, sizeof(char) );
      tempBuffer   = (char*)calloc( SERIAL_BUFFER_SIZE, sizeof(char) );
      xTaskCreatePinnedToCore(serialTask, "serialTask", 8192 + SERIAL_BUFFER_SIZE, NULL, 0, NULL, SERIALTASK_CORE ); /* last = Task Core */
    }

    static void serialTask( void * parameter ) {
      CommandTpl Commands[] = {
        { "help",          nullCB,                 "Print this list" },
        { "start",         startScanCB,            "Start/resume scan" },
        { "stop",          stopScanCB,             "Stop scan" },
        { "toggleFilter",  toggleFilterCB,         "Toggle vendor filter on the TFT (persistent)" },
        { "toggleEcho",    toggleEchoCB,           "Toggle BLECards in the Serial Console (persistent)" },
        { "dump",          startDumpCB,            "Dump returning BLE devices to the display and updates DB" },
        { "setBrightness", setBrightnessCB,        "Set brightness to [value] (0-255)" },
        { "ls",            listDirCB,              "Show [dir] Content on the SD" },
        { "rm",            rmFileCB,               "Delete [file] from the SD" },
        { "restart",       restartCB,              "Restart BLECollector ('restart now' to skip replication)" },
        #if HAS_EXTERNAL_RTC
          { "bleclock",      setTimeServerOn,        "Broadcast time to another BLE Device (implicit)" },
          { "bletime",       setTimeClientOn,        "Get time from another BLE Device (explicit)" },
        #else
          { "bleclock",      setTimeServerOn,        "Broadcast time to another BLE Device (explicit)" },
          { "bletime",       setTimeClientOn,        "Get time from another BLE Device (implicit)" },
        #endif
        { "blereceive",    startFileSharingServer, "Update .db files from another BLE app"},
        { "blesend",       setFileSharingClientOn, "Share .db files with anothe BLE app" },
        { "screenshot",    screenShotCB,           "Make a screenshot and save it on the SD" },
        { "screenshow",    screenShowCB,           "Show screenshot" },
        { "toggle",        toggleCB,               "toggle a bool value" },
        #if HAS_GPS
          { "gpstime",       setGPSTime,     "sync time from GPS" },
        #endif
        { "resetDB",       resetCB,        "Hard Reset DB + forced restart" },
        { "pruneDB",       pruneCB,        "Soft Reset DB without restarting (hopefully)" },
        #ifdef WITH_WIFI
          { "stopBLE",       stopBLECB,      "Stop BLE and start WiFi (experimental)" },
          { "setWiFiSSID",   setWiFiSSID,    "Set WiFi SSID" },
          { "setWiFiPASS",   setWiFiPASS,    "Set WiFi Password" },
        #endif
      };

      SerialCommands = Commands;
      Csize = (sizeof(Commands) / sizeof(Commands[0]));

      ToggleTpl ToggleProps[] = {
        { "Out.serialEcho",      Out.serialEcho },
        { "DB.needsReset",       DB.needsReset },
        { "DBneedsReplication",  DBneedsReplication },
        { "DB.needsPruning",     DB.needsPruning },
        { "TimeIsSet",           TimeIsSet },
        { "foundTimeServer",     foundTimeServer },
        { "RTCisRunning",        RTCisRunning },
        { "ForceBleTime",        ForceBleTime },
        { "DayChangeTrigger",    DayChangeTrigger },
        { "HourChangeTrigger",   HourChangeTrigger },
        { "fileSharingEnabled",  fileSharingEnabled },
        { "timeServerIsRunning", timeServerIsRunning }
      };
      TogglableProps = ToggleProps;
      Tsize = (sizeof(ToggleProps) / sizeof(ToggleProps[0]));

      if (resetReason != 12) { // HW Reset
        runCommand( (char*)"help" );
        //runCommand( (char*)"toggle" );
        //runCommand( (char*)"ls" );
      }

      #if HAS_EXTERNAL_RTC
      if( TimeIsSet ) {
        // auto share time if available
        // TODO: fix this, broken since NimBLE
        // runCommand( (char*)"bleclock" );
      }
      #endif

      #if HAS_GPS
        GPSInit();
      #endif

      static byte idx = 0;
      char lf = '\n';
      char cr = '\r';
      char c;
      unsigned long lastHidCheck = millis();
      while ( 1 ) {
        while (Serial.available() > 0) {
          c = Serial.read();
          if (c != cr && c != lf) {
            serialBuffer[idx] = c;
            idx++;
            if (idx >= SERIAL_BUFFER_SIZE) {
              idx = SERIAL_BUFFER_SIZE - 1;
            }
          } else {
            serialBuffer[idx] = '\0'; // null terminate
            memcpy( tempBuffer, serialBuffer, idx + 1 );
            runCommand( tempBuffer );
            idx = 0;
          }
          vTaskDelay(1);
        }
        #if HAS_GPS
          GPSRead();
        #endif

        if( hasHID() ) {
          if( lastHidCheck + 150 < millis() ) {
            M5.update();
            if( M5.BtnA.wasPressed() ) {
              UI.brightness -= UI.brightnessIncrement;
              setBrightnessCB();
            }
            if( M5.BtnB.wasPressed() ) {
              UI.brightness += UI.brightnessIncrement;
              setBrightnessCB();
            }
            if( M5.BtnC.wasPressed() ) {
              toggleFilterCB();
            }
            lastHidCheck = millis();
          }
        } else if( hasXPaxShield() ) {

          takeMuxSemaphore();
          XPadShield.update();
          giveMuxSemaphore();

          if( XPadShield.wasPressed() ) { // on release

            // XPadShield.BtnA.wasPressed(); would work but xpad support simultaneous buttons push
            // so a stricter approach is chosen

            switch( XPadShield.state ) {
              case 0x01: // down
                log_w("XPadShield->down");
                UI.brightness -= UI.brightnessIncrement;
                setBrightnessCB();
              break;
              case 0x02: // up
                log_w("XPadShield->up");
                UI.brightness += UI.brightnessIncrement;
                setBrightnessCB();
              break;
              case 0x04: // right
                log_w("XPadShield->right");
              break;
              case 0x08: // leftheader_jpg
                log_w("XPadShield->left");
              break;
              case 0x10: // B
                log_w("XPadShield->B");
                runCommand( (char*)"toggleFilter");
              break;
              case 0x20: // A
                log_w("XPadShield->A");
                if ( scanTaskRunning ) {
                  runCommand( (char*)"stop");
                } else {
                  runCommand( (char*)"start");
                }
              break;
              case 0x40: // C
                log_w("XPadShield->C");
                runCommand( (char*)"toggleFilter");
              break;
              case 0x80: // D
                log_w("XPadShield->D");
              break;
              case 0xff: // probably bad I2C bus otherwise sour fingers
              case 0x00: // no button pushed
                // ignore
              break;
              default: // simultaneous buttons push
                log_w("XPadShield->Combined: 0x%02x", XPadShield.state);
              break;
            }
          }

        }

        vTaskDelay(1);
      }
    }


    static void runCommand( char* command ) {
      if ( isEmpty( command ) ) return;
      if ( strcmp( command, "help" ) == 0 ) {
        Serial.println("\nAvailable Commands:\n");
        for ( uint16_t i = 0; i < Csize; i++ ) {
          Serial.printf("  %02d) %16s : %s\n", i + 1, SerialCommands[i].command, SerialCommands[i].description);
        }
        Serial.println();
      } else {
        char *token;
        char delim[2];
        char *args;
        bool has_args = false;
        strncpy(delim, " ", 2); // strtok_r needs a null-terminated string

        if ( strstr(command, delim) ) {
          // turn command into token/arg
          token = strtok_r(command, delim, &args); // Search for command at start of buffer
          if ( token != NULL ) {
            has_args = true;
            //Serial.printf("[%s] Found arg for token '%s' : %s\n", command, token, args);
          }
        }
        for ( uint16_t i = 0; i < Csize; i++ ) {
          if ( strcmp( SerialCommands[i].command, command ) == 0 ) {
            if ( has_args ) {
              Serial.printf( "Running '%s %s' command\n", token, args );
              SerialCommands[i].cb.function( args );
            } else {
              Serial.printf( "Running '%s' command\n", SerialCommands[i].command );
              SerialCommands[i].cb.function( NULL );
            }
            //sprintf(command, "%s", "");
            return;
          }
        }
        Serial.printf( "Command '%s' not found\n", command );
      }
    }


    static void scanInit() {
      UI.update(); // run after-scan display stuff
      DB.maintain();
      scanTaskRunning = true;
      scanTaskStopped = false;

      if ( FoundDeviceCallback == NULL ) {
        FoundDeviceCallback = new FoundDeviceCallbacks(); // collect/store BLE data
      }
      pBLEScan = BLEDevice::getScan(); //create new scan
      pBLEScan->setAdvertisedDeviceCallbacks( FoundDeviceCallback );

      pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
      pBLEScan->setInterval(0x50); // 0x50
      pBLEScan->setWindow(0x30); // 0x30
    }


    static void scanDeInit() {
      scanTaskStopped = true;
      delete FoundDeviceCallback; FoundDeviceCallback = NULL;
    }


    static void scanTask( void * parameter ) {
      scanInit();
      byte onAfterScanStep = 0;
      while ( scanTaskRunning ) {
        if ( onAfterScanSteps( onAfterScanStep, scan_cursor ) ) continue;
        dumpStats("BeforeScan::");
        onBeforeScan();
        pBLEScan->start(SCAN_DURATION);
        onAfterScan();
        //DB.maintain();
        dumpStats("AfterScan:::");
        scan_rounds++;
      }
      scanDeInit();
      vTaskDelete( NULL );
    }


    static bool onAfterScanSteps( byte &onAfterScanStep, uint16_t &scan_cursor ) {
      switch ( onAfterScanStep ) {
        case POPULATE: // 0
          onScanPopulate( scan_cursor ); // OUI / vendorname / isanonymous
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
          if ( onScanPropagate( scan_cursor ) ) { // copy to DB / cache
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
      if ( onScanPopulated ) {
        log_v("%s", " onScanPopulated = true ");
        return false;
      }
      if ( _scan_cursor >= devicesCount) {
        onScanPopulated = true;
        log_d("%s", "done all");
        return false;
      }
      if ( isEmpty( BLEDevScanCache[_scan_cursor]->address ) ) {
        log_w("empty addess");
        return true; // end of cache
      }
      populate( BLEDevScanCache[_scan_cursor] );
      return true;
    }


    static bool onScanIfExists( int _scan_cursor ) {
      if ( onScanPostPopulated ) {
        log_v("onScanPostPopulated = true");
        return false;
      }
      if ( _scan_cursor >= devicesCount) {
        log_d("done all");
        onScanPostPopulated = true;
        return false;
      }
      int deviceIndexIfExists = -1;
      deviceIndexIfExists = getDeviceCacheIndex( BLEDevScanCache[_scan_cursor]->address );
      if ( deviceIndexIfExists > -1 ) {
        inCacheCount++;
        BLEDevRAMCache[deviceIndexIfExists]->hits++;
        if ( TimeIsSet ) {
          if ( BLEDevRAMCache[deviceIndexIfExists]->created_at.year() <= 1970 ) {
            BLEDevRAMCache[deviceIndexIfExists]->created_at = nowDateTime;
          }
          BLEDevRAMCache[deviceIndexIfExists]->updated_at = nowDateTime;
        }
        BLEDevHelper.mergeItems( BLEDevScanCache[_scan_cursor], BLEDevRAMCache[deviceIndexIfExists] ); // merge scan data into existing psram cache
        BLEDevHelper.copyItem( BLEDevRAMCache[deviceIndexIfExists], BLEDevScanCache[_scan_cursor] ); // copy back merged data for rendering
        log_i( "Device %d / %s exists in cache, increased hits to %d", _scan_cursor, BLEDevScanCache[_scan_cursor]->address, BLEDevScanCache[_scan_cursor]->hits );
      } else {
        if ( BLEDevScanCache[_scan_cursor]->is_anonymous ) {
          // won't land in DB (won't be checked either) but will land in cache
          uint16_t nextCacheIndex = BLEDevHelper.getNextCacheIndex( BLEDevRAMCache, BLEDevCacheIndex );
          BLEDevHelper.reset( BLEDevRAMCache[nextCacheIndex] );
          BLEDevScanCache[_scan_cursor]->hits++;
          BLEDevHelper.copyItem( BLEDevScanCache[_scan_cursor], BLEDevRAMCache[nextCacheIndex] );
          log_v( "Device %d / %s is anonymous, won't be inserted", _scan_cursor, BLEDevScanCache[_scan_cursor]->address, BLEDevScanCache[_scan_cursor]->hits );
        } else {
          deviceIndexIfExists = DB.deviceExists( BLEDevScanCache[_scan_cursor]->address ); // will load returning devices from DB if necessary
          if (deviceIndexIfExists > -1) {
            uint16_t nextCacheIndex = BLEDevHelper.getNextCacheIndex( BLEDevRAMCache, BLEDevCacheIndex );
            BLEDevHelper.reset( BLEDevRAMCache[nextCacheIndex] );
            BLEDevDBCache->hits++;
            if ( TimeIsSet ) {
              if ( BLEDevDBCache->created_at.year() <= 1970 ) {
                BLEDevDBCache->created_at = nowDateTime;
              }
              BLEDevDBCache->updated_at = nowDateTime;
            }
            BLEDevHelper.mergeItems( BLEDevScanCache[_scan_cursor], BLEDevDBCache ); // merge scan data into BLEDevDBCache
            BLEDevHelper.copyItem( BLEDevDBCache, BLEDevRAMCache[nextCacheIndex] ); // copy merged data to assigned psram cache
            BLEDevHelper.copyItem( BLEDevDBCache, BLEDevScanCache[_scan_cursor] ); // copy back merged data for rendering

            log_v( "Device %d / %s is already in DB, increased hits to %d", _scan_cursor, BLEDevScanCache[_scan_cursor]->address, BLEDevScanCache[_scan_cursor]->hits );
          } else {
            // will be inserted after rendering
            BLEDevScanCache[_scan_cursor]->in_db = false;
            log_v( "Device %d / %s is not in DB", _scan_cursor, BLEDevScanCache[_scan_cursor]->address );
          }
        }
      }
      return true;
    }


    static bool onScanRender( uint16_t _scan_cursor ) {
      if ( onScanRendered ) {
        log_v("onScanRendered = true");
        return false;
      }
      if ( _scan_cursor >= devicesCount) {
        log_v("done all");
        onScanRendered = true;
        return false;
      }
      UI.BLECardTheme.setTheme( IN_CACHE_ANON );
      BLEDevTmp = BLEDevScanCache[_scan_cursor];
      UI.printBLECard( (BlueToothDeviceLink){.cacheIndex=_scan_cursor,.device=BLEDevTmp} ); // render
      delay(1);
      sprintf( processMessage, processTemplateLong, "Rendered ", _scan_cursor + 1, " / ", devicesCount );
      UI.headerStats( processMessage );
      delay(1);
      UI.cacheStats();
      delay(1);
      UI.footerStats();
      delay(1);
      return true;
    }


    static bool onScanPropagate( uint16_t &_scan_cursor ) {
      if ( onScanPropagated ) {
        log_v("onScanPropagated = true");
        return false;
      }
      if ( _scan_cursor >= devicesCount) {
        log_v("done all");
        onScanPropagated = true;
        _scan_cursor = 0;
        return false;
      }
      //BLEDevScanCacheIndex = _scan_cursor;
      if ( isEmpty( BLEDevScanCache[_scan_cursor]->address ) ) {
        return true;
      }
      if ( BLEDevScanCache[_scan_cursor]->is_anonymous || BLEDevScanCache[_scan_cursor]->in_db ) { // don't DB-insert anon or duplicates
        sprintf( processMessage, processTemplateLong, "Released ", _scan_cursor + 1, " / ", devicesCount );
        if ( BLEDevScanCache[_scan_cursor]->is_anonymous ) AnonymousCacheHit++;
      } else {
        if ( DB.insertBTDevice( BLEDevScanCache[_scan_cursor] ) == DBUtils::INSERTION_SUCCESS ) {
          sprintf( processMessage, processTemplateLong, "Saved ", _scan_cursor + 1, " / ", devicesCount );
          log_d( "Device %d successfully inserted in DB", _scan_cursor );
          entries++;
        } else {
          log_e( "  [!!! BD INSERT FAIL !!!] Device %d could not be inserted", _scan_cursor );
          sprintf( processMessage, processTemplateLong, "Failed ", _scan_cursor + 1, " / ", devicesCount );
        }
      }
      BLEDevHelper.reset( BLEDevScanCache[_scan_cursor] ); // discard
      UI.headerStats( processMessage );
      return true;
    }


    static void onBeforeScan() {
      DB.maintain();
      UI.headerStats("Scan in progress");
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
      foundFileServer = false;
    }

    static void onAfterScan() {

      UI.stopBlink();

      if ( fileSharingEnabled ) { // found a peer to share with ?
        if( !fileSharingClientStarted ) { // fire file sharing client task
          if( fileServerBLEAddress != "" ) {
            UI.headerStats("File Sharing ...");
            log_w("Launching FileSharingClient Task");
            xTaskCreatePinnedToCore( startFileSharingClient, "startFileSharingClient", 2048, NULL, 5, NULL, TASKLAUNCHER_CORE );
            while( scanTaskRunning ) {
              vTaskDelay( 10 );
            }
            return;
          }
        }
      }

      if ( foundTimeServer && (!TimeIsSet || ForceBleTime) ) {
        if( ! timeClientisStarted ) {
          if( timeServerBLEAddress != "" ) {
            UI.headerStats("BLE Time sync ...");
            log_w("HOBO mode: found a peer with time provider service, launching BLE TimeClient Task");
            xTaskCreatePinnedToCore(startTimeClient, "startTimeClient", 2048, NULL, 0, NULL, TASKLAUNCHER_CORE ); /* last = Task Core */
            while( scanTaskRunning ) {
              vTaskDelay( 10 );
            }
            return;
          }
        }
      }

      UI.headerStats("Showing results ...");
      devicesCount = processedDevicesCount;
      BLEDevice::getScan()->clearResults();
      if ( devicesCount < MAX_DEVICES_PER_SCAN ) {
        if ( SCAN_DURATION + 1 < MAX_SCAN_DURATION ) {
          SCAN_DURATION++;
        }
      } else if ( devicesCount > MAX_DEVICES_PER_SCAN ) {
        if ( SCAN_DURATION - 1 >= MIN_SCAN_DURATION ) {
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

      UI.update();

    }


    static int getDeviceCacheIndex(const char* address) {
      if ( isEmpty( address ) )  return -1;
      for (int i = 0; i < BLEDEVCACHE_SIZE; i++) {
        if ( strcmp(address, BLEDevRAMCache[i]->address ) == 0  ) {
          BLEDevCacheHit++;
          log_v("[CACHE HIT] BLEDevCache ID #%s has %d cache hits", address, BLEDevRAMCache[i]->hits);
          return i;
        }
        delay(1);
      }
      return -1;
    }

    // used for serial debugging
    static void dumpStats(const char* prefixStr) {
      if (lastheap > freeheap) {
        // heap decreased
        sprintf(heapsign, "%s", "↘");
      } else if (lastheap < freeheap) {
        // heap increased
        sprintf(heapsign, "%s", "↗");
      } else {
        // heap unchanged
        sprintf(heapsign, "%s", "⇉");
      }
      if (lastscanduration > SCAN_DURATION) {
        sprintf(scantimesign, "%s", "↘");
      } else if (lastscanduration < SCAN_DURATION) {
        sprintf(scantimesign, "%s", "↗");
      } else {
        sprintf(scantimesign, "%s", "⇉");
      }

      lastheap = freeheap;
      lastscanduration = SCAN_DURATION;

      log_i("%s[Scan#%02d][%s][Duration%s%d][Processed:%d of %d][Heap%s%d / %d] [Cache hits][BLEDevCards:%d][Anonymous:%d][Oui:%d][Vendor:%d]",
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
        BLEDevCacheHit,
        AnonymousCacheHit,
        OuiCacheHit,
        VendorCacheHit
       );
    }

  private:

    static void getPrefs() {
      preferences.begin("BLEPrefs", true);
      Out.serialEcho   = preferences.getBool("serialEcho", true);
      UI.filterVendors = preferences.getBool("filterVendors", false);
      UI.brightness    = preferences.getUChar("brightness", BASE_BRIGHTNESS);
      log_d("Defrosted brightness: %d", UI.brightness );
      preferences.end();
    }
    static void setPrefs() {
      preferences.begin("BLEPrefs", false);
      preferences.putBool("serialEcho", Out.serialEcho);
      preferences.putBool("filterVendors", UI.filterVendors );
      preferences.putUChar("brightness", UI.brightness );
      preferences.end();
    }

    // completes unpopulated fields of a given entry by performing DB oui/vendor lookups
    static void populate( BlueToothDevice *CacheItem ) {
      if ( strcmp( CacheItem->ouiname, "[unpopulated]" ) == 0 ) {
        log_d("  [populating OUI for %s]", CacheItem->address);
        DB.getOUI( CacheItem->address, CacheItem->ouiname );
      }
      if ( strcmp( CacheItem->manufname, "[unpopulated]" ) == 0 ) {
        if ( CacheItem->manufid != -1 ) {
          log_d("  [populating Vendor for :%d]", CacheItem->manufid );
          DB.getVendor( CacheItem->manufid, CacheItem->manufname );
        } else {
          BLEDevHelper.set( CacheItem, "manufname", '\0');
        }
      }
      CacheItem->is_anonymous = BLEDevHelper.isAnonymous( CacheItem );
      log_v("[populated :%s]", CacheItem->address);
    }

};




BLEScanUtils BLECollector;
