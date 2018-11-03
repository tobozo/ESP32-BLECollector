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
      WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
      BLEDevice::init("");
    }


    static DeviceCacheStatus deviceExists(String bleDeviceAddress) {
      DeviceCacheStatus internalStatus;
      for(int i=0;i<BLEDEVCACHE_SIZE;i++) {
        if( BLEDevCache[i].address == bleDeviceAddress) {
          BLEDevCacheHit++;
          internalStatus.exists = true;
          internalStatus.index = i;
          return internalStatus;
        }
      }
      internalStatus.index = DB.deviceExists(bleDeviceAddress);
      if(internalStatus.index >=0) {
        internalStatus.exists = true;
      }
      return internalStatus;
    }


    static bool isAnonymousDevice(BlueToothDevice &bleDevice) {
      if(bleDevice.uuid!="") return false;
      if(bleDevice.name!="") return false;
      if(bleDevice.appearance!="") return false;
      if(bleDevice.ouiname=="[private]" || bleDevice.ouiname=="") return true;
      if(bleDevice.vname=="[unknown]" || bleDevice.vname=="") return true;
      if(bleDevice.vname!="" && bleDevice.ouiname!="") {
        return false;
      }
      return true;
    }


    static byte store(BLEAdvertisedDevice &advertisedDevice) {
      BLEDevCacheIndex++;
      BLEDevCacheIndex=BLEDevCacheIndex%BLEDEVCACHE_SIZE;
      byte cacheIndex = BLEDevCacheIndex;
      BLEDevCache[cacheIndex].borderColor = WROVER_RED;
      BLEDevCache[cacheIndex].in_db = false;
      BLEDevCache[cacheIndex].address = advertisedDevice.getAddress().toString().c_str();
      BLEDevCache[cacheIndex].spower = String( (int)advertisedDevice.getTXPower() );
      BLEDevCache[cacheIndex].ouiname = DB.getOUI( BLEDevCache[cacheIndex].address );
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
        BLEDevCache[cacheIndex].vname = DB.getVendor( vint );
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
        // check if device is in cache
        DeviceCacheStatus BLEDevStatus = deviceExists( address );
        if ( BLEDevStatus.exists && BLEDevStatus.index >=0 ) { // exists in cache
          cacheIndex = BLEDevStatus.index;
          if( isAnonymousDevice(BLEDevCache[cacheIndex]) ) {
            BLEDevCache[cacheIndex].borderColor = IN_CACHE_COLOR;
            BLEDevCache[cacheIndex].textColor = ANONYMOUS_COLOR;
            //Serial.println("CACHED ANONYMOUS: " + BLEDevCache[cacheIndex].address);
          } else {
            BLEDevCache[cacheIndex].borderColor = IN_CACHE_COLOR;
            BLEDevCache[cacheIndex].textColor = NOT_ANONYMOUS_COLOR;
            //Serial.println("CACHED NOT ANONYMOUS: " + BLEDevCache[cacheIndex].address);
          }
          headerMessage = "Result #";
        } else { // not in cache, gather the data
          cacheIndex = store( advertisedDevice );
          if( DB.isOOM ) {
            Out.println("[DB ERROR] restarting");
            delay(1000);
            ESP.restart();
          }
          if(isAnonymousDevice( BLEDevCache[cacheIndex] )) { 
            BLEDevCache[cacheIndex].borderColor = NOT_IN_CACHE_COLOR;
            BLEDevCache[cacheIndex].textColor = ANONYMOUS_COLOR;
            //Serial.println("SKIPPED ANONYMOUS: " + BLEDevCache[cacheIndex].address);
            headerMessage = "Skipped #";
            newDevicesCount++;
            AnonymousCacheHit++;
          } else {
            if(DB.insertBTDevice( BLEDevCache[cacheIndex] ) == INSERTION_SUCCESS) {
              entries++;
              prune_trigger++;
              newDevicesCount++;
              BLEDevCache[cacheIndex].in_db = true;
              BLEDevCache[cacheIndex].borderColor = NOT_IN_CACHE_COLOR;
              BLEDevCache[cacheIndex].textColor = NOT_ANONYMOUS_COLOR;
              //Serial.println("INSERTED NON ANONYMOUS: " + BLEDevCache[cacheIndex].address);
              headerMessage = "Inserted #";
            } else { // out of memory ?
              UI.headerStats("DB Error..!");
              BLEDevCache[cacheIndex].borderColor = WROVER_RED;
              BLEDevCache[cacheIndex].textColor = WROVER_RED;
              //Serial.println("FAILED INSERTING NON ANONYMOUS: " + BLEDevCache[cacheIndex].address);
            }
          }
        }
        UI.headerStats(headerMessage + String(i));
        UI.printBLECard( BLEDevCache[cacheIndex] );
        UI.footerStats();
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
      Serial.printf("Cache hits -- Cards:%s Self:%s Transient:%s, Oui:%s Vendor:%s\n", 
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
