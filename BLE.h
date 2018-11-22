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
      SDSetup();
      UI.init();
    };
    void scan() { };
};
#else


static byte processedDevicesCount = 0;

class FoundDeviceCallback: public BLEAdvertisedDeviceCallbacks {
  bool toggler = true;
  //byte processedDevicesCount = 0;
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    //Serial.printf("Found %s \n", advertisedDevice.toString().c_str()); // <<< memory leak
    processedDevicesCount++;
    if( processedDevicesCount+1 > BLEDEVCACHE_SIZE ) {
      // too many devices found can't fit in cache, stop scan
      if( SCAN_DURATION-1 >= MIN_SCAN_DURATION ) {
        SCAN_DURATION--;
        //#Serial.println("[SCAN_DURATION] decreased to " + String(SCAN_DURATION));
      }
      //processedDevicesCount = 0;
      advertisedDevice.getScan()->stop();
      UI.stopTaskBlink();
    }
    //UI.headerStats("Found " + String(processedDevicesCount));
    toggler = !toggler;
    if(toggler) {
      UI.bleStateIcon(WROVER_GREEN);
    } else {
      UI.bleStateIcon(WROVER_DARKGREEN);
    }
  }
};

const char* processTemplateLong = "%s%d%s%d";
const char* processTemplateShort = "%s%d";
static char processMessage[20];
static int scan_rounds = 0;

unsigned long lastheap = 0;
byte lastscanduration = SCAN_DURATION;
char heapsign[5]; // unicode sign terminated
char scantimesign[5]; // unicode sign terminated

class BLEScanUtils {

  public:

    void init() {
      SDSetup();
      UI.init();
      DB.init();

      #ifdef USE_NVS
      if ( resetReason == 12)  { // =  SW_CPU_RESET
        thaw(); // get leftovers from NVS
        if( feed() ) { // got insertions in DB ?
          clearNVS(); // purge this
          //ESP.restart();
        }
      } else {
        clearNVS();
        ESP.restart();
      }
      #endif
      
      WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
      //BLEDevice::init("");
      xTaskCreatePinnedToCore(scanTask, "scanTask", 5000, NULL, 0, NULL, 0); /* last = Task Core */
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
     
      Serial.printf("[Scan#%02d][%s][Duration%s%d][Processed:%d of %d][Heap%s%d] [Cache hits][Screen:%d][BLEDevCards:%d][Anonymous:%d][Oui:%d][Vendor:%d]\n", 
        scan_rounds,
        hhmmssString,
        scantimesign,
        lastscanduration,
        processedDevicesCount,
        devicesCount,
        heapsign,
        lastheap,
        SelfCacheHit,
        BLEDevCacheHit,
        AnonymousCacheHit,
        OuiCacheHit,
        VendorCacheHit
      );
    }

    static int getDeviceCacheIndex(const char* address) {
      for(int i=0;i<BLEDEVCACHE_SIZE;i++) {
        if( strcmp(address, BLEDevCache[i].address.c_str())==0  ) {
          BLEDevCache[i].hits++;
          BLEDevCacheHit++;
          //Serial.printf("[CACHE HIT] BLEDevCache ID #%s has %d cache hits\n", address, BLEDevCache[i].hits);
          return i;
        }
      }
      return -1;
    }


    static void connectToService(byte cacheindex) {
      // driver is still bugged, connect() waits forever
      //Serial.println("Stopping scan");
      //advertisedDevice.getScan()->stop();
      Serial.println("Creating client");
      auto* pClient = BLEDevice::createClient();
      if(pClient) {
        Serial.println("Client created");
        //auto* currentBLEAddress = new BLEAddress(advertisedDevice.getAddress());
        auto* pAddress = new BLEAddress( BLEDevCache[cacheindex].address.c_str() );
        
        if(pClient->connect(*pAddress)) {
          Serial.println("Client connected");
          //Serial.printf("  Client[%d]: %s", pClient->getAppID(), pClient->getPeerAddress().toString().c_str());
          Serial.println();
          
          auto* pRemoteServiceMap = pClient->getServices();
          for (auto itr : *pRemoteServiceMap)  {
            Serial.print("    ");
            Serial.println(itr.second->toString().c_str());
            
            /*auto* pCharacteristicMap = itr.second->getCharacteristicsByHandle();
            for (auto itr : *pCharacteristicMap)  {
              Serial.print("      ");
              Serial.print(itr.second->toString().c_str());
              
              if(itr.second->canNotify()) {
                //itr.second->registerForNotify(notifyCallback);
                Serial.print(" notify registered");
              }
              Serial.println();
            }*/
          }
        }
        delete pAddress;
      }
    }

    /* determines whether a device is worth saving or not */
    static bool isAnonymousDevice(byte cacheindex) {
      if(BLEDevCache[cacheindex].uuid!="") return false; // uuid's are interesting, let's collect
      if(BLEDevCache[cacheindex].name!="") return false; // has name, let's collect
      if(BLEDevCache[cacheindex].appearance!="") return false; // has icon, let's collect
      if(BLEDevCache[cacheindex].ouiname=="[unpopulated]") return false; // don't know yet, let's keep
      if(BLEDevCache[cacheindex].manufname=="[unpopulated]") return false; // don't know yet, let's keep
      if(BLEDevCache[cacheindex].ouiname=="[private]" || BLEDevCache[cacheindex].ouiname=="") return true; // don't care
      if(BLEDevCache[cacheindex].manufname=="[unknown]" || BLEDevCache[cacheindex].manufname=="") return true; // don't care
      if(BLEDevCache[cacheindex].manufname!="" && BLEDevCache[cacheindex].ouiname!="") return false; // anonymous but qualified device, let's collect
      return true;
    }

    /* stores BLEDevice info in memory cache after retrieving complementary data */
    static byte store(BLEAdvertisedDevice &advertisedDevice, bool populate=true) {
      BLEDevCacheIndex = getNextBLEDevCacheIndex();
      BLEDevCacheReset(BLEDevCacheIndex);// avoid mixing new and old data
      BLEDevCache[BLEDevCacheIndex].in_db = false;
      BLEDevCache[BLEDevCacheIndex].address = advertisedDevice.getAddress().toString().c_str();
      //BLEDevCache[BLEDevCacheIndex].spower = String( (int)advertisedDevice.getTXPower() );
      if(populate) {
        BLEDevCache[BLEDevCacheIndex].ouiname = DB.getOUI( BLEDevCache[BLEDevCacheIndex].address ); // TODO : procrastinate this
      } else {
        BLEDevCache[BLEDevCacheIndex].ouiname = "[unpopulated]";
      }
      BLEDevCache[BLEDevCacheIndex].rssi = String ( advertisedDevice.getRSSI() );
      if (advertisedDevice.haveName()) {
        BLEDevCache[BLEDevCacheIndex].name = String ( advertisedDevice.getName().c_str() );
      } else {
        BLEDevCache[BLEDevCacheIndex].name = "";
      }
      if (advertisedDevice.haveAppearance()) {
        BLEDevCache[BLEDevCacheIndex].appearance = advertisedDevice.getAppearance();
      } else {
        BLEDevCache[BLEDevCacheIndex].appearance = "";
      }
      if (advertisedDevice.haveManufacturerData()) {
        uint8_t* mdp = (uint8_t*)advertisedDevice.getManufacturerData().data();
        //std::string md = advertisedDevice.getManufacturerData();
        //char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
        uint8_t vlsb = mdp[0];
        uint8_t vmsb = mdp[1];
        uint16_t vint = vmsb * 256 + vlsb;
        if(populate) {
          //BLEDevCache[BLEDevCacheIndex].manufname = DB.getVendor( vint ); // TODO : procrastinate this
          DB.getVendor( vint, BLEDevCache[BLEDevCacheIndex].manufname ); // TODO : procrastinate this
        } else {
          memcpy( BLEDevCache[BLEDevCacheIndex].manufname, "[unpopulated]", 14 );
        }
        //BLEDevCache[BLEDevCacheIndex].manufid = String ( pHex );
        BLEDevCache[BLEDevCacheIndex].manufid = vint;
      } else {
        *BLEDevCache[BLEDevCacheIndex].manufname = '\0';
        BLEDevCache[BLEDevCacheIndex].manufid = -1;
      }
      if (advertisedDevice.haveServiceUUID()) {
        BLEDevCache[BLEDevCacheIndex].uuid = String( advertisedDevice.getServiceUUID().toString().c_str() );

        //connectToService( cacheIndex );
        
      } else {
        BLEDevCache[BLEDevCacheIndex].uuid = "";
      }
      return BLEDevCacheIndex;          
    }

    /* cleanup NVS cache data */
    void clearNVS() {
      for(byte i=0;i<MAX_ITEMS_IN_PREFS;i++) {
        String freezename = "cache-"+String(i);
        //Serial.println("Clearing " + freezename);
        preferences.begin(freezename.c_str(), false);
        preferences.clear();
        preferences.end();
      }
      Serial.println("Cleared NVS");
    }

    /* store BLECache device in nvram */
    static byte freeze(byte cacheindex) {
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
      String freezename = "cache-"+String(freezecounter);
      Serial.println("****** Freezing cache index "+String(cacheindex)+" into NVS as index "+freezename+" : " + BLEDevCache[cacheindex].address);
      preferences.begin(freezename.c_str(), false);
      preferences.putBool("in_db",        BLEDevCache[cacheindex].in_db);
      preferences.putUChar("hits",        BLEDevCache[cacheindex].hits);
      preferences.putString("appearance", BLEDevCache[cacheindex].appearance);
      preferences.putString("name",       BLEDevCache[cacheindex].name);
      preferences.putString("address",    BLEDevCache[cacheindex].address);
      preferences.putString("ouiname",    BLEDevCache[cacheindex].ouiname);
      preferences.putString("rssi",       BLEDevCache[cacheindex].rssi);
      preferences.putInt("manufid",       BLEDevCache[cacheindex].manufid);
      preferences.putString("manufname",  BLEDevCache[cacheindex].manufname);
      preferences.putString("uuid",       BLEDevCache[cacheindex].uuid);
      preferences.end();
      return freezecounter;
    }

    /* extract one BLECache device from nvram into cache */
    static void thaw(byte freezeindex, byte cacheindex) {
      String freezename = "cache-"+String(freezeindex);
      preferences.begin(freezename.c_str(), true);
      //BLEDevCache[cacheindex].reset(); // avoid mixing new and old data
      BLEDevCacheReset(cacheindex);
      BLEDevCache[cacheindex].in_db       = preferences.getBool("in_db", false);
      BLEDevCache[cacheindex].hits        = preferences.getUChar("hits", 0);
      BLEDevCache[cacheindex].appearance  = preferences.getString("appearance", "");
      BLEDevCache[cacheindex].name        = preferences.getString("name", "");
      BLEDevCache[cacheindex].address     = preferences.getString("address", "");
      BLEDevCache[cacheindex].ouiname     = preferences.getString("ouiname", "");
      BLEDevCache[cacheindex].rssi        = preferences.getString("rssi", "");
      BLEDevCache[cacheindex].manufid     = preferences.getInt("manufid", -1);
      preferences.getString("manufname", BLEDevCache[cacheindex].manufname, 32);
      BLEDevCache[cacheindex].uuid        = preferences.getString("uuid", "");
      if(BLEDevCache[cacheindex].address != "") {
        Serial.printf("****** Thawing pref index %d into cache index %d : %s\n", freezeindex, cacheindex, BLEDevCache[cacheindex].address.c_str());
      }
      preferences.end();
    }

    /* extract all BLECache devices from nvram */
    static void thaw() {
      UI.BLECardTheme.textColor = WROVER_WHITE;
      UI.BLECardTheme.borderColor = WROVER_CYAN;
      UI.BLECardTheme.bgColor = BLECARD_BGCOLOR;
      for(byte i=0;i<MAX_ITEMS_IN_PREFS;i++) {
        byte oldCacheIndex = BLEDevCacheIndex;
        BLEDevCacheIndex = getNextBLEDevCacheIndex();
        thaw(i, BLEDevCacheIndex);
        if(BLEDevCache[BLEDevCacheIndex].address!="") {
          populate(BLEDevCacheIndex, oldCacheIndex);
          UI.headerStats( "Defrosted "+String(BLEDevCacheIndex)+"#" + String(i) );
          UI.printBLECard( BLEDevCacheIndex );
          UI.footerStats();          
        }
      }
    }
    
    /* complete unpopulated fields of a given entry */
    static void populate(byte cacheIndex, byte oldCacheIndex) {
      BLEDevCacheIndex = oldCacheIndex;
      int deviceIndexIfExists = DB.deviceExists( BLEDevCache[cacheIndex].address.c_str() ); // will load from DB if necessary
      if(deviceIndexIfExists>-1) {
        return;
      } else {
        BLEDevCacheIndex = cacheIndex;
      }
      
      if(BLEDevCache[cacheIndex].ouiname == "[unpopulated]"){
        Serial.println("ouiname-populating " + BLEDevCache[cacheIndex].address);
        BLEDevCache[cacheIndex].ouiname = DB.getOUI( BLEDevCache[cacheIndex].address );
      }
      if(BLEDevCache[cacheIndex].manufname == "[unpopulated]") {
        if(BLEDevCache[cacheIndex].manufid!=-1 /*&& BLEDevCache[cacheIndex].manufid.length()>=4*/) {
          //BLEDevCache[cacheIndex].manufname = DB.getVendor( BLEDevCache[cacheIndex].manufid );
          DB.getVendor( BLEDevCache[cacheIndex].manufid, BLEDevCache[cacheIndex].manufname );
          //Serial.println("manufname-populating " + BLEDevCache[BLEDevCacheIndex].address);
          //Serial.println("  manufname: " + BLEDevCache[BLEDevCacheIndex].manufname);
        } else {
          //Serial.println("manufname-clearing " + BLEDevCache[BLEDevCacheIndex].address);
          *BLEDevCache[cacheIndex].manufname = '\0';
        }
      }
      
    }

    /* inserts thawed entry into DB */
    bool feed() {
      bool fed = false;
      for(int i=0;i<BLEDEVCACHE_SIZE;i++) {
        if(BLEDevCache[i].address == "") continue;
        if(BLEDevCache[i].in_db == true) continue;
        if(isAnonymousDevice( i )) continue;
        if(DB.insertBTDevice( i ) == DBUtils::INSERTION_SUCCESS) {
          fed = true;
          BLEDevCache[i].in_db == true;
          Serial.println("####### Feeding thawed " + BLEDevCache[i].address + " to DB");
        } else {
          Serial.println("####### Showing thawed " + BLEDevCache[i].address);
        }
      }
      return fed;
    }

    /* process+persist device data in 6 steps to determine if worthy to display or not */
    static bool processDevice( BLEAdvertisedDevice &advertisedDevice, byte &cacheIndex, int deviceNum ) {
      const char* currentBLEAddress = advertisedDevice.getAddress().toString().c_str();
      memcpy(DB.currentBLEAddress, currentBLEAddress, 18);
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
      if(deviceIndexIfExists>-1) { // load from cache
        inCacheCount++;
        cacheIndex = deviceIndexIfExists;
        UI.BLECardTheme.setTheme( (isAnonymousDevice( cacheIndex ) ? IN_CACHE_ANON : IN_CACHE_NOT_ANON) );
        sprintf( processMessage, processTemplateLong, "Cache ", cacheIndex, "#", deviceNum );
        return true;
      }
      // 3) return if BLEDevCache will explode
      //Serial.print("processDevice:3:");dumpStats();
      notInCacheCount++;
      if( /*notInCacheCount+inCacheCount*/ processedDevicesCount+1 > BLEDEVCACHE_SIZE) {
        Serial.println("[CRITICAL] device scan count exceeds circular cache size, cowardly giving up on this one: " + String(DB.currentBLEAddress));
        sprintf( processMessage, processTemplateShort, "Avoiding ", deviceNum );
        return false;
      }
      if(!DB.isOOM) {
        deviceIndexIfExists = DB.deviceExists( DB.currentBLEAddress ); // will load from DB if necessary
      }
      // 4) return if device is in DB
      //Serial.print("processDevice:4:");dumpStats();
      if(deviceIndexIfExists>-1) {
        cacheIndex = deviceIndexIfExists;
        UI.BLECardTheme.setTheme( IN_CACHE_NOT_ANON );
        sprintf( processMessage, processTemplateLong, "DB Seen ", cacheIndex, "#", deviceNum );
        return true;
      }
      // 5) return if data frozen
      //Serial.print("processDevice:5:");dumpStats();
      newDevicesCount++;
      if(DB.isOOM) { // newfound but OOM, gather what's left of data without DB
        cacheIndex = store( advertisedDevice, false ); // store data in cache but don't populate
        // freeze it partially ...
        BLEDevCache[cacheIndex].in_db = false;
        #ifdef USE_NVS
          byte prefIndex = freeze( cacheIndex );
          sprintf( processMessage, processTemplateLong, "Frozen ", cacheIndex, "#", deviceNum );
        #endif
        return false; // don't render it (will be thawed, populated, inserted and rendered on reboot)
      }
      // 6) return insertion/freeze state if nonanonymous
      //Serial.print("processDevice:6:");dumpStats();
      cacheIndex = store( advertisedDevice ); // store data in cache
      if(!isAnonymousDevice( cacheIndex )) {
        if(DB.insertBTDevice( cacheIndex ) == DBUtils::INSERTION_SUCCESS) {
          entries++;
          prune_trigger++;
          UI.BLECardTheme.setTheme( NOT_IN_CACHE_NOT_ANON );
          sprintf( processMessage, processTemplateLong, "Inserted ", cacheIndex, "#", deviceNum );
          return true;
        }
        #ifdef USE_NVS // DB Insert Error, freeze it in NVS!
          byte prefIndex = freeze( cacheIndex );
          sprintf( processMessage, processTemplateLong, "Frozen ", cacheIndex, "#", deviceNum );
        #endif
        return false; // don't render it (will be thawed, rendered and inserted on reboot)
      }
      // 7) device is anonymous
      //Serial.print("processDevice:7:");dumpStats();
      AnonymousCacheHit++;
      UI.BLECardTheme.setTheme( IN_CACHE_ANON );
      sprintf( processMessage, processTemplateLong, "Anon ", cacheIndex, "#", deviceNum );
      return true;
    }

    /* process scan data */
    static void onScanDone(BLEScanResults foundDevices) {
      UI.stopTaskBlink();
      UI.footerStats();
      UI.headerStats("Showing results ...");
      //String headerMessage = "";
      //headerMessage.reserve(20);
      byte cacheIndex;
      bool is_printable;
      notInCacheCount = 0;
      inCacheCount = 0;
      devicesCount = foundDevices.getCount();
      if(devicesCount < BLEDEVCACHE_SIZE) {
        if( SCAN_DURATION+1 < MAX_SCAN_DURATION ) {
          SCAN_DURATION++;
          //Serial.println("[SCAN_DURATION] increased to " + String(SCAN_DURATION));
        }
      }
      sessDevicesCount += devicesCount;
      for (int i = 0; i < devicesCount; i++) {
        BLEAdvertisedDevice advertisedDevice = foundDevices.getDevice(i);
        is_printable = processDevice( advertisedDevice, cacheIndex, i );
        UI.headerStats( processMessage );
        if( is_printable ) {
          UI.printBLECard( cacheIndex ); // TODO : procrastinate this
        }
        UI.footerStats();
      }
      UI.update(); // run after-scan display stuff
      
      if( DB.isOOM ) {
        Serial.println("[DB ERROR] restarting");
        Serial.println("During this session ("+ String(UpTimeString) +"), " 
         + String(newDevicesCount-AnonymousCacheHit) 
         + " out of " + String(sessDevicesCount) + " devices were added to the DB\n");
        delay(1000);
        ESP.restart();
      }
    }


    static void scanTask( void * parameter ) {
      BLEDevice::init("");
      auto pBLEScan = BLEDevice::getScan(); //create new scan
      auto pDeviceCallback = new FoundDeviceCallback();
      pBLEScan->setAdvertisedDeviceCallbacks( pDeviceCallback ); // memory leak ?
      pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
      pBLEScan->setInterval(0x50); // 0x50
      pBLEScan->setWindow(0x30); // 0x30

      while(1) {
        //updateTimeString();
        UI.headerStats("Scan in progress...");
        UI.startTaskBlink();
        processedDevicesCount = 0;
        devicesCount = 0;
        Serial.print("BeforeScan::");dumpStats();
        pBLEScan->start(SCAN_DURATION, onScanDone);
        Serial.print("AfterScan:::");dumpStats();
        scan_rounds++;
        //UI.stopTaskBlink();
        delay(1000);
      }
      
      vTaskDelete( NULL );
    }


    void scan() {

      // synchronous scan: blink icon and draw time-based scan progress in a separate task
      // while using the callback to update its status in real time
      delay(SCAN_DURATION*1000);

/*
      auto pBLEScan = BLEDevice::getScan(); //create new scan
      //pBLEScan->setAdvertisedDeviceCallbacks(new FoundDeviceCallback()); // memory leak !
      pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
      pBLEScan->setInterval(0x50); // 0x50
      pBLEScan->setWindow(0x30); // 0x30
      BLEScanResults foundDevices = pBLEScan->start(SCAN_DURATION);
*/

      

      /*
      if(foundDevices.getCount()<BLEDEVCACHE_SIZE) {
        if( SCAN_DURATION+1 < MAX_SCAN_DURATION ) {
          SCAN_DURATION++;
          Serial.println("[SCAN_DURATION] increased to " + String(SCAN_DURATION));
        }
      }*/
      //onScanDone( foundDevices );

      //pBLEScan->start(SCAN_DURATION, onScanDone);
      //UI.update(); // run after-scan display stuff
      //DB.maintain(); // check for db pruning
    }

};


#endif

BLEScanUtils BLECollector;
