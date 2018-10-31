/*
  class FoundDeviceCallback: public BLEAdvertisedDeviceCallbacks {
    bool toggler = true;
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      //Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
      toggler = !toggler;
      if(toggler) {
        tft.fillCircle(ICON_X, ICON_Y, ICON_R, WROVER_BLUE);
      } else {
        tft.fillCircle(ICON_X, ICON_Y, ICON_R-1, HEADER_BGCOLOR);
      }
    }
  };
*/

int deviceExists(String bleDeviceAddress) {
  // try fast answer first
  for(int i=0;i<DEVICEPOOL_SIZE;i++) {
    if( BLEDevCache[i].address == bleDeviceAddress) {
      BLEDevCacheHit++;
      return i;
    }
  }
  results = 0;
  sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB);
  String requestStr = "SELECT * FROM blemacs WHERE address='" + bleDeviceAddress + "'";
  int rc = sqlite3_exec(BLECollectorDB, requestStr.c_str(), BLEDev_db_callback, (void*)dataBLE, &zErrMsg);
  if (rc != SQLITE_OK) {
    Out.printf("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  }
  sqlite3_close(BLECollectorDB);
  // if the device exists, it's been loaded into BLEDevCache[BLEDevCacheIndex]
  return results>0 ? BLEDevCacheIndex : -1;
}


DBMessage insertBTDevice(BlueToothDevice &bleDevice) {
  sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB);
  sprintf(insertQuery, insertQueryTemplate,
    escape(bleDevice.appearance).c_str(),
    escape(bleDevice.name).c_str(),
    escape(bleDevice.address).c_str(),
    escape(bleDevice.ouiname).c_str(),
    escape(bleDevice.rssi).c_str(),
    escape(bleDevice.vdata).c_str(),
    escape(bleDevice.vname).c_str(),
    escape(bleDevice.uuid).c_str(),
    escape(bleDevice.spower).c_str()
  );
  
  int rc = db_exec(BLECollectorDB, insertQuery);
  if (rc != SQLITE_OK) {
    Out.println("[SQL ERROR] INSERTION FAILED");
    Serial.println("Heap level:" + String(freeheap));
    //Serial.println(requestStr);
    Serial.println(insertQuery);
    sqlite3_close(BLECollectorDB);
    return INSERTION_FAILED;
  }
  //requestStr = "";
  sqlite3_close(BLECollectorDB);
  return INSERTION_SUCCESS;
  /*
  if (RTC_is_running) {
    // don't use this if you want to keep your SD Card alive :)
    Serial.println("Found " + String(results) + " results, incrementing");
    requestStr  = "UPDATE blemacs set hits=hits+1 WHERE address='"+bleDevice.address +"'";
    const char * request = requestStr.c_str();
    rc = db_exec(BLECollectorDB, request);
    if (rc != SQLITE_OK) {
    Serial.println("[ERROR] INCREMENT FAILED");
    sqlite3_close(BLECollectorDB);
    return INCREMENT_FAILED;
    }
  }
  sqlite3_close(BLECollectorDB);
  return INCREMENT_SUCCESS;
  */
}


void onScanDone(BLEScanResults foundDevices) {
  headerStats("Showing results ...");
  devicesCount = foundDevices.getCount();
  sessDevicesCount += devicesCount;
  for (int i = 0; i < devicesCount; i++) {
    BLEAdvertisedDevice advertisedDevice = foundDevices.getDevice(i);
    String address = advertisedDevice.getAddress().toString().c_str();
    int BLECacheIndex = deviceExists( address );
    
    if ( BLECacheIndex >=0 ) {
      BLEDevCache[BLECacheIndex].in_db = true;
      BLEDevCache[BLECacheIndex].deviceColor = WROVER_DARKGREY;
      bool onScreen = false;
      for(int j=0;j<BLECARD_MAC_CACHE_SIZE;j++) {
        if( address == lastPrintedMac[j]) {
          onScreen = true;
        }
      }
      if( onScreen ) {
        // avoid repeating last printed card
        SelfCacheHit++;
        headerStats("Ignoring #" + String(i));
      } else {
        headerStats("Result #" + String(i));
        Out.printBLECard( BLEDevCache[BLECacheIndex] );
      }
      footerStats();
      continue;
    } else {
      BLEDevCacheIndex++;
      BLEDevCacheIndex=BLEDevCacheIndex%DEVICEPOOL_SIZE;
      BLEDevCache[BLEDevCacheIndex].deviceColor = WROVER_RED;
      BLEDevCache[BLEDevCacheIndex].in_db = false;
      BLEDevCache[BLEDevCacheIndex].address = address;
      BLEDevCache[BLEDevCacheIndex].spower = String( (int)advertisedDevice.getTXPower() );
      BLEDevCache[BLEDevCacheIndex].ouiname = getOUI( address );
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
        std::string md = advertisedDevice.getManufacturerData();
        uint8_t* mdp = (uint8_t*)advertisedDevice.getManufacturerData().data();
        char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
        uint8_t vlsb = mdp[0];
        uint8_t vmsb = mdp[1];
        uint16_t vint = vmsb * 256 + vlsb;
        BLEDevCache[BLEDevCacheIndex].vname = getVendor( vint );
        BLEDevCache[BLEDevCacheIndex].vdata = String ( pHex );
      } else {
        BLEDevCache[BLEDevCacheIndex].vname = "";
        BLEDevCache[BLEDevCacheIndex].vdata = "";
      }
      if (advertisedDevice.haveServiceUUID()) {
        BLEDevCache[BLEDevCacheIndex].uuid = String( advertisedDevice.getServiceUUID().toString().c_str() );
      } else {
        BLEDevCache[BLEDevCacheIndex].uuid = "";
      }
      if(insertBTDevice( BLEDevCache[BLEDevCacheIndex] ) == INSERTION_SUCCESS) {
        entries++;
        prune_trigger++;
        newDevicesCount++;
        BLEDevCache[BLEDevCacheIndex].deviceColor = WROVER_WHITE;
      } else { // out of memory ?
        headerStats("DB Error..!");
        delay(1000);
        ESP.restart();
      }
    }
    headerStats("Result #" + String(i));
    Out.printBLECard( BLEDevCache[BLEDevCacheIndex] );
    footerStats();
  }
}


void doBLEScan() {
  headerStats("Scan in progress ...");
  footerStats();
  // blink icon and draw scan progress in a separate task
  xTaskCreatePinnedToCore(blinkBlueIcon, "BlinkBlueIcon", 1000, NULL, 0, NULL, 0); /* last = Task Core */
  BLEScan *pBLEScan = BLEDevice::getScan(); //create new scan
  // using callback has no added value here, it just triggers the watchdog
  // pBLEScan->setAdvertisedDeviceCallbacks(new FoundDeviceCallback());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(0x50); // 0x50
  pBLEScan->setWindow(0x30); // 0x30
  //BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME);
  pBLEScan->start(SCAN_TIME, onScanDone);
}
