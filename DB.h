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
const char BACKSLASH = '\\'; // used to clean() slashes
static char *colNeedle = 0; // search criteria
static char colValue[32] = {'\0'}; // search result


// all DB queries
// used by showDataSamples()
#define nameQuery    "SELECT DISTINCT SUBSTR(name,0,32) FROM blemacs where TRIM(name)!=''"
#define manufnameQuery   "SELECT DISTINCT SUBSTR(manufname,0,32) FROM blemacs where TRIM(manufname)!=''"
#define ouinameQuery "SELECT DISTINCT SUBSTR(ouiname,0,32) FROM blemacs where TRIM(ouiname)!=''"
// used by getEntries()
#define BLEMAC_CREATE_FIELDNAMES " \
  appearance INTEGER, \
  name, \
  address, \
  ouiname, \
  rssi INTEGER, \
  manufid INTEGER, \
  manufname, \
  uuid \
"
#define BLEMAC_INSERT_FIELDNAMES " \
  appearance, \
  name, \
  address, \
  ouiname, \
  rssi, \
  manufid, \
  manufname, \
  uuid \
"
#define BLEMAC_SELECT_FIELDNAMES " \
  appearance, \
  name, \
  address, \
  ouiname, \
  rssi, \
  manufid, \
  manufname, \
  uuid \
"
#define allEntriesQuery "SELECT " BLEMAC_SELECT_FIELDNAMES " FROM blemacs;"
#define countEntriesQuery "SELECT count(*) FROM blemacs;"
// used by resetDB()
#define dropTableQuery   "DROP TABLE IF EXISTS blemacs;"
#define createTableQuery "CREATE TABLE IF NOT EXISTS blemacs( " BLEMAC_CREATE_FIELDNAMES " )"
//  created_at timestamp NOT NULL DEFAULT current_timestamp, \
//  updated_at timestamp NOT NULL DEFAULT current_timestamp) \
// used by pruneDB()
#define pruneTableQuery "DELETE FROM blemacs WHERE appearance='' AND name='' AND uuid='' AND ouiname='[private]' AND (manufname LIKE 'Apple%' or manufname='[unknown]')"
// used by testVendorNames()
#define testVendorNamesQuery "SELECT SUBSTR(vendor,0,32)  FROM 'ble-oui' LIMIT 10"
// used by testOUI()
#define testOUIQuery "SELECT * FROM 'oui-light' limit 10"
// used by insertBTDevice()
#define insertQueryTemplate "INSERT INTO blemacs(" BLEMAC_INSERT_FIELDNAMES ") VALUES(%d,\"%s\",\"%s\",\"%s\",%d,%d,\"%s\",\"%s\")"
static char insertQuery[256]; // stack overflow ? pray that 256 is enough :D
#define searchDeviceTemplate "SELECT " BLEMAC_SELECT_FIELDNAMES " FROM blemacs WHERE address='%s'"
static char searchDeviceQuery[132];
//String requestStr = "SELECT appearance, name, address, ouiname, rssi, manufname, uuid FROM blemacs WHERE address='" + bleDeviceAddress + "'";



// used by getVendor()
#ifndef VENDORCACHE_SIZE // override this from Settings.h
#define VENDORCACHE_SIZE 16
#endif
struct VendorCacheStruct {
  int devid = -1;
  char *vendor = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));// = {'\0'};
};
VendorCacheStruct VendorCache[VENDORCACHE_SIZE];
byte VendorCacheIndex = 0; // index in the circular buffer
static int VendorCacheHit = 0;




// used by getOUI()
#ifndef OUICACHE_SIZE // override this from Settings.h
#define OUICACHE_SIZE 32
#endif
struct OUICacheStruct {
  char *mac = (char*)calloc(MAC_LEN+1, sizeof(char));// = {'\0'}; // todo: to char[18]
  char *assignment = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));// = {'\0'}; // todo: to char[32]
};
OUICacheStruct OuiCache[OUICACHE_SIZE];
byte OuiCacheIndex = 0; // index in the circular buffer
static int OuiCacheHit = 0;



class DBUtils {
  public:

    char currentBLEAddress[MAC_LEN+1] = "00:00:00:00:00:00"; // used to proxy BLE search term to DB query

    sqlite3 *BLECollectorDB; // read/write
    sqlite3 *BLEVendorsDB; // readonly
    sqlite3 *OUIVendorsDB; // readonly

    enum DBMessage {
      TABLE_CREATION_FAILED = -1,
      INSERTION_FAILED = -2,
      INCREMENT_FAILED = -3,
      INSERTION_IGNORED = -4,
      DB_IS_OOM = -5,
      INSERTION_SUCCESS = 1,
      INCREMENT_SUCCESS = 2
    };
    
    enum DBName {
      BLE_COLLECTOR_DB = 0,
      MAC_OUI_NAMES_DB = 1,
      BLE_VENDOR_NAMES_DB =2
    };
  
    bool isOOM = false; // for stability
    bool isCorrupt = false; // for maintenance
    byte BLEDevCacheUsed = 0; // for statistics
    byte VendorCacheUsed = 0; // for statistics
    byte OuiCacheUsed = 0; // for statistics

    
    void init() {
      while(SDSetup()==false) {
        UI.headerStats("Card Mount Failed");
        delay(500);
        UI.headerStats(" ");
        delay(300);
      }
      sqlite3_initialize();
      cacheWarmup();
      initial_free_heap = freeheap;
      entries = getEntries();
      if ( resetReason == 12)  { // =  SW_CPU_RESET
        // CPU was reset by software, don't perform tests (faster load)
      } else {
        Out.println();
        Out.println("Cold boot detected");
        Out.println();
        Out.println("Testing Database...");
        Out.println();
        resetDB(); // use this when db is corrupt (shit happens) or filled by ESP32-BLE-BeaconSpam
        pruneDB(); // remove unnecessary/redundant entries
        delay(2000);
        // initial boot, perform some tests
        testOUI(); // test oui database
        testVendorNames(); // test vendornames database
        //showDataSamples(); // print some of the collected values (WARN: memory hungry)
        // let the BLE.init() handle the restart
        //ESP.restart();
      }
    }


    void cacheWarmup() {
      bool hasPsram = psramInit();
      if( hasPsram ) {
        Serial.println("PSRAM OK");
      } else {
        Serial.println("PSRAM FAIL");
      }
      
      for(byte i=0; i<BLEDEVCACHE_SIZE; i++) {
        BLEDevCache[i].init( hasPsram );
        BLEDevTmpCache[i].init( hasPsram );
      }
      for(byte i=0; i<VENDORCACHE_SIZE; i++) {
        memset( VendorCache[i].vendor, 0, MAX_FIELD_LEN+1 );
      }
      for(byte i=0; i<OUICACHE_SIZE; i++) {
        memset( OuiCache[i].mac,        0, MAC_LEN+1 );
        memset( OuiCache[i].assignment, 0, MAX_FIELD_LEN+1 );
      }
    }

    void maintain() {
      if( isOOM ) {
        Serial.println("[DB ERROR] restarting");
        Serial.printf("During this session (%d), %d out of %d devices were added to the DB\n", UpTimeString, newDevicesCount-AnonymousCacheHit, sessDevicesCount);
        delay(1000);
        ESP.restart();
      }
      if( isCorrupt ) {
        Serial.println("[I/O ERROR] this DB file is too big");
        Serial.printf("During this session (%d), %d out of %d devices were added to the DB\n", UpTimeString, newDevicesCount-AnonymousCacheHit, sessDevicesCount);
        moveDB();
        delay(1000);
        ESP.restart();
      }
    }


    static void VendorCacheSet(byte cacheindex, int devid, const char* manufname) {
      VendorCache[cacheindex].devid = devid;
      memset( VendorCache[cacheindex].vendor, '\0', MAX_FIELD_LEN+1);
      memcpy( VendorCache[cacheindex].vendor, manufname, strlen(manufname) );
      //Serial.print("[+] VendorCacheSet: "); Serial.println( manufname );
    }
    static byte getNextVendorCacheIndex() {
      VendorCacheIndex++;
      VendorCacheIndex = VendorCacheIndex % VENDORCACHE_SIZE;  
      return VendorCacheIndex;
    }

    static void OUICacheSet(byte cacheindex, const char* shortmac, const char* assignment) {
      memset( OuiCache[cacheindex].mac, '\0', MAC_LEN+1);
      memcpy( OuiCache[cacheindex].mac, shortmac, strlen(shortmac) );
      memset( OuiCache[cacheindex].assignment, '\0', MAX_FIELD_LEN+1);
      memcpy( OuiCache[cacheindex].assignment, assignment, strlen(assignment) );
      //Serial.print("[+] OUICacheSet: "); Serial.println( assignment );
    }
    static byte getNextOUICacheIndex() {
      OuiCacheIndex++;
      OuiCacheIndex = OuiCacheIndex % OUICACHE_SIZE;  
      return OuiCacheIndex;
    }


    void cacheState() {
      BLEDevCacheUsed = 0;
      for(int i=0;i<BLEDEVCACHE_SIZE;i++) {
        if( !isEmpty( BLEDevCache[i].address ) ) {
          BLEDevCacheUsed++;
        }
      }
      VendorCacheUsed = 0;
      for(int i=0;i<VENDORCACHE_SIZE;i++) {
        if( VendorCache[i].devid > -1 ) {
          VendorCacheUsed++;
        }
      }
      OuiCacheUsed = 0;
      for(int i=0;i<OUICACHE_SIZE;i++) {
        if( !isEmpty( OuiCache[i].assignment ) ) {
          OuiCacheUsed++;
        }
      }
      BLEDevCacheUsed = BLEDevCacheUsed*100 / BLEDEVCACHE_SIZE;
      VendorCacheUsed = VendorCacheUsed*100 / VENDORCACHE_SIZE;
      OuiCacheUsed = OuiCacheUsed*100 / OUICACHE_SIZE;
      //Serial.println("Circular-Buffered Cache Fill: BLEDevCache: "+String(BLEDevCacheUsed)+"%, "+String(VendorCacheUsed)+"%, "+String(OuiCacheUsed)+"%");
    }


    int open(DBName dbName, bool readonly=true) {
     int rc;
      switch(dbName) {
        case BLE_COLLECTOR_DB: // will be created upon first boot
          rc = sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB); 
        break;
        case MAC_OUI_NAMES_DB: // https://code.wireshark.org/review/gitweb?p=wireshark.git;a=blob_plain;f=manuf
          rc = sqlite3_open("/sdcard/mac-oui-light.db", &OUIVendorsDB); 
        break;
        case BLE_VENDOR_NAMES_DB: // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
          rc = sqlite3_open("/sdcard/ble-oui.db", &BLEVendorsDB); 
        break;
        default: Serial.println("Can't open null DB"); UI.dbStateIcon(-1); return rc;
      }
      if (rc) {
        Serial.print("Can't open database "); Serial.println(dbName);
        // SD Card removed ? File corruption ? OOM ?
        // isOOM = true;
        UI.dbStateIcon(-1); // OOM or I/O error
        return rc;
      } else {
        //Serial.println("Opened database successfully");
        if(readonly) {
          UI.dbStateIcon(1); // R/O
        } else {
          UI.dbStateIcon(2); // R+W
        }
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
      cacheState();
      UI.cacheStats(BLEDevCacheUsed, VendorCacheUsed, OuiCacheUsed);
    }
    
    
    // replaces any needle from haystack (defaults to double=>single quotes)
    static void clean(char *haystack, const char needle = '"', const char replacewith='\'') {
      if( isEmpty( haystack ) ) return;
      int len = strlen( (const char*) haystack );
      for( int _i=0;_i<len;_i++ ) {
        if( haystack[_i] == needle ) {
          haystack[_i] = replacewith;
        }
      }
    }

    // checks if a BLE Device exists, returns its cache index if found
    int deviceExists(const char* address) {
      results = 0;
      if( (address && !address[0]) || strlen( address ) > MAC_LEN+1 || strlen( address ) < 17 || address[0]==3) {
        Serial.printf("Cowardly refusing to perform an empty or invalid request : %s / %s\n", address, currentBLEAddress);
        return -1;
      }
      open(BLE_COLLECTOR_DB);
      sprintf(searchDeviceQuery, searchDeviceTemplate, address);
      //Serial.print( address ); Serial.print( " => " ); Serial.println(searchDeviceQuery);
      int rc = sqlite3_exec(BLECollectorDB, searchDeviceQuery, BLEDev_db_callback, (void*)dataBLE, &zErrMsg);
      if (rc != SQLITE_OK) {
        error(zErrMsg);
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
      if(results < BLEDEVCACHE_SIZE) {
        BLEDevCacheIndex = getNextBLEDevCacheIndex(BLEDevCache, BLEDevCacheIndex);
        BLEDevCache[BLEDevCacheIndex].reset(); // avoid mixing new and old data
        for (int i = 0; i < argc; i++) {
          BLEDevCache[BLEDevCacheIndex].set(azColName[i], argv[i] ? argv[i] : '\0');
          //Serial.printf("BLEDev Set cb %s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
        }
        BLEDevCache[BLEDevCacheIndex].hits = 1;
      } else {
        Serial.print("Device Pool Size Exceeded, ignoring: ");
        for (int i = 0; i < argc; i++) {
          Serial.printf("    %s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
        }
      }
      //Serial.println( BLEDevCache[BLEDevCacheIndex].toString() );
      return 0;
    }

    // counts results from a DB query
    static int db_callback(void *data, int argc, char **argv, char **azColName) {
      results++;
      int i;
      //String out = "";
      if (results == 1 && colNeedle == 0 ) {
        if (print_results && print_tabular) {
          //out = "--- ";
          for (i = 0; i < argc; i++) {
            //out += String(azColName[i]) + SPACE;
          }
          //Out.println(out);
          //out = "";
        }
      }
      for (i = 0; i < argc; i++) {
        if (colNeedle != 0) {
          if ( strcmp(colNeedle, azColName[i])==0 ) {
            memset( colValue, 0, MAX_FIELD_LEN );
            if( argv[i] ) {
              memcpy( colValue, argv[i], strlen(argv[i]) );
            }
            //colValue = argv[i] ? argv[i] : "";
          }
          continue;
        }
        if (print_results) {
          if (print_tabular) {
            //out += String(argv[i] ? argv[i] : "NULL") + SPACE;
          } else {
            //Out.println(" " + String(azColName[i]) + "="+ String(argv[i] ? argv[i] : "NULL"));
          }
        }
      }
      if (print_results && colNeedle == 0) {
        //Out.println(" " + out);
      }
      //out = "";
      return 0;
    }


    void error(const char* zErrMsg) {
      Serial.printf("SQL error: %s\n", zErrMsg);
      if (strcmp(zErrMsg, "database disk image is malformed")==0) {
        resetDB();
      } else if (strcmp(zErrMsg, "no such table: blemacs")==0) {
        resetDB();        
      } else if (strcmp(zErrMsg, "out of memory")==0) {
        isOOM = true;
      } else if(strcmp(zErrMsg, "disk I/O error")==0) {
        isCorrupt = true; // TODO: rename the DB file and create a new DB
      } else {
        UI.headerStats(zErrMsg);
        delay(1000); 
      }
    }


    int db_exec(sqlite3 *db, const char *sql, bool _print_results = false, char *_colNeedle = 0) {
      results = 0;
      print_results = _print_results;
      colNeedle = _colNeedle;
      *colValue = {'\0'};
      //Serial.println(sql);
      //long start = micros();
      int rc = sqlite3_exec(db, sql, db_callback, (void*)data, &zErrMsg);
      if (rc != SQLITE_OK) {
        error(zErrMsg);
        sqlite3_free(zErrMsg);
      } else {
        //Serial.printf("Operation done successfully\n");
      }
      //Serial.print(F("Time taken:")); //Serial.println(micros()-start);
      return rc;
    }


    DBMessage insertBTDevice( BlueToothDevice *_BLEDevCache, byte _index) {
      if(isOOM) {
        // cowardly refusing to use DB when OOM
        return DB_IS_OOM;
      }
      if( _BLEDevCache[_index].appearance==0 
       && isEmpty( _BLEDevCache[_index].name )
       && isEmpty( _BLEDevCache[_index].uuid )
       && isEmpty( _BLEDevCache[_index].ouiname )
       && isEmpty( _BLEDevCache[_index].manufname )
       ) {
        // cowardly refusing to insert empty result
        return INSERTION_IGNORED;
      }
      open(BLE_COLLECTOR_DB, false);

      clean( _BLEDevCache[_index].name );
      clean( _BLEDevCache[_index].ouiname );
      clean( _BLEDevCache[_index].manufname );
      clean( _BLEDevCache[_index].uuid );
      
      sprintf(insertQuery, insertQueryTemplate,
        _BLEDevCache[_index].appearance,
        _BLEDevCache[_index].name,
        _BLEDevCache[_index].address,
        _BLEDevCache[_index].ouiname,
        _BLEDevCache[_index].rssi,
        _BLEDevCache[_index].manufid,
        _BLEDevCache[_index].manufname,
        _BLEDevCache[_index].uuid,
        ""//clean(bleDevice.spower).c_str()
      );
      //Serial.println( insertQuery );
      int rc = db_exec( BLECollectorDB, insertQuery );
      if (rc != SQLITE_OK) {
        Serial.print("SQlite Error occured when heap level was at:"); Serial.println(freeheap);
        Serial.println(insertQuery);
        close(BLE_COLLECTOR_DB);
        _BLEDevCache[_index].in_db = false;
        return INSERTION_FAILED;
      }

      close(BLE_COLLECTOR_DB);
      _BLEDevCache[_index].in_db = true;
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


    /* checks for existence in cache */
    int vendorExists(uint16_t devid) {
      // try fast answer first
      for(int i=0;i<VENDORCACHE_SIZE;i++) {
        if( VendorCache[i].devid == devid) {
          VendorCacheHit++;
          return i;
        }
      }
      return -1;
    }


    void getVendor(uint16_t devid, char *dest) {
      int vendorCacheIdIfExists = vendorExists( devid );
      if(vendorCacheIdIfExists>-1) {
        byte vendorCacheLen = strlen( VendorCache[vendorCacheIdIfExists].vendor );
        memcpy( dest, VendorCache[vendorCacheIdIfExists].vendor, vendorCacheLen );
        dest[vendorCacheLen] = '\0';
        return;
      } else {
        *dest = {'\0'};
      }
      byte vendorcacheindex = getNextVendorCacheIndex();
      open(BLE_VENDOR_NAMES_DB);
      const char * vendorRequestTpl = "SELECT vendor FROM 'ble-oui' WHERE id='%d'";
      char vendorRequestStr[64] = {'\0'};
      sprintf(vendorRequestStr, vendorRequestTpl, devid);
      db_exec(BLEVendorsDB, vendorRequestStr, true, (char*)"vendor");
      close(BLE_VENDOR_NAMES_DB);
      uint16_t colValueLen = 10; // sizeof("[unknown]")
      if ( !isEmpty(colValue) ) {
        colValueLen = strlen( colValue );
        if(colValueLen > MAX_FIELD_LEN) {
          colValue[MAX_FIELD_LEN] = '\0';
          colValueLen = MAX_FIELD_LEN+1;
        } else {
          colValue[colValueLen] = '\0';
          colValueLen++;
        }
        VendorCacheSet(vendorcacheindex, devid, colValue);
      } else {
        VendorCacheSet(vendorcacheindex, devid, "[unknown]");
      }
      memcpy( dest, VendorCache[vendorcacheindex].vendor, colValueLen );
    }


    /* checks for existence in cache */
    int OUIExists(const char* mac) {
      // try fast answer first
      for(int i=0;i<OUICACHE_SIZE;i++) {
        if( strcmp(OuiCache[i].mac, mac)==0 ) {
          OuiCacheHit++;
          return i;
        }
      }
      return -1;
    }

    void getOUI(const char* mac, char *dest) {
      *dest = {'\0'};
      char shortmac[7] = {'\0'};
      byte bytepos =  0;
      for(byte i=0;i<9;i++) {
        if(mac[i]!=':') {
          shortmac[bytepos] = mac[i];
          bytepos++;
        }
      }
      if(bytepos!=6) {
        Serial.printf("Bad getOUI query with %d chars instead of %s vs %s\n", bytepos, mac, shortmac);
      }
      int OUICacheIdIfExists = OUIExists( shortmac );
      if(OUICacheIdIfExists>-1) {
        byte OUICacheLen = strlen( OuiCache[OUICacheIdIfExists].assignment );
        memcpy( dest, OuiCache[OUICacheIdIfExists].assignment, OUICacheLen );
        dest[OUICacheLen] = '\0';
        return;
      }
      byte assignmentcacheindex = getNextOUICacheIndex();
      open(MAC_OUI_NAMES_DB);
      const char *OUIRequestTpl = "SELECT * FROM 'oui-light' WHERE Assignment=UPPER('%s');";
      char OUIRequestStr[76];
      sprintf( OUIRequestStr, OUIRequestTpl, shortmac);
      db_exec(OUIVendorsDB, OUIRequestStr, true, (char*)"Organization Name");
      close(MAC_OUI_NAMES_DB);
      uint16_t colValueLen = 10; // sizeof("[private]")
      if ( !isEmpty( colValue ) ) {
        colValueLen = strlen( colValue );
        if(colValueLen > MAX_FIELD_LEN) {
          colValue[MAX_FIELD_LEN] = '\0';
          colValueLen = MAX_FIELD_LEN+1;
        } else {
          colValue[colValueLen] = '\0';
          colValueLen++;
        }
        OUICacheSet( assignmentcacheindex, shortmac, colValue );
      } else {
        OUICacheSet( assignmentcacheindex, shortmac, "[private]" );
      }
      memcpy( dest, OuiCache[assignmentcacheindex].assignment, colValueLen );
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
      db_exec(BLECollectorDB, manufnameQuery, true);
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
        results = atoi(colValue);
      }
      close(BLE_COLLECTOR_DB);
      return results;
    }


    void resetDB() {
      Out.println();
      Out.println("Re-creating database");
      Out.println();
      SD_MMC.remove("/blemacs.db");
      open(BLE_COLLECTOR_DB, false);
      db_exec(BLECollectorDB, dropTableQuery);
      db_exec(BLECollectorDB, createTableQuery);
      close(BLE_COLLECTOR_DB);
      ESP.restart();
    }

    void moveDB() {
      // TODO: give a timestamp to the destination filename
      if(SD_MMC.rename("/blemacs.db", "/blemacs.corrupt.db") !=0) {
        Serial.println("[I/O ERROR] renaming failed, will reset");
        resetDB();
      } else {
        ESP.restart();
      }
    }


    void pruneDB() {
      unsigned int before_pruning = getEntries();
      tft.setTextColor(WROVER_YELLOW);
      UI.headerStats("Pruning DB");
      tft.setTextColor(WROVER_GREEN);
      open(BLE_COLLECTOR_DB, false);
      //db_exec(BLECollectorDB, cleanTableQuery, true);
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
      char *vendorname = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
      getVendor( 0x001D /*Qualcomm*/, vendorname );
      if (strcmp(vendorname, "Qualcomm")!=0) {
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
      char *ouiname = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
      getOUI( "B499BA" /*Hewlett Packard */, ouiname );
      if ( strcmp(ouiname, "Hewlett Packard")!=0 ) {
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
