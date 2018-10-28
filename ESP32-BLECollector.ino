/*

  ESP32 BLE Collector - A BLE scanner with sqlite data persistence on the SD Card
  
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


  Hardware requirements:
    - ESP32 (with or without PSRam)
    - SD Card breakout (e.g. Wrover-Kit, M5Stack, LoLinD32 Pro)
    - Micro SD (FAT32 formatted, max 32GB)
    - 'mac-oui-light.db' and 'ble-oui.db' files copied on the Micro SD Card root

  Arduino IDE Settings:
    - Partition Scheme : No OTA (Large APP)


*/

#include <Arduino.h>

#include <Adafruit_GFX.h>    // Core graphics library
#include "WROVER_KIT_LCD.h" // Must have the VScroll def patch: https://github.com/espressif/WROVER_KIT_LCD/pull/3/files

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h> // https://github.com/siara-cc/esp32_arduino_sqlite3_lib
#include <SPI.h>
#include <FS.h>
#include <SD_MMC.h>

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// because ESP.getFreeHeap() is inconsistent across SDK versions
// use the primitive... eats 25Kb memory
#define freeheap heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
#define SCAN_TIME  10 // seconds
#define MAX_FIELD_LEN 32 // max chars returned by field
#define MAX_ROW_LEN 37 // max chars per line on display

#define ICON_X 4
#define ICON_Y 4
#define ICON_R 4

WROVER_KIT_LCD tft;

sqlite3 *BLECollectorDB; // read/write
sqlite3 *BLEVendorsDB; // readonly
sqlite3 *OUIVendorsDB; // readonly

uint16_t scrollTopFixedArea = 0;
uint16_t scrollBottomFixedArea = 0;
uint16_t textHeight = 16;
uint16_t yStart = scrollTopFixedArea;
uint16_t tft_height = tft.height();//ILI9341_HEIGHT;//tft.height();
uint16_t tft_width  = tft.width();//ILI9341_WIDTH;//tft.width();
uint16_t yArea = tft_height - scrollTopFixedArea - scrollBottomFixedArea;

int scrollPosY = -1;
int scrollPosX = -1;
uint16_t headerArea = 0;


struct BlueToothDevice {
  int id;
  bool in_db = false;
  String appearance = "";
  String name = ""; // device name
  String address = ""; // device mac address
  String ouiname = ""; // oui vendor name (from mac address, see oui.h)
  String rssi = "";
  String vdata = ""; // manufacturer data
  String vname = ""; // manufacturer name (from manufacturer data, see ble-oui.db)
  String uuid = ""; // service uuid
  String spower = "";
  String toString() {
    return 
      (appearance!="" ? "App: " + appearance + "\t" : "") +
      (name!="" ? "Name: " + name + "\t" : "") +
      (address!="" ? "Addr: " + address + "\t" : "") +
      (ouiname!="" ? "OUI: " + ouiname + "\t" : "") +
      (rssi!="" ? "RSSI: " + rssi + "\t" : "") +
      (vdata!="" ? "VData: " + vdata + "\t" : "") +
      (vname!="" ? "VName: " + vname + "\t" : "") +
      (uuid!="" ? "UUID: " + uuid + "\t" : "")+
      (spower!="" ? "txPow: " + spower : "")
    ;
  }

};


class FoundDeviceCallback: public BLEAdvertisedDeviceCallbacks {
    bool toggler = true;
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      //Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
      toggler = !toggler;
      if(toggler) {
        tft.fillCircle(ICON_X, ICON_Y, ICON_R, WROVER_BLUE);
      } else {
        tft.fillCircle(ICON_X, ICON_Y, ICON_R-1, WROVER_BLACK);
      }
    }
};


enum DBMessage {
  TABLE_CREATION_FAILED = -1,
  INSERTION_FAILED = -2,
  INCREMENT_FAILED = -3,
  INSERTION_SUCCESS = 1,
  INCREMENT_SUCCESS = 2
};


void setupScrollArea(uint16_t TFA, uint16_t BFA, bool clear=false) {
  tft.setCursor(0, TFA);
  tft.setupScrollArea(TFA, BFA);
  scrollPosY = TFA;
  scrollTopFixedArea = TFA;
  scrollBottomFixedArea = BFA;
  yStart = scrollTopFixedArea;
  yArea = tft_height - scrollTopFixedArea - scrollBottomFixedArea;
  Serial.printf("*** NEW Scroll Setup: Top=%d Bottom=%d YArea=%d\n", TFA, BFA, yArea);
  if(clear) {
    tft.fillRect(0, TFA, tft_width, yArea, WROVER_BLACK);
  }
}


int scroll_slow(int lines, int wait) {
  int yTemp = yStart;
  scrollPosY = -1;
  for (int i = 0; i < lines; i++) {
    yStart++;
    if (yStart == tft_height - scrollBottomFixedArea) yStart = scrollTopFixedArea;
    tft.scrollTo(yStart);
    delay(wait);
  }
  return  yTemp;
}


struct OutputService {
  int printf(char* fmt ...) {
    char buf[1024]; // resulting string limited to 1024 chars
    va_list args;
    va_start (args, fmt );
    vsnprintf(buf, 1024, fmt, args);
    va_end (args);
    return print(buf);
  }
  int println(String str=" ") {
    return print(str + "\n");
  }
  int print(String str) {
    Serial.print( str );
    return scroll(str);
  }
  int printBLECard(BlueToothDevice BLDev) {
      uint16_t randomcolor = tft.color565(random(128, 255), random(128, 255), random(128, 255));
      uint16_t pos = 0;
      tft.setTextColor(randomcolor);
      //pos+=println("/---------------------------------------");
      pos+=println(" ");

      if(BLDev.address.length() + BLDev.rssi.length() < MAX_ROW_LEN) {
        uint8_t len = MAX_ROW_LEN - (BLDev.address.length() + BLDev.rssi.length());
        pos+=println( "  " + BLDev.address + String(std::string(len, ' ').c_str()) + BLDev.rssi );
        //String(foo.c_str());
      } else {
        if(BLDev.address!="") pos+=println("  Addr: " + BLDev.address);
        if(BLDev.rssi!="") pos+=println("  RSSI: " + BLDev.rssi);
      }
      
      if(BLDev.appearance!="") pos+=println("  Appearance: " + BLDev.appearance);
      if(BLDev.name!="") pos+=println("  Name: " + BLDev.name);

      if(BLDev.ouiname!="" && BLDev.ouiname!="[private]") pos+=println("  OUI: " + BLDev.ouiname);

      //f(BLDev.vdata!="") pos+=println("  HAS VData");
      if(BLDev.vname!="" && BLDev.vname!="[unknown]") pos+=println("  VName: " + BLDev.vname);
      if(BLDev.uuid!="") pos+=println("  HAS UUID");
      if(BLDev.spower!="") pos+=println("  txPow: " + BLDev.spower);
      pos+=println(" ");
      return pos;
  }
  int scroll(String str) {
    if(scrollPosY == -1) {
      scrollPosY = tft.getCursorY();
    }
    scrollPosX = tft.getCursorX();
    if(scrollPosY>=(tft_height-scrollBottomFixedArea)) {
      scrollPosY = (scrollPosY%(tft_height-scrollBottomFixedArea)) + scrollTopFixedArea;
    }
    int16_t  x1, y1;
    uint16_t w, h;
    tft.getTextBounds(str, scrollPosX, scrollPosY, &x1, &y1, &w, &h);

    if(scrollPosX==0) {
      tft.fillRect(0, scrollPosY, tft_width, h, WROVER_BLACK);
    } else {
      tft.fillRect(0, scrollPosY, w, h, WROVER_BLACK);
    }
    tft.setCursor(scrollPosX, scrollPosY);
    scroll_slow(h, 5); // Scroll lines, 5ms per line   
    tft.print(str);
    
    scrollPosY = tft.getCursorY();

    return h;
  }
};

OutputService Out;

int results = 0;
unsigned int entries = 0;
byte prune_trigger = 0; // incremented on every insertion, reset on prune()
byte prune_threshold = 10; // prune every x inertions
bool print_results = false;
bool print_tabular = true;
char *colNeedle = 0;
const char BACKSLASH = '\\';
String colValue = "";
uint32_t min_free_heap = 120000; // sql out of memory errors occur under 120000


String escape(String str, String what="'") {
    str.replace(String(what), String(BACKSLASH) + String(what));
    return str;
}


int StrToHex(char str[]) {
  return (int) strtol(str, 0, 16);
}


const char* data = "cb";
static int callback(void *data, int argc, char **argv, char **azColName){
  results++;
  //if(!print_results) return 0;
  int i;
  if(results==1 && colNeedle==0 ) {
    if(print_results && print_tabular) {
      String out = "--- ";
      for (i = 0; i<argc; i++){
        out += String(azColName[i]) + " ";
      }
      Out.println(out);
    }
  }
  String out = "";
  for (i = 0; i<argc; i++){
    if(colNeedle!=0) {
      if(String(colNeedle)==String(azColName[i])) {
        colValue = argv[i] ? argv[i] : "";
      }
      continue;
    }
    if(print_results) {
      if(print_tabular) {
        out += String(argv[i] ? argv[i] : "NULL")+" ";
      } else {
        Out.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
      }
    }
  }
  if(print_results && colNeedle==0) {
    Out.println(out);
  }
  return 0;
}


char *zErrMsg = 0;
int db_exec(sqlite3 *db, const char *sql, bool _print_results=false, char *_colNeedle=0) {
  results = 0;
  print_results = _print_results;
  colNeedle = _colNeedle;
  colValue = "";
  //Serial.println(sql);
  //long start = micros();
  int rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
  if (rc != SQLITE_OK) {
    Out.printf("SQL error: %s\n", zErrMsg);
    if(String(zErrMsg)=="database disk image is malformed") {
      resetDB();
    }
    sqlite3_free(zErrMsg);
  } else {
    //Serial.printf("Operation done successfully\n");
  }
  //Serial.print(F("Time taken:")); //Serial.println(micros()-start);
  return rc;
}

void headerStats() {
  int16_t posX = tft.getCursorX();
  int16_t posY = tft.getCursorX();
  int16_t x1, y1;
  uint16_t w, h;

  String s_heap = "Heap: "+String(freeheap);
  String s_entries = "Entries: "+String(entries);

  tft.setTextColor(WROVER_RED);

  tft.getTextBounds(s_heap, 128, 1, &x1, &y1, &w, &h);
  tft.setCursor(tft_width-w, 0);
  tft.fillRect(tft_width-w, 0, w, h, WROVER_BLACK);
  tft.print(s_heap);

  tft.getTextBounds(s_entries, 128, 16, &x1, &y1, &w, &h);
  tft.setCursor(tft_width-w, 16);
  tft.fillRect(tft_width-w, 16, w, h, WROVER_BLACK);
  tft.print(s_entries);

  tft.setCursor(posX, posY);
}

void initUI() {
  tft.begin();
  tft.setRotation( 0 ); // required to get smooth scrolling
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_YELLOW);
  String welcomeMSG = (String)"   ESP32 BLE Collector\n"
    + "----------------------------------------\n"
    + "Scan in progress ...\n"
    + "----------------------------------------\n"
  ;
  int16_t  x1, y1;
  uint16_t w;
  tft.getTextBounds(welcomeMSG, 0, 1, &x1, &y1, &w, &headerArea);
  tft.setCursor(0, 1);
  tft.print(welcomeMSG);
  //headerArea = tft.getCursorY();
  headerStats();
  setupScrollArea(headerArea, 0);
}


void showDataSamples() {
  sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB);
  tft.setTextColor(WROVER_YELLOW);
  Out.println(" \nCollected Named Devices");
  tft.setTextColor(WROVER_PINK);
  db_exec(BLECollectorDB, "SELECT DISTINCT SUBSTR(name,0,32) FROM blemacs where TRIM(name)!=''", true);
  tft.setTextColor(WROVER_YELLOW);
  Out.println(" \nCollected Devices Vendors");
  tft.setTextColor(WROVER_PINK);
  db_exec(BLECollectorDB, "SELECT DISTINCT SUBSTR(vname,0,32) FROM blemacs where TRIM(vname)!=''", true);
  tft.setTextColor(WROVER_YELLOW);
  Out.println(" \nCollected Devices MAC's Vendors");
  tft.setTextColor(WROVER_PINK);
  db_exec(BLECollectorDB, "SELECT DISTINCT SUBSTR(ouiname,0,32) FROM blemacs where TRIM(ouiname)!=''", true);
  sqlite3_close(BLECollectorDB);
}


unsigned int getEntries(bool _display_results=false) {
  sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB);
  if(_display_results) {
    db_exec(BLECollectorDB, "SELECT appearance, name, address, ouiname, rssi, vdata, vname, uuid, spower FROM blemacs;", true);
  } else {
    db_exec(BLECollectorDB, "SELECT count(*) FROM blemacs;", true, "count(*)");
    results = atoi(colValue.c_str());
  }
  sqlite3_close(BLECollectorDB);
  return results;
}


String getVendor(uint16_t devid) {
  sqlite3_open("/sdcard/ble-oui.db", &BLEVendorsDB); // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
  String requestStr = "SELECT vendor FROM 'ble-oui' WHERE id='"+ String(devid) +"'";
  db_exec(BLEVendorsDB, requestStr.c_str(), true, "vendor");
  requestStr = "";
  sqlite3_close(BLEVendorsDB);
  if(colValue!="" && colValue!="NULL") {
    if(colValue.length()>MAX_FIELD_LEN) {
      colValue = colValue.substring(0,MAX_FIELD_LEN);
    }
    return colValue;
  }
  return "[unknown]";
}


void resetDB() {
  Out.println("Re-creating database");
  SD_MMC.remove("/blemacs.db");
  sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB);
  db_exec(BLECollectorDB, "DROP TABLE IF EXISTS blemacs;");
  db_exec(BLECollectorDB, "CREATE TABLE blemacs(id INTEGER, appearance, name, address, ouiname, rssi, vdata, vname, uuid, spower, hits INTEGER);");
  sqlite3_close(BLECollectorDB);
  ESP.restart();
}


void pruneDB() {
  unsigned int before_pruning = getEntries();
  tft.setTextColor(WROVER_YELLOW);
  Out.println("Pruning DB");
  tft.setTextColor(WROVER_GREEN);
  sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB);
  db_exec(BLECollectorDB, "DELETE FROM blemacs WHERE appearance='' AND name='' AND spower='0' AND uuid='' AND ouiname='[private]' AND (vname LIKE 'Apple%' or vname='[unknown]')", true);
  sqlite3_close(BLECollectorDB);
  entries = getEntries();
  tft.setTextColor(WROVER_YELLOW);
  Out.println("DB haz now " + String(entries) + " entries ("+String(before_pruning-entries)+" removed)");
  prune_trigger = 0;
  headerStats();
}


void testVendorNames() {
  sqlite3_open("/sdcard/ble-oui.db", &BLEVendorsDB); // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
  Serial.println("Testing Vendor Names Database ...");
  db_exec(BLEVendorsDB, "SELECT SUBSTR(vendor,0,32)  FROM 'ble-oui' LIMIT 10", true);
  sqlite3_close(BLEVendorsDB);
  // 0x001D = Qualcomm
  String vendorname = getVendor(0x001D);
  if(vendorname!="Qualcomm") {
    Out.println(vendorname);
    Out.println("Vendor Names Test failed, looping");
    while(1) yield();
  } else {
    Out.println("Vendor Names Test success!");
  }  
}


void testOUI() {
  sqlite3_open("/sdcard/mac-oui-light.db", &OUIVendorsDB); // https://code.wireshark.org/review/gitweb?p=wireshark.git;a=blob_plain;f=manuf
  tft.setTextColor(WROVER_YELLOW);
  Out.println("Testing MAC OUI database ...");
  tft.setTextColor(WROVER_ORANGE);
  db_exec(OUIVendorsDB, "SELECT * FROM 'oui-light' limit 10", true);
  sqlite3_close(OUIVendorsDB);
  String ouiname = getOUI("B499BA" /*Hewlett Packard */);
  if(ouiname!="Hewlett Packard") {
    Out.println(ouiname);
    tft.setTextColor(WROVER_RED);
    Out.println("MAC OUI Test failed, looping");
    while(1) yield();
  } else {
    tft.setTextColor(WROVER_GREEN);
    Out.println("MAC OUI Test success!");
  }
}


String getOUI(String mac) {
  sqlite3_open("/sdcard/mac-oui-light.db", &OUIVendorsDB); // https://code.wireshark.org/review/gitweb?p=wireshark.git;a=blob_plain;f=manuf
  mac.replace(":", "");
  mac.toUpperCase();
  mac = mac.substring(0, 6);
  String requestStr = "SELECT * FROM 'oui-light' WHERE Assignment ='"+ mac +"';";
  db_exec(OUIVendorsDB, requestStr.c_str(), true, "Organization Name");
  requestStr = "";
  sqlite3_close(OUIVendorsDB);
  if(colValue!="" && colValue!="NULL") {
    if(colValue.length()>MAX_FIELD_LEN) {
      colValue = colValue.substring(0,MAX_FIELD_LEN);
    }
    return colValue;
  }
  return "[private]";
}


DBMessage insertOrUpdate(BlueToothDevice bleDevice) {
  sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB);
  String requestStr = "SELECT * FROM blemacs WHERE address='"+ bleDevice.address +"'";
  int rc = db_exec(BLECollectorDB, requestStr.c_str());
  requestStr = "";
  if (rc != SQLITE_OK) {
    Out.println("[WARN] SQL FAILED, creating table");
    rc = db_exec(BLECollectorDB, "CREATE TABLE IF NOT EXISTS blemacs(id INTEGER, appearance, name, address, ouiname, rssi, vdata, vname, uuid, spower, hits INTEGER);");
    if (rc != SQLITE_OK) {
      Out.println("TABLE CREATION FAILED");
      sqlite3_close(BLECollectorDB);
      return TABLE_CREATION_FAILED;
    }
  }
  if(results==0) {
    String requestStr  = "INSERT INTO blemacs(appearance, name, address, ouiname, rssi, vdata, vname, uuid, spower, hits) VALUES('"
      + escape(bleDevice.appearance)  +"', '"
      + escape(bleDevice.name) +"', '"
      + escape(bleDevice.address) +"', '"
      + escape(bleDevice.ouiname)  +"', '"
      + escape(bleDevice.rssi) +"', '"
      + escape(bleDevice.vdata) +"', '"
      + escape(bleDevice.vname) +"', '"
      + escape(bleDevice.uuid) +"', '"
      + escape(bleDevice.spower) +"', "
      +"1"+")"
    ;
    rc = db_exec(BLECollectorDB, requestStr.c_str());
    requestStr = "";
    if (rc != SQLITE_OK) {
      Out.println("[SQL ERROR] INSERTION FAILED");
      sqlite3_close(BLECollectorDB);
      return INSERTION_FAILED;
    }
    sqlite3_close(BLECollectorDB);
    return INSERTION_SUCCESS;
  } else {
    /*
    // don't use this if you want to keep your SD Card alive :)
    Serial.println("Found " + String(results) + " results, incrementing");
    requestStr  = "UPDATE blemacs set hits=hits+1 WHERE address='"+bleDevice.address +"'";
    const char * request = requestStr.c_str();
    rc = db_exec(BLECollectorDB, request);
    if (rc != SQLITE_OK) {
      Serial.println("[ERROR] INCREMENT FAILED");
      sqlite3_close(BLECollectorDB);
      return INCREMENT_FAILED;
    }*/
    sqlite3_close(BLECollectorDB);
    return INCREMENT_SUCCESS;
  }

}


int doBLEScan() {
  headerStats();
  tft.fillCircle(ICON_X, ICON_Y, ICON_R, WROVER_BLUE);
  BLEScan *pBLEScan = BLEDevice::getScan(); //create new scan
  // using callback has no added value here, plus it triggers the watchdog ...
  pBLEScan->setAdvertisedDeviceCallbacks(new FoundDeviceCallback());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(0x50); // 0x50
  pBLEScan->setWindow(0x30); // 0x30
  //Serial.printf("Start BLE scan for %d seconds...\n", SCAN_TIME);
  BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME);
  tft.fillCircle(ICON_X, ICON_Y, ICON_R+2, WROVER_BLACK);
  //tft.fillRect(0, 0, ICON_R*2, ICON_R*2, WROVER_BLACK);
  int devicesCount = foundDevices.getCount();
  for(int i=0;i<devicesCount;i++) {
    BLEAdvertisedDevice advertisedDevice = foundDevices.getDevice(i);
    BlueToothDevice BLEDev;
    BLEDev.spower = String( (int)advertisedDevice.getTXPower() );
    BLEDev.address = String( advertisedDevice.getAddress().toString().c_str() );
    BLEDev.ouiname = getOUI( BLEDev.address );
    BLEDev.rssi = String ( advertisedDevice.getRSSI() );
    if(advertisedDevice.haveName()) {
      BLEDev.name = String ( advertisedDevice.getName().c_str() );
    }
    if (advertisedDevice.haveAppearance()) {
      BLEDev.appearance = advertisedDevice.getAppearance();
    }
    if (advertisedDevice.haveManufacturerData()) {
      std::string md = advertisedDevice.getManufacturerData();
      uint8_t* mdp = (uint8_t*)advertisedDevice.getManufacturerData().data();
      char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
      uint8_t vlsb = mdp[0];
      uint8_t vmsb = mdp[1];
      uint16_t vint = vmsb*256 + vlsb;
      BLEDev.vname = getVendor( vint );
      BLEDev.vdata = String ( pHex );
    }
    if (advertisedDevice.haveServiceUUID()) {
      BLEDev.uuid = String( advertisedDevice.getServiceUUID().toString().c_str() );
    }
    DBMessage insertResult = insertOrUpdate( BLEDev );
    int h = Out.printBLECard( BLEDev );

    if(scrollPosY-h>=scrollTopFixedArea) {
      tft.drawRect(1, scrollPosY-h+1, tft_width-2, h-2, WROVER_WHITE);
    }
    
    switch(insertResult) {
      case INSERTION_SUCCESS:
        entries++;
        prune_trigger++;
        //tft.drawRect(1, scrollPosY-h+1, tft_width-2, h-2, WROVER_WHITE);
      break;
      case INCREMENT_SUCCESS:
        //tft.drawRect(1, scrollPosY-h+1, tft_width-2, h-2, WROVER_DARKGREY);
        BLEDev.in_db = true;
      break;
      case INCREMENT_FAILED:
      case INSERTION_FAILED:
        // out of memory ?
        Out.println("DB Error, will restart");
        ESP.restart();
      break;
    }
    headerStats();
  }
  return devicesCount;
}



void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(115200);
  Serial.println("ESP32 BLE Scanner");
  Serial.println("Free heap at boot: " + String(freeheap));


  initUI();
  
  // sorry this won't work with SPIFFS, too much ram is eaten by BLE functions :-(
  if (!SD_MMC.begin()) {
    Out.println("Card Mount Failed, will restart");
    delay(1000);
    ESP.restart();
  }

  sqlite3_initialize();

  BLEDevice::init("");

  pruneDB(); // remove unnecessary/redundant entries
  testOUI(); // test oui database
  testVendorNames(); // test vendornames database
  showDataSamples(); // print some of the collected values

/*
  // test scrolling
  while(1) {
    Out.println(String(millis()) + " 7a:33:60:9f:30:6c RSSI: -90 VData: 4c0007190102202b990f01000038455070511385375bed94b82f210428 VName: Apple, Inc.  txPow: 0");
    delay(1000);
  }
*/
}


void loop() {
  doBLEScan();
  if(prune_trigger > prune_threshold) {
    pruneDB();
  }
  if(freeheap < min_free_heap) {
    Out.println("Om nom nom nom! There's not Enough heap left because SQLite ate it all! Will restart...");
    ESP.restart();
  }
}
