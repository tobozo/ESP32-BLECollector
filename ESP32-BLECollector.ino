
#define SCAN_TIME  30 // seconds

#include <Arduino.h>
#include <sstream>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h> // https://github.com/siara-cc/esp32_arduino_sqlite3_lib
#include <SPI.h>
#include <FS.h>
#include "SD.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define freeheap heap_caps_get_free_size(MALLOC_CAP_INTERNAL)

std::stringstream ss;
bool data_sent = false;
//unsigned long freeheap;

sqlite3 *BLECollectorDB; // read/write
sqlite3 *BLEVendorsDB; // readonly
sqlite3 *OUIVendorsDB; // readonly

enum DBMessages {
 TABLE_CREATION_FAILED = -1,
 INSERTION_FAILED = -2,
 INCREMENT_FAILED = -3,
 INSERTION_SUCCESS = 1,
 INCREMENT_SUCCESS = 2
};


struct BlueToothDevice {
  int id;
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

int results = 0;
bool print_results = false;
bool print_tabular = true;
char *colNeedle = 0;
String colValue = "";
uint32_t min_free_heap = 105000; // sql out of memory errors occur under 100000


int StrToHex(char str[]) {
  return (int) strtol(str, 0, 16);
}

const char* data = "cb";
static int callback(void *data, int argc, char **argv, char **azColName){
  results++;
  if(!print_results) return 0;
  if(colNeedle==0) Serial.printf("%s: ", (const char*)data);
  int i;
  if(results==1 && colNeedle==0 ) {
    if(print_tabular) {
      for (i = 0; i<argc; i++){
        Serial.printf("%s \t", azColName[i]);
      }
      Serial.printf("\n");
    }
  }
  for (i = 0; i<argc; i++){
    if(colNeedle!=0) {
      if(String(colNeedle)==String(azColName[i])) {
        colValue = argv[i] ? argv[i] : "";
      }
      continue;
    }
    if(print_tabular) {
      Serial.printf("%s\t", argv[i] ? argv[i] : "NULL");      
    } else {
      Serial.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
  }
  if(colNeedle==0) Serial.printf("\n");
  return 0;
}


int openDb(const char *filename, sqlite3 **db) {
  int rc = sqlite3_open(filename, db);
  if (rc) {
    Serial.printf("Can't open database: %s\n", sqlite3_errmsg(*db));
  } else {
    Serial.printf("Opened database successfully\n");
  }
  return rc;
}


char *zErrMsg = 0;
int db_exec(sqlite3 *db, const char *sql) {
  //Serial.println(sql);
  long start = micros();
  results = 0;
  colValue = "";
  int rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
  colNeedle = 0;
  if (rc != SQLITE_OK) {
    Serial.printf("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    //Serial.printf("Operation done successfully\n");
  }
  //Serial.print(F("Time taken:")); //Serial.println(micros()-start);
  return rc;
}


String getVendor(uint16_t devid) {
  sqlite3_open("/sd/ble-oui.db", &BLEVendorsDB); // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
  String requestStr = "SELECT vendor FROM 'ble-oui' WHERE id='"+ String(devid) +"'";
  colNeedle = "vendor";
  print_results = true;
  int rc = db_exec(BLEVendorsDB, requestStr.c_str());
  print_results = false;
  requestStr = "";
  sqlite3_close(BLEVendorsDB);
  if(colValue!="" && colValue!="NULL") {
    return colValue;
  }
  return "[unknown]";
}


void testOUI() {
  sqlite3_open("/sd/mac-oui-light.db", &OUIVendorsDB); // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
  String requestStr = "SELECT * FROM 'oui-light' limit 10";
  print_results = true;
  colNeedle = 0;
  int rc = db_exec(OUIVendorsDB, requestStr.c_str());
  sqlite3_close(OUIVendorsDB);
  String ouiname = getOUI("B499BA" /*Hewlett Packard */);
  if(ouiname!="Hewlett Packard") {
    Serial.println(ouiname);
    Serial.print("OUI Test failed, looping");
    while(1) yield();
  } else {
    Serial.print("OUI Test success!");
  }
  
}


String getOUI(String mac) {
  sqlite3_open("/sd/mac-oui-light.db", &OUIVendorsDB); // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
  mac.replace(":", "");
  mac.toUpperCase();
  mac = mac.substring(0, 6);

  String requestStr = "SELECT * FROM 'oui-light' WHERE Assignment ='"+ mac +"';";
  //String requestStr = "SELECT * FROM 'oui-light' limit 10";
  //Serial.println(requestStr);
  colNeedle = "Organization Name";
  print_results = true;
  int rc = db_exec(OUIVendorsDB, requestStr.c_str());
  //Serial.println("Found " + String(results) + " results : " + colValue);
  print_results = false;
  requestStr = "";
  //while(1) yield();
  sqlite3_close(OUIVendorsDB);
  if(colValue!="" && colValue!="NULL") {
    return colValue;
  }
  return "[private]";
}


void rebuildOUI() {
  sqlite3_open("/sd/blemacs.db", &BLECollectorDB);
  int rc;

  // rebuild ouiname (after purge)
  int rescount = 0;
  bool nextrecord = true;
  String address;
  String addressStr;
  String ouiname;
  int data[6];
  uint8_t mac[5];

  do {
    print_results = true;
    colNeedle = "address";
    colValue = "";
    rc = db_exec(BLECollectorDB, "SELECT address FROM blemacs where address!='' and ouiname is null or ouiname='' LIMIT 1;");
    rescount = results;
    //Serial.println(colValue);
    if(rc == SQLITE_OK && colValue!="" && results > 0 ) {

      addressStr = colValue + "";
      ouiname = "";
      
      String macvendor = getOUI( addressStr );
      if( macvendor !="" ) {
        ouiname = macvendor;
      } else {
        ouiname = "[private]";
      }

      String requestStr = "UPDATE blemacs SET ouiname='"+ ouiname +"' WHERE address like'"+ addressStr.substring(0,8) +"%';";
      Serial.println("[" + String( freeheap ) + "]" + requestStr+" / "+addressStr);
      print_results = false;
      colNeedle = 0;
      rc = db_exec(BLECollectorDB, requestStr.c_str());

      if (rc != SQLITE_OK || freeheap < min_free_heap) {
        sqlite3_close(BLECollectorDB);
        ESP.restart();        
      }

      if(random(0,10)>8) {
        rc = db_exec(BLECollectorDB, "SELECT address FROM blemacs where address!='' and ouiname is null or ouiname='';");
        Serial.println("****************************** " + String(results) + " entries left");
      }
      
      nextrecord = true;
    } else {
      rescount = 0;
      nextrecord = false;
    }
  } while(nextrecord == true);
  sqlite3_close(BLECollectorDB);

}


int insertOrUpdate(BlueToothDevice bleDevice) {
  sqlite3_open("/sd/blemacs.db", &BLECollectorDB);
  String requestStr = "SELECT * FROM blemacs WHERE address='"+ bleDevice.address +"'";
  int rc = db_exec(BLECollectorDB, requestStr.c_str());
  requestStr = "";
  if (rc != SQLITE_OK) {
    // SQL failed
    Serial.println("[WARN} SQL FAILED, creating table");
    rc = db_exec(BLECollectorDB, "CREATE TABLE blemacs(id INTEGER, appearance, name, address, ouiname, rssi, vdata, vname, uuid, spower, hits INTEGER);");
    if (rc != SQLITE_OK) {
      Serial.println("TABLE CREATION FAILED, creating table");
      sqlite3_close(BLECollectorDB);
      return TABLE_CREATION_FAILED;
    }
  }
  //delete request;
  if(results==0) {
    String requestStr  = "INSERT INTO blemacs(appearance, name, address, ouiname, rssi, vdata, vname, uuid, spower, hits) VALUES(";
    requestStr += "'"+ bleDevice.appearance  +"', ";
    requestStr += "'"+ bleDevice.name  +"', ";
    requestStr += "'"+ bleDevice.address  +"', ";
    requestStr += "'"+ bleDevice.ouiname  +"', ";
    requestStr += "'"+ bleDevice.rssi  +"', ";
    requestStr += "'"+ bleDevice.vdata  +"', ";
    requestStr += "'"+ bleDevice.vname  +"', ";
    requestStr += "'"+ bleDevice.uuid  +"', ";
    requestStr += "'"+ bleDevice.spower  +"', ";
    requestStr += "1";
    requestStr += ")";
    rc = db_exec(BLECollectorDB, requestStr.c_str());
    requestStr = "";
    if (rc != SQLITE_OK) {
      Serial.println("[ERROR] INSERTION FAILED");
      // SQL failed
      sqlite3_close(BLECollectorDB);
      return INSERTION_FAILED;
    }
    sqlite3_close(BLECollectorDB);
    return INSERTION_SUCCESS;
  } else {
    /*
    //Serial.println("Found " + String(results) + " results, incrementing");
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


class FoundDeviceCallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      BlueToothDevice BLEDev;
      BLEDev.spower = String( (int)advertisedDevice.getTXPower() );
      //const char *blbl = String( advertisedDevice.getAddress().toString().c_str() );
      //std::string address = advertisedDevice.getAddress().toString();
      //const char *addrptr = address.c_str();
      BLEDev.address = String( advertisedDevice.getAddress().toString().c_str() );

      //BLEDev.ouiname = getOUI( BLEDev.address );

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
        //Serial.println(String(vint) + " / " + String ( pHex ));
      }
      if (advertisedDevice.haveServiceUUID()) {
        BLEDev.uuid = String( advertisedDevice.getServiceUUID().toString().c_str() );
      }
      int ret = insertOrUpdate( BLEDev );
      switch(ret) {
        case INSERTION_SUCCESS:
          Serial.printf("[+][%d] %s\n", freeheap, BLEDev.toString().c_str());
          //Serial.printf("[+][%d] %s\n", freeheap, advertisedDevice.toString().c_str());
        break;
        case INCREMENT_SUCCESS:
          //Serial.println("Free heap : " + String(freeheap));
          //Serial.printf("[-][%d] %s\n", freeheap, advertisedDevice.toString().c_str());
        break;
        case INCREMENT_FAILED:
        case INSERTION_FAILED:
          // out of memory ?
          Serial.println("DB Error, will restart, just in case ...");
          ESP.restart();
        break;
      }
    }
};


int doBLEScan() {
  BLEScan *pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new FoundDeviceCallback());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(0x50); // 0x50
  pBLEScan->setWindow(0x30); // 0x30
  //Serial.printf("Start BLE scan for %d seconds...\n", SCAN_TIME);
  BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME);
  int count = foundDevices.getCount();
  // BLEAdvertisedDevice d = foundDevices.getDevice(i);
  return count;
}


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(115200);
  //delay(300);
  Serial.println("ESP32 BLE Scanner");
  Serial.println("Free heap at boot: " + String(freeheap));
  // sorry this won't work with SPIFFS, too much ram is eaten by BLE functions :-(
  SPI.begin(14, 2, 15, 13);
  //delay(700);
  if(!SD.begin( 13 )){
    Serial.println("Card Mount Failed, will restart");
    delay(1000);
    ESP.restart();
  }

  //SD.remove("/blemacs.db");
  
  sqlite3_initialize();
  int rc;
  
/*
  // add "ouiname" column
  rc = db_exec(BLECollectorDB, "SELECT ouiname from blemacs;");
  if (rc != SQLITE_OK) {
    Serial.println("Table haz no ouiname field, adding");
    rc = db_exec(BLECollectorDB, "ALTER TABLE blemacs add ouiname;");
    if (rc != SQLITE_OK) {
      Serial.println("ouiname field added successfully");
      sqlite3_close(BLECollectorDB);
      delay(1000);
      ESP.restart();    
    } else {
      Serial.println("ouiname field addition failed!");
      while(1) {
        yield();
      }
    }
  } else {
    Serial.println("ouiname field already there");
  }
*/

/*
  // purge all vnames
  db_exec(BLECollectorDB, "UPDATE blemacs set vname=null WHERE vdata!=''");
  Serial.println("vendor names reset !");
  sqlite3_close(BLECollectorDB);
  while(2) {
    yield();
  }
*/



  //rc = db_exec(BLECollectorDB, "UPDATE blemacs set ouiname='' where ouiname!='';");
  //Serial.println("purged oui names");
  //while(1) yield();

  testOUI();
  rebuildOUI();
  
  sqlite3_open("/sd/blemacs.db", &BLECollectorDB);
  print_results = false;
  rc = db_exec(BLECollectorDB, "SELECT appearance, name, address, ouiname, rssi, vdata, vname, uuid, spower FROM blemacs;");
  if (rc != SQLITE_OK) {
    Serial.println("wut?");
  }
  Serial.println("DB haz " + String(results) + " entries");
  
  print_results = true;
  rc = db_exec(BLECollectorDB, "SELECT DISTINCT name, vname, ouiname FROM blemacs where name!='';");
  print_results = true;
  rc = db_exec(BLECollectorDB, "SELECT DISTINCT vname FROM blemacs;");
  print_results = true;
  rc = db_exec(BLECollectorDB, "SELECT DISTINCT ouiname FROM blemacs;");
  if (rc != SQLITE_OK) {
    Serial.println("wut?");
  }
  print_results = false;
  sqlite3_close(BLECollectorDB);

  BLEDevice::init("");
  Serial.println("Free heap after BLE/Sqlite init: " + String(freeheap));
  Serial.println("Starting Scan!");

}

void loop() {
  doBLEScan();
  rebuildOUI();
  if(freeheap < min_free_heap) {
    Serial.println("Om nom nom nom! There's not Enough heap left because SQLite ate it all! Will restart...");
    ESP.restart();
  }
}
