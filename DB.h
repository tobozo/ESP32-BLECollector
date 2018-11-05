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

  class DBUtils {
    public:
      void init() { };
      void pruneDB() { };
  };
  
#else

const char* data = 0; // for some reason sqlite3 db callback needs this
const char* dataBLE = 0; // for some reason sqlite3 db callback needs this
char *zErrMsg = 0; // holds DB Error message
const char BACKSLASH = '\\'; // used to escape() slashes
char *colNeedle = 0; // search criteria
String colValue = ""; // search result
#define MAX_FIELD_LEN 32 // max chars returned by field

// all DB queries
// used by showDataSamples()
const char *nameQuery    = "SELECT DISTINCT SUBSTR(name,0,32) FROM blemacs where TRIM(name)!=''";
const char *vnameQuery   = "SELECT DISTINCT SUBSTR(vname,0,32) FROM blemacs where TRIM(vname)!=''";
const char *ouinameQuery = "SELECT DISTINCT SUBSTR(ouiname,0,32) FROM blemacs where TRIM(ouiname)!=''";
// used by getEntries()
const char *allEntriesQuery   = "SELECT appearance, name, address, ouiname, rssi, vdata, vname, uuid, spower FROM blemacs;";
const char *countEntriesQuery = "SELECT count(*) FROM blemacs;";
// used by resetDB()
const char *dropTableQuery   = "DROP TABLE IF EXISTS blemacs;";
const char *createTableQuery = "CREATE TABLE IF NOT EXISTS blemacs(id INTEGER, appearance, name, address, ouiname, rssi, vdata, vname, uuid, spower, hits INTEGER, created_at timestamp NOT NULL DEFAULT current_timestamp, updated_at timestamp NOT NULL DEFAULT current_timestamp);";
// used by pruneDB()
const char *pruneTableQuery = "DELETE FROM blemacs WHERE appearance='' AND name='' AND uuid='' AND ouiname='[private]' AND (vname LIKE 'Apple%' or vname='[unknown]')";
// used by testVendorNames()
const char *testVendorNamesQuery = "SELECT SUBSTR(vendor,0,32)  FROM 'ble-oui' LIMIT 10";
// used by testOUI()
const char *testOUIQuery = "SELECT * FROM 'oui-light' limit 10";
// used by insertBTDevice()
const char* insertQueryTemplate = "INSERT INTO blemacs(appearance, name, address, ouiname, rssi, vdata, vname, uuid, spower, hits) VALUES('%s','%s','%s','%s','%s','%s','%s','%s','%s','1')";
static char insertQuery[1024]; // stack overflow ? pray that 1024 is enough :D

// used by getVendor()
#ifndef VENDORCACHE_SIZE // override this from Settings.h
#define VENDORCACHE_SIZE 16
#endif
struct VendorCacheStruct {
  uint16_t devid;
  String vendor;
};
VendorCacheStruct VendorCache[VENDORCACHE_SIZE];
byte VendorCacheIndex = 0; // index in the circular buffer
static int VendorCacheHit = 0;

// used by getOUI()
#ifndef OUICACHE_SIZE // override this from Settings.h
#define OUICACHE_SIZE 32
#endif
struct OUICacheStruct {
  String mac;
  String assignment;
};
OUICacheStruct OuiCache[OUICACHE_SIZE];
byte OuiCacheIndex = 0; // index in the circular buffer
static int OuiCacheHit = 0;

enum DBMessage {
  TABLE_CREATION_FAILED = -1,
  INSERTION_FAILED = -2,
  INCREMENT_FAILED = -3,
  INSERTION_IGNORED = -4,
  DB_IS_OOM = -5,
  INSERTION_SUCCESS = 1,
  INCREMENT_SUCCESS = 2
};

sqlite3 *BLECollectorDB; // read/write
sqlite3 *BLEVendorsDB; // readonly
sqlite3 *OUIVendorsDB; // readonly

enum DBName {
  BLE_COLLECTOR_DB = 0,
  MAC_OUI_NAMES_DB = 1,
  BLE_VENDOR_NAMES_DB =2
};


class DBUtils {
  public:
  
    bool isOOM = false;
    
    void init() {
      while(SDSetup()==false) {
        UI.headerStats("Card Mount Failed");
        delay(500);
        UI.headerStats(" ");
        delay(300);
      }
      sqlite3_initialize();
      initial_free_heap = freeheap;
      entries = getEntries();
      //resetDB(); // use this when db is corrupt (shit happens)
      if ( resetReason == 12)  { // =  SW_CPU_RESET
        // CPU was reset by software, don't perform tests (faster load)
      } else {
        Out.println();
        Out.println("Cold boot detected");
        Out.println();
        Out.println("Testing Database...");
        Out.println();
        pruneDB(); // remove unnecessary/redundant entries
        delay(2000);
        // initial boot, perform some tests
        testOUI(); // test oui database
        testVendorNames(); // test vendornames database
        showDataSamples(); // print some of the collected values (WARN: memory hungry)
        // let the BLE.init() handle the restart
        //ESP.restart();
      }
    }

    void maintain() {
      if (prune_trigger > prune_threshold) {
        pruneDB();
      }
    }


    int open(DBName dbName) {
     int rc;
      switch(dbName) {
        case BLE_COLLECTOR_DB:    rc = sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB); break;// will be created upon first boot
        case MAC_OUI_NAMES_DB:    rc = sqlite3_open("/sdcard/mac-oui-light.db", &OUIVendorsDB); break;// https://code.wireshark.org/review/gitweb?p=wireshark.git;a=blob_plain;f=manuf
        case BLE_VENDOR_NAMES_DB: rc = sqlite3_open("/sdcard/ble-oui.db", &BLEVendorsDB); break;// https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
        default: Serial.println("Can't open null DB"); UI.dbStateIcon(-1); return rc;
      }
      if (rc) {
        Serial.println("Can't open database " + String(dbName));
        // SD Card removed ? File corruption ? OOM ?
        // isOOM = true;
        UI.dbStateIcon(-1);
        return rc;
      } else {
        //Serial.println("Opened database successfully");
        UI.dbStateIcon(1);
      }
      return rc;
    }


    void close(DBName dbName) {
      UI.dbStateIcon(0);
      switch(dbName) {
        case BLE_COLLECTOR_DB:    sqlite3_close(BLECollectorDB); break;
        case MAC_OUI_NAMES_DB:    sqlite3_close(OUIVendorsDB); break;
        case BLE_VENDOR_NAMES_DB: sqlite3_close(BLEVendorsDB); break;
        default: /* duh ! */ Serial.println("Can't open null DB");
      }
    }
    
    
    // adds a backslash before needle (defaults to single quotes)
    String escape(String haystack, String needle = "'") {
      haystack.replace(String(BACKSLASH), String(BACKSLASH) + String(BACKSLASH)); // escape existing backslashes
      haystack.replace(String(needle), String(BACKSLASH) + String(needle)); // slash needle
      return haystack;
    }

    // checks if a BLE Device exists, returns its cache index if found
    int deviceExists(String bleDeviceAddress) {
      results = 0;
      open(BLE_COLLECTOR_DB);
      String requestStr = "SELECT appearance, name, address, ouiname, rssi, vname, uuid FROM blemacs WHERE address='" + bleDeviceAddress + "'";
      int rc = sqlite3_exec(BLECollectorDB, requestStr.c_str(), BLEDev_db_callback, (void*)dataBLE, &zErrMsg);
      if (rc != SQLITE_OK) {
        error(String(zErrMsg));
        //Out.printf("SQL error: %s\n", msg);
        //Out.println("SQL error:"+String(zErrMsg));
        sqlite3_free(zErrMsg);
        close(BLE_COLLECTOR_DB);
        return -2;
      }
      close(BLE_COLLECTOR_DB);
      // if the device exists, it's been loaded into BLEDevCache[BLEDevCacheIndex]
      return results>0 ? BLEDevCacheIndex : -1;
    }
    
    // loads a DB entry into a BLEDevice struct
    static int BLEDev_db_callback(void *dataBLE, int argc, char **argv, char **azColName) {
      //Serial.println("BLEDev_db_callback");
      results++;
      BLEDevCacheIndex++;
      BLEDevCacheIndex = BLEDevCacheIndex % BLEDEVCACHE_SIZE;
      BLEDevCache[BLEDevCacheIndex].reset(); // avoid mixing new and old data
      if(results < BLEDEVCACHE_SIZE) {
        int i;
        for (i = 0; i < argc; i++) {
          BLEDevCache[BLEDevCacheIndex].set(String(azColName[i]), String(argv[i] ? argv[i] : ""));
          //Serial.printf("BLEDev Set cb %s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
        }
      } else {
        Serial.println("Device Pool Size Exceeded, ignoring");
      }
      //Serial.println( BLEDevCache[BLEDevCacheIndex].toString() );
      return 0;
    }

    // counts or prints results from a DB query
    static int db_callback(void *data, int argc, char **argv, char **azColName) {
      results++;
      int i;
      String out = "";
      if (results == 1 && colNeedle == 0 ) {
        if (print_results && print_tabular) {
          out = "--- ";
          for (i = 0; i < argc; i++) {
            out += String(azColName[i]) + SPACE;
          }
          Out.println(out);
          out = "";
        }
      }
      for (i = 0; i < argc; i++) {
        if (colNeedle != 0) {
          if (String(colNeedle) == String(azColName[i])) {
            colValue = argv[i] ? argv[i] : "";
          }
          continue;
        }
        if (print_results) {
          if (print_tabular) {
            out += String(argv[i] ? argv[i] : "NULL") + SPACE;
          } else {
            Out.println(" " + String(azColName[i]) + "="+ String(argv[i] ? argv[i] : "NULL"));
          }
        }
      }
      if (print_results && colNeedle == 0) {
        Out.println(" " + out);
      }
      out = "";
      return 0;
    }


    void error(String zErrMsg) {
      Serial.println("SQL error: "+zErrMsg);
      if (zErrMsg == "database disk image is malformed") {
        resetDB();
      } else if (zErrMsg == "out of memory") {
        isOOM = true;
      } else {
        UI.headerStats(zErrMsg);
        delay(1000); 
      }
    }


    int db_exec(sqlite3 *db, const char *sql, bool _print_results = false, char *_colNeedle = 0) {
      results = 0;
      print_results = _print_results;
      colNeedle = _colNeedle;
      colValue = "";
      //Serial.println(sql);
      //long start = micros();
      int rc = sqlite3_exec(db, sql, db_callback, (void*)data, &zErrMsg);
      if (rc != SQLITE_OK) {
        error(String(zErrMsg));
        sqlite3_free(zErrMsg);
      } else {
        //Serial.printf("Operation done successfully\n");
      }
      //Serial.print(F("Time taken:")); //Serial.println(micros()-start);
      return rc;
    }


    DBMessage insertBTDevice(byte cacheindex) {
      if(isOOM) {
        // cowardly refusing to use DB when OOM
        return DB_IS_OOM;
      }
      if( BLEDevCache[cacheindex].appearance=="" 
       && BLEDevCache[cacheindex].name=="" 
       //&& bleDevice.spower==""
       && BLEDevCache[cacheindex].uuid=="" 
       && BLEDevCache[cacheindex].ouiname==""
       && BLEDevCache[cacheindex].vname==""
       ) {
        // cowardly refusing to insert empty result
        return INSERTION_IGNORED;
      }
      open(BLE_COLLECTOR_DB);
      sprintf(insertQuery, insertQueryTemplate,
        escape(BLEDevCache[cacheindex].appearance).c_str(),
        escape(BLEDevCache[cacheindex].name).c_str(),
        escape(BLEDevCache[cacheindex].address).c_str(),
        escape(BLEDevCache[cacheindex].ouiname).c_str(),
        escape(BLEDevCache[cacheindex].rssi).c_str(),
        escape(BLEDevCache[cacheindex].vdata).c_str(),
        escape(BLEDevCache[cacheindex].vname).c_str(),
        escape(BLEDevCache[cacheindex].uuid).c_str(),
        ""//escape(bleDevice.spower).c_str()
      );
      
      int rc = db_exec(BLECollectorDB, insertQuery);
      if (rc != SQLITE_OK) {
        Serial.println("SQlite Error occured when heap level was at:" + String(freeheap));
        Serial.println(insertQuery);
        close(BLE_COLLECTOR_DB);
        return INSERTION_FAILED;
      }
      //requestStr = "";
      close(BLE_COLLECTOR_DB);
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


    String getVendor(uint16_t devid) {
      // try fast answer first
      for(int i=0;i<VENDORCACHE_SIZE;i++) {
        if( VendorCache[i].devid == devid) {
          VendorCacheHit++;
          return VendorCache[i].vendor;
        }
      }
      VendorCacheIndex++;
      VendorCacheIndex = VendorCacheIndex % VENDORCACHE_SIZE;
      VendorCache[VendorCacheIndex].devid = devid;
      open(BLE_VENDOR_NAMES_DB);
      String requestStr = "SELECT vendor FROM 'ble-oui' WHERE id='" + String(devid) + "'";
      db_exec(BLEVendorsDB, requestStr.c_str(), true, (char*)"vendor");
      requestStr = "";
      close(BLE_VENDOR_NAMES_DB);
      if (colValue != "" && colValue != "NULL") {
        if (colValue.length() > MAX_FIELD_LEN) {
          colValue = colValue.substring(0, MAX_FIELD_LEN);
        }
        VendorCache[VendorCacheIndex].vendor = colValue;
      } else {
        VendorCache[VendorCacheIndex].vendor = "[unknown]";
      }
      return VendorCache[VendorCacheIndex].vendor ;
    }


    String getOUI(String mac) {
      mac.replace(":", "");
      mac.toUpperCase();
      mac = mac.substring(0, 6);
      // try fast answer first
      for(int i=0;i<OUICACHE_SIZE;i++) {
        if( OuiCache[i].mac == mac) {
          OuiCacheHit++;
          return OuiCache[i].assignment;
        }
      }
      OuiCacheIndex++;
      OuiCacheIndex = OuiCacheIndex % OUICACHE_SIZE;
      OuiCache[OuiCacheIndex].mac = mac;
      open(MAC_OUI_NAMES_DB);
      String requestStr = "SELECT * FROM 'oui-light' WHERE Assignment ='" + mac + "';";
      db_exec(OUIVendorsDB, requestStr.c_str(), true, (char*)"Organization Name");
      requestStr = "";
      close(MAC_OUI_NAMES_DB);
      if (colValue != "" && colValue != "NULL") {
        if (colValue.length() > MAX_FIELD_LEN) {
          colValue = colValue.substring(0, MAX_FIELD_LEN);
        }
        OuiCache[OuiCacheIndex].assignment = colValue;
      } else {
        OuiCache[OuiCacheIndex].assignment = "[private]";
      }
      return OuiCache[OuiCacheIndex].assignment;
    }


    void showDataSamples() {
      open(BLE_COLLECTOR_DB);
      tft.setTextColor(WROVER_YELLOW);
      Out.println(" Collected Named Devices:");
      tft.setTextColor(WROVER_PINK);
      db_exec(BLECollectorDB, nameQuery , true);
      tft.setTextColor(WROVER_YELLOW);
      Out.println(" Collected Devices Vendors:");
      tft.setTextColor(WROVER_PINK);
      db_exec(BLECollectorDB, vnameQuery, true);
      tft.setTextColor(WROVER_YELLOW);
      Out.println(" Collected Devices MAC's Vendors:");
      tft.setTextColor(WROVER_PINK);
      db_exec(BLECollectorDB, ouinameQuery, true);
      close(BLE_COLLECTOR_DB);
      Out.println();
    }


    unsigned int getEntries(bool _display_results = false) {
      open(BLE_COLLECTOR_DB);
      if (_display_results) {
        db_exec(BLECollectorDB, allEntriesQuery, true);
      } else {
        db_exec(BLECollectorDB, countEntriesQuery, true, (char*)"count(*)");
        results = atoi(colValue.c_str());
      }
      close(BLE_COLLECTOR_DB);
      return results;
    }


    void resetDB() {
      Out.println();
      Out.println("Re-creating database");
      Out.println();
      SD_MMC.remove("/blemacs.db");
      open(BLE_COLLECTOR_DB);
      db_exec(BLECollectorDB, dropTableQuery);
      db_exec(BLECollectorDB, createTableQuery);
      close(BLE_COLLECTOR_DB);
      ESP.restart();
    }


    void pruneDB() {
      unsigned int before_pruning = getEntries();
      tft.setTextColor(WROVER_YELLOW);
      UI.headerStats("Pruning DB");
      tft.setTextColor(WROVER_GREEN);
      open(BLE_COLLECTOR_DB);
      db_exec(BLECollectorDB, pruneTableQuery, true);
      close(BLE_COLLECTOR_DB);
      entries = getEntries();
      tft.setTextColor(WROVER_YELLOW);
      prune_trigger = 0;
      UI.headerStats("DB Pruned");
      UI.footerStats();
    }


    void testVendorNames() {
      open(BLE_VENDOR_NAMES_DB);
      tft.setTextColor(WROVER_YELLOW);
      Out.println();
      Out.println("Testing Vendor Names Database ...");
      tft.setTextColor(WROVER_GREENYELLOW);
      db_exec(BLEVendorsDB, testVendorNamesQuery, true);
      close(BLE_VENDOR_NAMES_DB);
      // 0x001D = Qualcomm
      String vendorname = getVendor(0x001D);
      if (vendorname != "Qualcomm") {
        tft.setTextColor(WROVER_RED);
        Out.println(vendorname);
        Out.println("Vendor Names Test failed, looping");
        while (1) yield();
      } else {
        tft.setTextColor(WROVER_GREEN);
        Out.println("Vendor Names Test success!");
      }
      Out.println();
    }


    void testOUI() {
      open(MAC_OUI_NAMES_DB);
      tft.setTextColor(WROVER_YELLOW);
      Out.println();
      Out.println("Testing MAC OUI database ...");
      tft.setTextColor(WROVER_GREENYELLOW);
      db_exec(OUIVendorsDB, testOUIQuery, true);
      close(MAC_OUI_NAMES_DB);
      String ouiname = getOUI("B499BA" /*Hewlett Packard */);
      if (ouiname != "Hewlett Packard") {
        tft.setTextColor(WROVER_RED);
        Out.println(ouiname);
        Out.println("MAC OUI Test failed, looping");
        while (1) yield();
      } else {
        tft.setTextColor(WROVER_GREEN);
        Out.println("MAC OUI Test success!");
      }
      Out.println();
    }

};

#endif


DBUtils DB;
