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
    };
    void scan() { };
};
#else


class FoundDeviceCallback: public BLEAdvertisedDeviceCallbacks {
  bool toggler = true;
  byte foundDevicesCount = 0;
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.printf("Found %s \n", advertisedDevice.toString().c_str());
    foundDevicesCount++;
    //UI.headerStats("Found " + String(foundDevicesCount));
    toggler = !toggler;
    if(toggler) {
      UI.bleStateIcon(WROVER_GREEN);
    } else {
      UI.bleStateIcon(WROVER_DARKGREEN);
    }
  }
};

struct DeviceCacheStatus {
  bool exists = false;
  int index = -1;
};

class BLEScanUtils {

  public:

    void init() {
      UI.init();
      DB.init();
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
      WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
      BLEDevice::init("");
    }


    static DeviceCacheStatus deviceCacheStatus(String bleDeviceAddress, bool skipDB=false) {
      DeviceCacheStatus internalStatus;
      for(int i=0;i<BLEDEVCACHE_SIZE;i++) {
        if( BLEDevCache[i].address == bleDeviceAddress) {
          BLEDevCacheHit++;
          internalStatus.exists = true;
          internalStatus.index = i;
          return internalStatus;
        }
      }
      if(skipDB) return internalStatus;
      internalStatus.index = DB.deviceExists(bleDeviceAddress);
      if(internalStatus.index >=0) {
        internalStatus.exists = true;
      }
      return internalStatus;
    }


    static int getDeviceCacheIndex( String address ) {
      DeviceCacheStatus BLEDevStatus = deviceCacheStatus( address, true );
      return BLEDevStatus.index;
    }


    static bool isAnonymousDevice(byte cacheindex) {
      if(BLEDevCache[cacheindex].uuid!="") return false; // uuid's are interesting, let's collect
      if(BLEDevCache[cacheindex].name!="") return false; // has name, let's collect
      if(BLEDevCache[cacheindex].appearance!="") return false; // has icon, let's collect
      if(BLEDevCache[cacheindex].ouiname=="[unpopulated]") return false; // don't know yet, let's keep
      if(BLEDevCache[cacheindex].vname=="[unpopulated]") return false; // don't know yet, let's keep
      if(BLEDevCache[cacheindex].ouiname=="[private]" || BLEDevCache[cacheindex].ouiname=="") return true; // don't care
      if(BLEDevCache[cacheindex].vname=="[unknown]" || BLEDevCache[cacheindex].vname=="") return true; // don't care
      if(BLEDevCache[cacheindex].vname!="" && BLEDevCache[cacheindex].ouiname!="") return false; // anonymous but qualified device, let's collect
      return true;
    }

    /* stores BLEDevice info in memory cache after retrieving complementary data */
    static byte store(BLEAdvertisedDevice &advertisedDevice, bool populate=true) {
      BLEDevCacheIndex++;
      BLEDevCacheIndex=BLEDevCacheIndex%BLEDEVCACHE_SIZE;
      BLEDevCache[BLEDevCacheIndex].reset(); // avoid mixing new and old data
      byte cacheIndex = BLEDevCacheIndex;
      BLEDevCache[cacheIndex].borderColor = WROVER_RED;
      BLEDevCache[cacheIndex].in_db = false;
      BLEDevCache[cacheIndex].address = advertisedDevice.getAddress().toString().c_str();
      //BLEDevCache[cacheIndex].spower = String( (int)advertisedDevice.getTXPower() );
      if(populate) {
        BLEDevCache[cacheIndex].ouiname = DB.getOUI( BLEDevCache[cacheIndex].address ); // TODO : procrastinate this
      } else {
        BLEDevCache[cacheIndex].ouiname = "[unpopulated]";
      }
      BLEDevCache[cacheIndex].rssi = String ( advertisedDevice.getRSSI() );
      if (advertisedDevice.haveName()) {
        BLEDevCache[cacheIndex].name = String ( advertisedDevice.getName().c_str() );
      } else {
        BLEDevCache[cacheIndex].name = "";
      }
      if (advertisedDevice.haveAppearance()) {
        BLEDevCache[cacheIndex].appearance = advertisedDevice.getAppearance();
      } else {
        BLEDevCache[cacheIndex].appearance = "";
      }
      if (advertisedDevice.haveManufacturerData()) {
        std::string md = advertisedDevice.getManufacturerData();
        uint8_t* mdp = (uint8_t*)advertisedDevice.getManufacturerData().data();
        char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
        uint8_t vlsb = mdp[0];
        uint8_t vmsb = mdp[1];
        uint16_t vint = vmsb * 256 + vlsb;
        if(populate) {
          BLEDevCache[cacheIndex].vname = DB.getVendor( vint ); // TODO : procrastinate this
        } else {
          BLEDevCache[cacheIndex].vname = "[unpopulated]";
        }
        BLEDevCache[cacheIndex].vdata = String ( pHex );
      } else {
        BLEDevCache[cacheIndex].vname = "";
        BLEDevCache[cacheIndex].vdata = "";
      }
      if (advertisedDevice.haveServiceUUID()) {
        BLEDevCache[cacheIndex].uuid = String( advertisedDevice.getServiceUUID().toString().c_str() );
      } else {
        BLEDevCache[cacheIndex].uuid = "";
      }
      return cacheIndex;          
    }


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
      Serial.printf("****** Freezing cache index %d into NVS as index %s : %s\n", cacheindex, freezename, BLEDevCache[cacheindex].address.c_str());
      preferences.begin(freezename.c_str(), false);
      preferences.putBool("appearance",   BLEDevCache[cacheindex].in_db);
      preferences.putString("appearance", BLEDevCache[cacheindex].appearance);
      preferences.putString("name",       BLEDevCache[cacheindex].name);
      preferences.putString("address",    BLEDevCache[cacheindex].address);
      preferences.putString("ouiname",    BLEDevCache[cacheindex].ouiname);
      preferences.putString("rssi",       BLEDevCache[cacheindex].rssi);
      preferences.putString("vdata",      BLEDevCache[cacheindex].vdata);
      preferences.putString("vname",      BLEDevCache[cacheindex].vname);
      preferences.putString("uuid",       BLEDevCache[cacheindex].uuid);
      preferences.end();
      return freezecounter;
    }

    /* extract BLECache device from nvram into cache */
    static void thaw(byte freezeindex) {
      String freezename = "cache-"+String(freezeindex);
      preferences.begin(freezename.c_str(), true);
      BLEDevCacheIndex++;
      BLEDevCacheIndex=BLEDevCacheIndex%BLEDEVCACHE_SIZE;
      BLEDevCache[BLEDevCacheIndex].reset(); // avoid mixing new and old data
      BLEDevCache[BLEDevCacheIndex].in_db       = preferences.getBool("in_db", false);
      BLEDevCache[BLEDevCacheIndex].appearance  = preferences.getString("appearance", "");
      BLEDevCache[BLEDevCacheIndex].name        = preferences.getString("name", "");
      BLEDevCache[BLEDevCacheIndex].address     = preferences.getString("address", "");
      BLEDevCache[BLEDevCacheIndex].ouiname     = preferences.getString("ouiname", "");
      BLEDevCache[BLEDevCacheIndex].rssi        = preferences.getString("rssi", "");
      BLEDevCache[BLEDevCacheIndex].vdata       = preferences.getString("vdata", "");
      BLEDevCache[BLEDevCacheIndex].vname       = preferences.getString("vname", "");
      BLEDevCache[BLEDevCacheIndex].uuid        = preferences.getString("uuid", "");
      BLEDevCache[BLEDevCacheIndex].borderColor = WROVER_CYAN;
      BLEDevCache[BLEDevCacheIndex].textColor   = WROVER_DARKGREY;
      if(BLEDevCache[BLEDevCacheIndex].address != "") {
        Serial.printf("****** Thawing pref index %d into cache index %d : %s\n", freezeindex, BLEDevCacheIndex, BLEDevCache[BLEDevCacheIndex].address.c_str());
      }
      preferences.end();
    }

    /* extract all BLECache devices from nvram */
    static void thaw() {
      for(byte i=0;i<MAX_ITEMS_IN_PREFS;i++) {
        thaw(i);
        if(BLEDevCache[BLEDevCacheIndex].address!="") {
          if(BLEDevCache[BLEDevCacheIndex].ouiname == "[unpopulated]"){
            Serial.println("ouiname-populating " + BLEDevCache[BLEDevCacheIndex].address);
            BLEDevCache[BLEDevCacheIndex].ouiname = DB.getOUI( BLEDevCache[BLEDevCacheIndex].address );
          }
          if(BLEDevCache[BLEDevCacheIndex].vname == "[unpopulated]") {
            if(BLEDevCache[BLEDevCacheIndex].vdata!="" && BLEDevCache[BLEDevCacheIndex].vdata.length()>=4) {
              //Serial.println("vname-populating " + BLEDevCache[BLEDevCacheIndex].address);
              char hex0[3];
              char hex1[3];
              strcpy(hex0, BLEDevCache[BLEDevCacheIndex].vdata.substring(0,2).c_str());
              strcpy(hex1, BLEDevCache[BLEDevCacheIndex].vdata.substring(2,4).c_str());
              //Serial.printf("  vname-info hex0:%s: hex1:%s\n", hex0, hex1);
              uint8_t vlsb = strtoul( hex0, nullptr, 16);
              uint8_t vmsb = strtoul( hex1, nullptr, 16);
              //Serial.printf("  vname-info lsb:%d msb:%d\n", vlsb, vmsb);
              uint16_t vint = vmsb * 256 + vlsb;
              BLEDevCache[BLEDevCacheIndex].vname = DB.getVendor( vint );
              //Serial.println("  vname: " + BLEDevCache[BLEDevCacheIndex].vname);
            } else {
              //Serial.println("vname-clearing " + BLEDevCache[BLEDevCacheIndex].address);
              BLEDevCache[BLEDevCacheIndex].vname = ""; 
            }
          }

          UI.headerStats("Defrosted "+String(BLEDevCacheIndex)+"#" + String(i));
          UI.printBLECard( BLEDevCache[BLEDevCacheIndex] );
          UI.footerStats();          
        }
      }
    }


    bool feed() {
      bool fed = false;
      for(int i=0;i<BLEDEVCACHE_SIZE;i++) {
        if(BLEDevCache[i].address == "") continue;
        if(BLEDevCache[i].in_db == true) continue;
        if(isAnonymousDevice( i )) continue;
        Serial.println("####### Feeding thawed " + BLEDevCache[i].address + " to DB");
        if(DB.insertBTDevice( i ) == INSERTION_SUCCESS) {
          fed = true;
          BLEDevCache[i].in_db == true;
        }
      }
      return fed;
    }

    static void onScanDone(BLEScanResults foundDevices) {
      UI.headerStats("Showing results ...");
      String headerMessage = "                    ";
      byte cacheIndex;
      devicesCount = foundDevices.getCount();
      sessDevicesCount += devicesCount;
      for (int i = 0; i < devicesCount; i++) {
        BLEAdvertisedDevice advertisedDevice = foundDevices.getDevice(i);
        String address = advertisedDevice.getAddress().toString().c_str();
        if( UI.BLECardIsOnScreen( address ) ) { 
          // avoid repeating last printed card
          SelfCacheHit++;
          UI.headerStats("Ignoring #" + String(i));
          UI.footerStats();
          continue;
        }

        // make sure it's in cache first
        int deviceIndexIfExists = getDeviceCacheIndex( address );
        if(deviceIndexIfExists>-1) {
          // load from cache
          cacheIndex = deviceIndexIfExists;
          BLEDevCache[cacheIndex].borderColor = IN_CACHE_COLOR;
          BLEDevCache[cacheIndex].vdata = ""; // hack to free some heap ? this has already been decoded anyway
          BLEDevCache[cacheIndex].textColor = isAnonymousDevice( cacheIndex ) ? ANONYMOUS_COLOR : NOT_ANONYMOUS_COLOR;
          headerMessage = "Cache "+String(cacheIndex)+"#";
        } else {
          if(!DB.isOOM) {
            deviceIndexIfExists = DB.deviceExists( address ); // will load from DB if necessary
          }
          if(deviceIndexIfExists>-1) {
            cacheIndex = deviceIndexIfExists;
            BLEDevCache[cacheIndex].borderColor = IN_CACHE_COLOR;
            BLEDevCache[cacheIndex].textColor = NOT_ANONYMOUS_COLOR;
            headerMessage = "DB Seen "+String(cacheIndex)+"#";
          } else {
            newDevicesCount++;
            if(DB.isOOM) { // newfound but OOM, gather what's left of data without DB
              cacheIndex = store( advertisedDevice, false ); // store data in cache but don't populate
              // freeze it partially ...
              BLEDevCache[cacheIndex].in_db = false;
              byte prefIndex = freeze( cacheIndex );
              // don't render it (will be thawed, populated, inserted and rendered on reboot)
              continue;
            } else { // newfound
              cacheIndex = store( advertisedDevice ); // store data in cache
              if(!isAnonymousDevice( cacheIndex )) {
                if(DB.insertBTDevice( cacheIndex ) == INSERTION_SUCCESS) {
                  entries++;
                  prune_trigger++;
                  newDevicesCount++;
                  BLEDevCache[cacheIndex].in_db = true;
                  BLEDevCache[cacheIndex].textColor = NOT_ANONYMOUS_COLOR;
                  BLEDevCache[cacheIndex].borderColor = NOT_IN_CACHE_COLOR;
                  headerMessage = "Inserted "+String(cacheIndex)+"#";
                  //byte prefIndex = freeze( cacheIndex );
                } else {
                  // DB Error, freeze it in NVS!
                  BLEDevCache[cacheIndex].in_db = false;
                  byte prefIndex = freeze( cacheIndex );
                  // don't render it (will be thawed, rendered and inserted on reboot)
                  continue;
                }
              } else {
                BLEDevCache[cacheIndex].borderColor = IN_CACHE_COLOR;
                BLEDevCache[cacheIndex].textColor = ANONYMOUS_COLOR;
                AnonymousCacheHit++;
                headerMessage = "Anon "+String(cacheIndex)+"#";
              }
            }
          }
        }

        UI.headerStats(headerMessage + String(i));
        UI.printBLECard( BLEDevCache[cacheIndex] ); // TODO : procrastinate this
        UI.footerStats();
      }
      
      if( DB.isOOM ) {
        Serial.println("[DB ERROR] restarting");
        delay(1000);
        ESP.restart();
      }
      
    }


    void scan() {
      UI.headerStats("Scan in progress...");
      UI.footerStats();
      // synchronous scan: blink icon and draw time-based scan progress in a separate task
      // while using the callback to update its status in real time
      UI.taskBlink();
      BLEScan *pBLEScan = BLEDevice::getScan(); //create new scan
      pBLEScan->setAdvertisedDeviceCallbacks(new FoundDeviceCallback());
      pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
      pBLEScan->setInterval(0x50); // 0x50
      pBLEScan->setWindow(0x30); // 0x30
      //BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME);
      //onScanDone( foundDevices );
      pBLEScan->start(SCAN_TIME, onScanDone);
      UI.update(); // run after-scan display stuff
      DB.maintain(); // check for db pruning
      Serial.printf("Cache hits -- Cards:%s Self:%s Anonymous:%s, Oui:%s Vendor:%s\n", 
        String(BLEDevCacheHit).c_str(), 
        String(SelfCacheHit).c_str(), 
        String(AnonymousCacheHit).c_str(), 
        String(OuiCacheHit).c_str(), 
        String(VendorCacheHit).c_str()
      );
    }

};


#endif

BLEScanUtils BLECollector;
