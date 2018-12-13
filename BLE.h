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
BLEAdvertisedDevice advertisedDevice;


static uint16_t processedDevicesCount = 0;
bool foundDeviceToggler = true;

class FoundDeviceCallback: public BLEAdvertisedDeviceCallbacks {

  //byte processedDevicesCount = 0;
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    //Serial.printf("Found %s \n", advertisedDevice.toString().c_str());
    if( onScanDone  ) return;

    if( scan_cursor < MAX_DEVICES_PER_SCAN ) {
      Serial.printf(  "  [%s] storing #%d : %s\n", __func__, scan_cursor, advertisedDevice.toString().c_str());
      BLEDevHelper.store( BLEDevTmpCache[scan_cursor], advertisedDevice );
      scan_cursor++;
      processedDevicesCount++;
    } else {
      advertisedDevice.getScan()->stop();
      onScanDone = true;
      scan_cursor = 0;
      if( SCAN_DURATION-1 >= MIN_SCAN_DURATION ) {
        SCAN_DURATION--;
      }
    }
    foundDeviceToggler = !foundDeviceToggler;
    if(foundDeviceToggler) {
      UI.setBLEStateIcon(WROVER_GREEN);
    } else {
      UI.setBLEStateIcon(WROVER_DARKGREEN);
    }
    vTaskDelay(100);
  }
};



class BLEScanUtils {

  public:

    void init() {
      UI.init();
      SDSetup();
      DB.init();

      #ifdef USE_NVS
        if ( resetReason == 12)  { // =  SW_CPU_RESET
          thaw(); // get leftovers from NVS
          //if( feed() ) { // got insertions in DB ?
            clearNVS(); // purge this
            //ESP.restart();
          //}
        } else {
          clearNVS();
          ESP.restart();
        }
      #endif
      
      WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

      #if   SCAN_MODE==SCAN_TASK_0
        xTaskCreatePinnedToCore(scanTask, "scanTask", 10000, NULL, 0, NULL, 0); /* last = Task Core */
      #elif SCAN_MODE==SCAN_TASK_1
        xTaskCreatePinnedToCore(scanTask, "scanTask", 10000, NULL, 0, NULL, 1); /* last = Task Core */
      #elif SCAN_MODE==SCAN_TASK
        //xTaskCreate(scanTask, "scanTask", 8000, NULL, 1, NULL);
        xTaskCreatePinnedToCore(scanTask, "scanTask", 16000, NULL, 5, NULL, 1); /* last = Task Core */
      #elif SCAN_MODE==SCAN_LOOP
        BLEDevice::init("");
        pBLEScan = BLEDevice::getScan(); //create new scan
        pBLEScan->setAdvertisedDeviceCallbacks( new FoundDeviceCallback() ); // memory leak ?
        pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
        pBLEScan->setInterval(0x50); // 0x50
        pBLEScan->setWindow(0x30); // 0x30
      #else
        #error "Unknown value for SCAN_MODE, see Setting.h"
      #endif
    }


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


    static int getDeviceCacheIndex(const char* address) {
      if( isEmpty( address ) )  return -1;
      for(int i=0; i<BLEDEVCACHE_SIZE; i++) {
        if( strcmp(address, BLEDevCache[i].address)==0  ) {
          BLEDevCache[i].hits++;
          BLEDevCacheHit++;
          //Serial.printf("[CACHE HIT] BLEDevCache ID #%s has %d cache hits\n", address, BLEDevCache[i].hits);
          return i;
        }
        delay(1);
      }
      return -1;
    }


    /* determines whether a device is worth saving or not */
    static bool isAnonymousDevice( BlueToothDevice *_BLEDevCache, uint16_t _index) {
      if( _index>=BLEDEVCACHE_SIZE ) {
        Serial.printf("[!!!isAnonymousDevice!!!] Ignored Invalid Cache Index :%d\n", _index);        
        return false;
      }
      if(_BLEDevCache[_index].uuid && strlen(_BLEDevCache[_index].uuid)>=0) return false; // uuid's are interesting, let's collect
      if(_BLEDevCache[_index].name && strlen(_BLEDevCache[_index].name)>0) return false; // has name, let's collect
      if(_BLEDevCache[_index].appearance!=0) return false; // has icon, let's collect
      if(strcmp(_BLEDevCache[_index].ouiname, "[unpopulated]")==0) return false; // don't know yet, let's keep
      if(strcmp(_BLEDevCache[_index].manufname, "[unpopulated]")==0) return false; // don't know yet, let's keep
      if(strcmp(_BLEDevCache[_index].ouiname, "[private]")==0 || isEmpty( _BLEDevCache[_index].ouiname ) ) return true; // don't care
      if(strcmp(_BLEDevCache[_index].manufname, "[unknown]")==0 || isEmpty( _BLEDevCache[_index].manufname ) ) return true; // don't care
      if( !isEmpty( _BLEDevCache[_index].manufname ) && !isEmpty( _BLEDevCache[_index].ouiname ) ) return false; // anonymous but qualified device, let's collect
      return true;
    }

    /* stores BLEDevice info in memory cache after optionnaly retrieving complementary data */
    static uint16_t store( BLEAdvertisedDevice &advertisedDevice, BlueToothDevice *_BLEDevCache, uint16_t _BLEDevCacheIndex, bool populate=true ) {
      if( _BLEDevCacheIndex>=BLEDEVCACHE_SIZE ) {
        Serial.printf("[!!!store!!!] Ignored Invalid Cache Index :%d\n", _BLEDevCacheIndex);        
        return _BLEDevCacheIndex;
      }
      //_BLEDevCache = getNextBLEDevCacheIndex();
      BLEDevHelper.reset( _BLEDevCache[_BLEDevCacheIndex]);//.reset();// avoid mixing new and old data
      BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "in_db", false);
      BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "address", advertisedDevice.getAddress().toString().c_str());
      BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "rssi", advertisedDevice.getRSSI());
      if( populate ) {
        DB.getOUI( _BLEDevCache[_BLEDevCacheIndex].address, _BLEDevCache[_BLEDevCacheIndex].ouiname );
      } else {
        BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "ouiname", "[unpopulated]");
      }
      if ( advertisedDevice.haveName() ) {
        BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "name", advertisedDevice.getName().c_str());
      } else {
        BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "name", '\0');
      }
      if ( advertisedDevice.haveAppearance() ) {
        BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "appearance", advertisedDevice.getAppearance());
      } else {
        BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "appearance", 0);
      }
      if ( advertisedDevice.haveManufacturerData() ) {
        uint8_t* mdp = (uint8_t*)advertisedDevice.getManufacturerData().data();
        //std::string md = advertisedDevice.getManufacturerData();
        //char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
        uint8_t vlsb = mdp[0];
        uint8_t vmsb = mdp[1];
        uint16_t vint = vmsb * 256 + vlsb;
        if( populate ) {
          DB.getVendor( vint, _BLEDevCache[_BLEDevCacheIndex].manufname );
        } else {
          BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "manufname", "[unpopulated]");
        }
        BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "manufid", vint);
      } else {
        BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "manufname", '\0');
        BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "manufid", -1);
      }
      if ( advertisedDevice.haveServiceUUID() ) {
        BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "uuid", advertisedDevice.getServiceUUID().toString().c_str());
      } else {
        BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "uuid", '\0');
      }
      return _BLEDevCacheIndex;          
    }

    /* cleanup NVS cache data */
    void clearNVS() {
      for( byte i=0; i<MAX_ITEMS_IN_PREFS; i++ ) {
        const char* freezenameTpl = "cache-%d";
        char freezenameStr[12] = {'\0'};
        sprintf(freezenameStr, freezenameTpl, i);
        //Serial.println("Clearing " + freezename);
        preferences.begin(freezenameStr, false);
        preferences.clear();
        preferences.end();
      }
      Serial.println("Cleared NVS");
    }

    /* store BLECache device in nvram */
    static byte freeze(uint16_t cacheindex) {
      preferences.begin("BLECollector", false);
      unsigned int freezecounter = preferences.getUInt("freezecounter", 0);
      // Increase counter by 1
      freezecounter++;
      if (freezecounter > MAX_ITEMS_IN_PREFS ) {
        freezecounter = 0;
      }
      // Store the new counter to the Preferences
      preferences.putUInt("freezecounter", freezecounter);
      preferences.end();
      const char* freezenameTpl = "cache-%d";
      char freezenameStr[12] = {'\0'};
      sprintf(freezenameStr, freezenameTpl, freezecounter);
      Serial.printf("****** Freezing cache index %d into NVS as index %s : %s\n", cacheindex, freezenameStr, BLEDevCache[cacheindex].address);
      //Serial.print("****** Freezing cache index "); Serial.print(cacheindex); Serial.print(" into NVS as index "); 
      //Serial.print(freezenameStr); Serial.print(" : "); Serial.println(BLEDevCache[cacheindex].address);
      preferences.begin(freezenameStr, false);
      preferences.putBool("in_db",        BLEDevCache[cacheindex].in_db);
      preferences.putUChar("hits",        BLEDevCache[cacheindex].hits);
      preferences.putInt("appearance",    BLEDevCache[cacheindex].appearance);
      preferences.putString("name",       BLEDevCache[cacheindex].name);
      preferences.putString("address",    BLEDevCache[cacheindex].address);
      preferences.putString("ouiname",    BLEDevCache[cacheindex].ouiname);
      preferences.putInt("rssi",          BLEDevCache[cacheindex].rssi);
      preferences.putInt("manufid",       BLEDevCache[cacheindex].manufid);
      preferences.putString("manufname",  BLEDevCache[cacheindex].manufname);
      preferences.putString("uuid",       BLEDevCache[cacheindex].uuid);
      preferences.end();
      return freezecounter;
    }

    /* extract one BLECache device from nvram into cache */
    static void thaw(byte freezeindex, uint16_t cacheindex) {
      const char* freezenameTpl = "cache-%d";
      char freezenameStr[12] = {'\0'};
      sprintf(freezenameStr, freezenameTpl, freezeindex);
      preferences.begin(freezenameStr, true);
      //BLEDevCache[cacheindex].reset(); // avoid mixing new and old data
      BLEDevHelper.reset( BLEDevCache[cacheindex] );
      BLEDevCache[cacheindex].in_db       = preferences.getBool("in_db", false);
      BLEDevCache[cacheindex].hits        = preferences.getUChar("hits", 0);
      BLEDevCache[cacheindex].appearance  = preferences.getInt("appearance", 0);
      preferences.getString("name", BLEDevCache[cacheindex].name, 32);
      preferences.getString("address", BLEDevCache[cacheindex].address, MAC_LEN+1);
      preferences.getString("ouiname", BLEDevCache[cacheindex].ouiname, 32);
      BLEDevCache[cacheindex].rssi        = preferences.getInt("rssi", 0);
      BLEDevCache[cacheindex].manufid     = preferences.getInt("manufid", -1);
      preferences.getString("manufname", BLEDevCache[cacheindex].manufname, 32);
      preferences.getString("uuid", BLEDevCache[cacheindex].uuid, 32);
      if( !isEmpty( BLEDevCache[cacheindex].address ) ) {
        Serial.printf("****** Thawing pref index %d into cache index %d : %s\n", freezeindex, cacheindex, BLEDevCache[cacheindex].address);
      }
      preferences.end();
    }

    /* extract all BLECache devices from nvram */
    static void thaw() {
      UI.BLECardTheme.textColor = WROVER_WHITE;
      UI.BLECardTheme.borderColor = WROVER_CYAN;
      UI.BLECardTheme.bgColor = BLECARD_BGCOLOR;
      for(byte i=0;i<MAX_ITEMS_IN_PREFS;i++) {
        uint16_t oldCacheIndex = BLEDevCacheIndex;
        BLEDevCacheIndex = getNextBLEDevCacheIndex(BLEDevCache, BLEDevCacheIndex);
        thaw(i, BLEDevCacheIndex);
        if( !isEmpty( BLEDevCache[BLEDevCacheIndex].address ) ) {
          populate(BLEDevCache, BLEDevCacheIndex);
          const char* headerTpl = "Defrosted %d#%d";
          char headerStr[32] = {'\0'};
          sprintf( headerStr, headerTpl, BLEDevCacheIndex, i);
          UI.headerStats( headerStr );
          UI.printBLECard( BLEDevCache, BLEDevCacheIndex );
          UI.cacheStats( /*BLEDevCacheUsed, VendorCacheUsed, OuiCacheUsed */);
          UI.footerStats();          
        }
      }
    }

    /* complete unpopulated fields of a given entry */
    static void populate( BlueToothDevice *_BLEDevCache, uint16_t _BLEDevCacheIndex ) {
      if( _BLEDevCacheIndex>=BLEDEVCACHE_SIZE ) {
        Serial.printf("[!!!populate!!!] Ignored Invalid Cache Index :%d\n", _BLEDevCacheIndex);        
        return;
      }
      //Serial.printf("[populating :%d]\n", _BLEDevCacheIndex);        
      /*
      _BLEDevCacheIndex = oldCacheIndex;
      int deviceIndexIfExists = DB.deviceExists( _BLEDevCache[currentCacheIndex].address ); // will load from DB if necessary
      if(deviceIndexIfExists>-1) {
        //_BLEDevCacheIndex = deviceIndexIfExists;
        return;
      } else {
        _BLEDevCacheIndex = currentCacheIndex;
      }*/
      
      if( strcmp( _BLEDevCache[_BLEDevCacheIndex].ouiname, "[unpopulated]" )==0 ){
        Serial.printf("  [populating OUI :%d]\n", _BLEDevCacheIndex);
        DB.getOUI( _BLEDevCache[_BLEDevCacheIndex].address, _BLEDevCache[_BLEDevCacheIndex].ouiname );
      }
      if( strcmp( _BLEDevCache[_BLEDevCacheIndex].manufname, "[unpopulated]" )==0 ) {
        if( _BLEDevCache[_BLEDevCacheIndex].manufid!=-1 ) {
          Serial.printf("  [populating Vendor :%d]\n", _BLEDevCacheIndex);
          DB.getVendor( _BLEDevCache[_BLEDevCacheIndex].manufid, _BLEDevCache[_BLEDevCacheIndex].manufname );
        } else {
          BLEDevHelper.set( _BLEDevCache[_BLEDevCacheIndex], "manufname", '\0');
        }
      }
      //Serial.printf("[populating anon state :%d]\n", _BLEDevCacheIndex);
      _BLEDevCache[_BLEDevCacheIndex].is_anonymous = isAnonymousDevice( _BLEDevCache, _BLEDevCacheIndex );
      //Serial.printf("[populated :%d]\n", _BLEDevCacheIndex);
    }

    /* inserts thawed entry into DB */
    bool feed() {
      bool fed = false;
      for( int i=0; i<BLEDEVCACHE_SIZE; i++ ) {
        if( isEmpty( BLEDevCache[i].address ) /*&& BLEDevCache[i].address[0] == '\0'*/) continue;
        if( BLEDevCache[i].in_db == true ) continue;
        if( isAnonymousDevice( BLEDevCache, i ) ) continue;
        DB.clean( BLEDevCache[i].name );
        DB.clean( BLEDevCache[i].ouiname );
        DB.clean( BLEDevCache[i].manufname );
        DB.clean( BLEDevCache[i].uuid );
        if( DB.insertBTDevice( BLEDevCache, i ) == DBUtils::INSERTION_SUCCESS ) {
          fed = true;
          BLEDevCache[i].in_db == true;
          Serial.println("####### Feeding thawed "); Serial.print( BLEDevCache[i].address ); Serial.println(" to DB");
        } else {
          Serial.print("####### Showing thawed "); Serial.println( BLEDevCache[i].address );
        }
        delay(1);
      }
      return fed;
    }

    /* process+persist device data in 6 steps to determine if worthy to display or not */
    /*
    static bool processDevice( BLEAdvertisedDevice &advertisedDevice, byte &_cacheIndex, int deviceNum ) {
      const char* currentBLEAddress = advertisedDevice.getAddress().toString().c_str();
      memcpy( DB.currentBLEAddress, currentBLEAddress, MAC_LEN+1 );
      // 1) return if BLECard is already on screen
      //Serial.print("processDevice:1:");dumpStats();
      if( UI.BLECardIsOnScreen( DB.currentBLEAddress ) ) {
        SelfCacheHit++;
        sprintf( processMessage, processTemplateShort, "Ignoring #", deviceNum );
        return false;
      }
      // 2) return if BLECard is already in cache
      //Serial.print("processDevice:2:");dumpStats();
      int deviceIndexIfExists = getDeviceCacheIndex( DB.currentBLEAddress );
      if( deviceIndexIfExists > -1 ) { // load from cache
        inCacheCount++;
        _cacheIndex = deviceIndexIfExists;
        UI.BLECardTheme.setTheme( (isAnonymousDevice( BLEDevCache, _cacheIndex ) ? IN_CACHE_ANON : IN_CACHE_NOT_ANON) );
        sprintf( processMessage, processTemplateLong, "Cache ", _cacheIndex, "#", deviceNum );
        return true;
      }
      notInCacheCount++;

      if(!DB.isOOM) {
        deviceIndexIfExists = DB.deviceExists( DB.currentBLEAddress ); // will load from DB if necessary
      }
      // 4) return if device is in DB
      //Serial.print("processDevice:4:");dumpStats();
      if( deviceIndexIfExists > -1 ) {
        _cacheIndex = deviceIndexIfExists;
        UI.BLECardTheme.setTheme( IN_CACHE_NOT_ANON );
        sprintf( processMessage, processTemplateLong, "DB Seen ", _cacheIndex, "#", deviceNum );
        return true;
      }
      // 5) return if data frozen
      //Serial.print("processDevice:5:");dumpStats();
      newDevicesCount++;
      if( DB.isOOM ) { // newfound but OOM, gather what's left of data without DB
        _cacheIndex = getNextBLEDevCacheIndex( BLEDevCache, BLEDevCacheIndex );
        store( advertisedDevice, BLEDevCache, _cacheIndex, false ); // store data in cache but don't populate
        // freeze it partially ...
        BLEDevCache[_cacheIndex].in_db = false;
        #ifdef USE_NVS
          byte prefIndex = freeze( _cacheIndex );
          sprintf( processMessage, processTemplateLong, "Frozen ", _cacheIndex, "#", deviceNum );
        #endif
        return false; // don't render it (will be thawed, populated, inserted and rendered on reboot)
      }
      // 6) return insertion/freeze state if nonanonymous
      //Serial.print("processDevice:6:");dumpStats();
      _cacheIndex = getNextBLEDevCacheIndex( BLEDevCache, BLEDevCacheIndex );
      store( advertisedDevice, BLEDevCache, _cacheIndex ); // store data in cache
      if( !isAnonymousDevice( BLEDevCache, _cacheIndex ) ) {
        if(DB.insertBTDevice( BLEDevCache, _cacheIndex ) == DBUtils::INSERTION_SUCCESS) {
          entries++;
          prune_trigger++;
          UI.BLECardTheme.setTheme( NOT_IN_CACHE_NOT_ANON );
          sprintf( processMessage, processTemplateLong, "Inserted ", _cacheIndex, "#", deviceNum );
          return true;
        }
        #ifdef USE_NVS // DB Insert Error, freeze it in NVS!
          byte prefIndex = freeze( _cacheIndex );
          sprintf( processMessage, processTemplateLong, "Frozen ", _cacheIndex, "#", deviceNum );
        #endif
        return false; // don't render it (will be thawed, rendered and inserted on reboot)
      }
      // 7) device is anonymous
      //Serial.print("processDevice:7:");dumpStats();
      AnonymousCacheHit++;
      UI.BLECardTheme.setTheme( IN_CACHE_ANON );
      sprintf( processMessage, processTemplateLong, "Anon ", _cacheIndex, "#", deviceNum );
      return true;
    }*/


    /*
    static bool onScanProcess( BLEScanResults foundDevices ) {
      if( onScanProcessed ) {
        Serial.printf("[%s] %s\n", __func__, " onScanProcessed = true ");
        return false;
      }
      if( scan_cursor >= devicesCount || scan_cursor >= MAX_DEVICES_PER_SCAN ) {
        BLEDevice::getScan()->clearResults(); // release some memory
        onScanProcessed = true;
        Serial.printf("[%s] %s\n", __func__, "done all");
        scan_cursor = 0;
        return false;
      }
      Serial.printf("[%s] %s $d\n", __func__, "stored", scan_cursor);
      advertisedDevice = foundDevices.getDevice( scan_cursor );
      BLEDevHelper.store( BLEDevTmpCache[scan_cursor], advertisedDevice );
      //store( advertisedDevice, BLEDevTmpCache, scan_cursor, false ); // store data in cache but don't populate
      sprintf( processMessage, processTemplateLong, "Stored ", scan_cursor, " / ", devicesCount );
      UI.headerStats( processMessage );
      scan_cursor++;
      return true;
    }*/




    static bool onScanPopulate( uint16_t _scan_cursor ) {
      if( onScanPopulated ) {
        //Serial.printf("[%s] %s\n", __func__, " onScanPopulated = true ");
        return false;
      }
      if( scan_cursor >= devicesCount) {
        onScanPopulated = true;
        //scan_cursor = 0;
        Serial.printf("[%s] %s\n", __func__, "done all");
        return false;
      }
      uint16_t BLEDevTmpCacheIndex = _scan_cursor;
      if( isEmpty( BLEDevTmpCache[_scan_cursor].address ) ) {
        //scan_cursor++;
        Serial.printf("  [%s] %s\n", __func__, "empty addess");
        return true; // end of cache
      }
      if( strcmp( BLEDevTmpCache[_scan_cursor].ouiname, "[unpopulated]")!=0 ) {
        //scan_cursor++;
        Serial.printf("  [%s] %s\n", __func__, "already oui-populated");
        return true; // already populated
      }
      if( strcmp( BLEDevTmpCache[_scan_cursor].manufname, "[unpopulated]")!=0 ) {
        //scan_cursor++;
        Serial.printf(  "[%s] %s\n", __func__, "already manufname-populated");
        return true; // already populated
      }
      populate( BLEDevTmpCache, BLEDevTmpCacheIndex );
      //scan_cursor++;
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
        //scan_cursor = 0;
        return false;
      }
      if( BLEDevTmpCache[scan_cursor].hits > 0 ) {
        //scan_cursor++;
        Serial.printf("  [%s] %s\n", __func__, "skipping, has hits");
        return true;
      }
      int deviceIndexIfExists = -1;
      deviceIndexIfExists = getDeviceCacheIndex( BLEDevTmpCache[_scan_cursor].address );
      if( deviceIndexIfExists > -1 ) {
        inCacheCount++;
        BLEDevCache[deviceIndexIfExists].hits++;
        BLEDevTmpCache[_scan_cursor].hits = BLEDevCache[deviceIndexIfExists].hits;
        BLEDevTmpCache[_scan_cursor].in_db = BLEDevCache[deviceIndexIfExists].in_db;
        Serial.printf( "  [%s] Device %d exists in cache, copied %d hits\n", __func__, _scan_cursor, BLEDevTmpCache[_scan_cursor].hits );
      } else {
        if( BLEDevTmpCache[_scan_cursor].is_anonymous ) {
          // won't land in DB but can land in cache
          uint16_t nextCacheIndex = getNextBLEDevCacheIndex( BLEDevCache, BLEDevCacheIndex );
          BLEDevTmpCache[_scan_cursor].hits = 1;
          BLEDevCache[nextCacheIndex] = BLEDevTmpCache[_scan_cursor]; // TODO: copy properly
          Serial.printf( "  [%s] Device %d is anonymous, won't be inserted, set %d hits to 1 and copied\n", __func__, _scan_cursor, BLEDevTmpCache[scan_cursor].hits );
        } else {
          deviceIndexIfExists = DB.deviceExists( BLEDevTmpCache[_scan_cursor].address ); // will load returning devices from DB if necessary
          if(deviceIndexIfExists>-1) {
            BLEDevTmpCache[_scan_cursor].in_db = true;
            BLEDevCache[deviceIndexIfExists].hits++; // increase hits
            BLEDevTmpCache[_scan_cursor].hits = BLEDevCache[deviceIndexIfExists].hits;
            Serial.printf( "  [%s] Device %d is already in DB, increased hits to %d\n", __func__, _scan_cursor, BLEDevTmpCache[scan_cursor].hits );
          } else {
            Serial.printf( "  [%s] Device %d is not in DB\n", __func__, _scan_cursor );
          }
        }
      }
      //scan_cursor++;
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
        //scan_cursor = 0;
        return false;
      }
      UI.BLECardTheme.setTheme( IN_CACHE_ANON );
      //delay(10);
      //esp_task_wdt_reset();
      UI.printBLECard( BLEDevTmpCache, _scan_cursor ); // render
      delay(1);
      sprintf( processMessage, processTemplateLong, "Rendered ", _scan_cursor+1, " / ", devicesCount );
      UI.headerStats( processMessage );
      delay(1);
      UI.cacheStats( /*BLEDevCacheUsed, VendorCacheUsed, OuiCacheUsed */);
      delay(1);
      UI.footerStats();
      delay(1);
      //scan_cursor++;
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
      if( isEmpty( BLEDevTmpCache[_scan_cursor].address ) ) {
        //scan_cursor++;
        return true;
      }
      if( BLEDevTmpCache[_scan_cursor].is_anonymous || BLEDevTmpCache[_scan_cursor].in_db ) { // don't DB-insert anon or duplicates
        sprintf( processMessage, processTemplateLong, "Released ", _scan_cursor+1, " / ", devicesCount );
        if( BLEDevTmpCache[_scan_cursor].is_anonymous ) AnonymousCacheHit++;
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
      //scan_cursor++;
      return true;
    }

/*
    static bool onScanPopulate() {
      if( onScanPopulated ) return false;

      for( uint16_t i=0; i<devicesCount; i++ ) {
         BLEDevTmpCacheIndex = i;
        if( isEmpty( BLEDevTmpCache[i].address ) ) continue; // end of cache
        if( strcmp( BLEDevTmpCache[i].ouiname, "[unpopulated]")!=0 ) continue; // already populated
        if( strcmp( BLEDevTmpCache[i].manufname, "[unpopulated]")!=0 ) continue; // already populated
        populate( BLEDevTmpCache, BLEDevTmpCacheIndex );
        int deviceIndexIfExists = -1;
        deviceIndexIfExists = getDeviceCacheIndex( BLEDevTmpCache[i].address );
        if( deviceIndexIfExists > -1 ) {
          inCacheCount++;
          BLEDevCache[deviceIndexIfExists].hits++;
          BLEDevTmpCache[i].hits = BLEDevCache[deviceIndexIfExists].hits;
          BLEDevTmpCache[i].in_db = BLEDevCache[deviceIndexIfExists].in_db;
          Serial.printf( "Device %d exists in cache, copied %d hits\n", i, BLEDevTmpCache[i].hits );
        } else {
          if( BLEDevTmpCache[i].is_anonymous ) {
            // won't land in DB but can land in cache
            uint16_t nextCacheIndex = getNextBLEDevCacheIndex( BLEDevCache, BLEDevCacheIndex );
            BLEDevTmpCache[i].hits = 1;
            BLEDevCache[nextCacheIndex] = BLEDevTmpCache[i]; // TODO: copy properly
            Serial.printf( "Device %d is anonymous, won't be inserted, set %d hits to 1 and copied\n", i, BLEDevTmpCache[i].hits );
          } else {
            deviceIndexIfExists = DB.deviceExists( BLEDevTmpCache[i].address ); // will load returning devices from DB if necessary
            if(deviceIndexIfExists>-1) {
              BLEDevTmpCache[i].in_db = true;
              BLEDevCache[deviceIndexIfExists].hits++; // increase hits
              BLEDevTmpCache[i].hits = BLEDevCache[deviceIndexIfExists].hits;
              Serial.printf( "Device %d is already in DB, increed hits to %d\n", i, BLEDevTmpCache[i].hits );
            } else {
              Serial.printf( "Device %d is not in DB\n", i );
            }
          }
        }
        // TODO : post process
        UI.BLECardTheme.setTheme( IN_CACHE_ANON );
        delay(10);
        esp_task_wdt_reset();
        UI.printBLECard( BLEDevTmpCache, i ); // render
        sprintf( processMessage, processTemplateLong, "Rendered ", i+1, " / ", devicesCount );
        UI.headerStats( processMessage );
        UI.footerStats();
        return true;
      }
      onScanPopulated = true;
      return false;
    }*/


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
      Serial.println("Clearing scan results");
      BLEDevice::getScan()->clearResults();

      if( devicesCount < MAX_DEVICES_PER_SCAN ) {
        if( SCAN_DURATION+1 < MAX_SCAN_DURATION ) {
          SCAN_DURATION++;
        }
      } else if( devicesCount > MAX_DEVICES_PER_SCAN ) {
        if( SCAN_DURATION-1 >= MIN_SCAN_DURATION ) {
          SCAN_DURATION--;
        }
        Serial.printf("Cache overflow (%d results vs %d slots), truncating results...\n", devicesCount, MAX_DEVICES_PER_SCAN);
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

    /* process scan data */
    /*
    static void onScanDone( BLEScanResults foundDevices  ) {
      UI.stopBlink();
      //UI.footerStats();
      UI.headerStats("Showing results ...");

      if( processedDevicesCount == 0 ) { // sync scan
        devicesCount = foundDevices.getCount();
      } else { // async scan
        devicesCount = processedDevicesCount;
        Serial.println("Clearing scan results");
        BLEDevice::getScan()->clearResults();
      }

      if( devicesCount < MAX_DEVICES_PER_SCAN ) {
        if( SCAN_DURATION+1 < MAX_SCAN_DURATION ) {
          SCAN_DURATION++;
        }
      } else if( devicesCount >= MAX_DEVICES_PER_SCAN ) {
        if( SCAN_DURATION-1 >= MIN_SCAN_DURATION ) {
          SCAN_DURATION--;
        }
        Serial.printf("Cache overflow (%d results vs %d slots), truncating results...\n", devicesCount, MAX_DEVICES_PER_SCAN);
        devicesCount = MAX_DEVICES_PER_SCAN;
      }


      sessDevicesCount += devicesCount;
      notInCacheCount = 0;
      inCacheCount = 0;
      onScanProcessed = true;
      
    }*/


    #if SCAN_MODE==SCAN_TASK_0 || SCAN_MODE==SCAN_TASK_1 || SCAN_MODE==SCAN_TASK
      static void scanTask( void * parameter ) {
        BLEDevice::init("");
        pBLEScan = BLEDevice::getScan(); //create new scan
        auto pDeviceCallback = new FoundDeviceCallback(); // collect/store BLE data
        pBLEScan->setAdvertisedDeviceCallbacks( pDeviceCallback ); // memory leak ?
        pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
        pBLEScan->setInterval(0x50); // 0x50
        pBLEScan->setWindow(0x30); // 0x30
        while( 1 ) {
          // flattened logic tree saves memory
          //vTaskDelay(10);
          //esp_task_wdt_reset();
          if( onScanPopulate( scan_cursor ) )  { /*scan_cursor++; continue;*/ } // OUI / vendorname / isanonymous
          //vTaskDelay(10);
          //esp_task_wdt_reset();
          if( onScanIfExists( scan_cursor ) )  { /*scan_cursor++; continue;*/ } // exists + hits
          //vTaskDelay(10);
          //esp_task_wdt_reset();
          if( onScanRender( scan_cursor ) )    { /*scan_cursor++; continue;*/ } // ui work
          //vTaskDelay(10);
          //esp_task_wdt_reset();
          if( onScanPropagate( scan_cursor ) ) { scan_cursor++; continue; } // copy to DB / cache

          Serial.print("BeforeScan::");dumpStats();

          onBeforeScan();
          pBLEScan->start(SCAN_DURATION);
          onAfterScan();

          UI.update(); // run after-scan display stuff
          DB.maintain();
          Serial.print("AfterScan:::");dumpStats();
          scan_rounds++;
        }
        vTaskDelete( NULL );
      }
    #elif SCAN_MODE==SCAN_LOOP
      void scanLoop() {
        //if( onScanProcess( bleresults ) ) continue; // fast-store without populating
        if( onScanPopulate() ) continue;
        if( onScanPropagate() ) continue;
        UI.headerStats("  Scan in progress");
        UI.startBlink();
        Serial.print("BeforeScan::");dumpStats();
        processedDevicesCount = 0;
        devicesCount = 0;
        //pBLEScan->start(SCAN_DURATION, onScanDone);
        pBLEScan->start(SCAN_DURATION);
        bleresults = pBLEScan->getResults();
        onScanDone( bleresults );
        UI.update(); // run after-scan display stuff
        DB.maintain();
        Serial.print("AfterScan:::");dumpStats();
        scan_rounds++;
      }
    #endif

};


#endif

BLEScanUtils BLECollector;
