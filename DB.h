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
const char* dataOUI = 0; // for some reason sqlite3 db callback needs this
const char* dataVendor = 0; 
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
#define vendorRequestTpl "SELECT vendor FROM 'ble-oui' WHERE id='%d'"
#define OUIRequestTpl "SELECT * FROM 'oui-light' WHERE Assignment=UPPER('%s');"


// used by getVendor()
#ifndef VENDORCACHE_SIZE // override this from Settings.h
#define VENDORCACHE_SIZE 16
#endif

struct VendorHeapCacheStruct {
  int devid = -1;
  char *vendor = NULL;
  void init( bool hasPsram=false ) {
    if( hasPsram ) {  
      vendor = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
    } else {
      vendor = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
    }
  }
};

VendorHeapCacheStruct VendorHeapCache[VENDORCACHE_SIZE];
uint16_t VendorCacheIndex = 0; // index in the circular buffer
static int VendorCacheHit = 0;


struct VendorPsramCacheStruct {
  uint16_t *devid = NULL;
  uint16_t hits   = 0; // cache hits
  char *vendor = NULL;
};

/*
static void VendorPsramCacheStructInit( VendorPsramCacheStruct *CacheItem, bool hasPsram=true ) {
  if( hasPsram ) {
    CacheItem = (VendorPsramCacheStruct*)ps_calloc(1, sizeof( VendorPsramCacheStruct ));
    if( CacheItem == NULL ) {
      Serial.printf("[ERROR][%d][%d] can't allocate\n", freeheap, freepsheap);
      return;
    }
    CacheItem->devid  = (uint16_t*)ps_malloc(sizeof(uint16_t));
    CacheItem->vendor = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
  } else {
    CacheItem = (VendorPsramCacheStruct*)calloc(1, sizeof( VendorPsramCacheStruct ));
    if( CacheItem == NULL ) {
      Serial.printf("[ERROR][%d][%d] can't allocate\n", freeheap, freepsheap);
      return;
    }
    CacheItem->devid  = (uint16_t*)malloc(sizeof(uint16_t));
    CacheItem->vendor = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
  }
}
*/

#define VendorDBSize 1740 // how many entries in the OUI lookup DB
VendorPsramCacheStruct** VendorPsramCache = NULL;

// used by getOUI()
#ifndef OUICACHE_SIZE // override this from Settings.h
#define OUICACHE_SIZE 32
#endif


struct OUIHeapCacheStruct {
  char *mac = NULL;
  char *assignment = NULL;
  void init( bool hasPsram=false ) {
    if( hasPsram ) {  
      mac =        (char*)ps_calloc(SHORT_MAC_LEN+1, sizeof(char));
      assignment = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
    } else {
      mac =        (char*)calloc(SHORT_MAC_LEN+1, sizeof(char));
      assignment = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
    }
  }
  void setMac( const char* _mac ) {
    copy( mac, _mac, SHORT_MAC_LEN );
    
  }
  void setAssignment( const char* _assignment ) {
    copy( assignment, _assignment, MAX_FIELD_LEN );
  }
};

OUIHeapCacheStruct OuiHeapCache[OUICACHE_SIZE];
uint16_t OuiCacheIndex = 0; // index in the circular buffer
static int OuiCacheHit = 0;

struct OUIPsramCacheStruct {
  char *mac = NULL;
  uint16_t hits    = 0; // cache hits
  char *assignment = NULL;
};

#define OUIDBSize 25523 // how many entries in the OUI lookup DB
OUIPsramCacheStruct** OuiPsramCache = NULL;


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
    bool hasPsram = false;
    
    void init() {
      while(SDSetup()==false) {
        UI.headerStats("Card Mount Failed");
        delay(500);
        UI.headerStats(" ");
        delay(300);
      }
      hasPsram = psramInit();
      sqlite3_initialize();
      initial_free_heap = freeheap;
      if ( resetReason == 12)  { // =  SW_CPU_RESET
        // CPU was reset by software, don't perform tests (faster load)
        cacheWarmup();
      } else {
        Out.println();
        Out.println("Cold boot detected");
        Out.println();
        Out.println("Testing Database...");
        Out.println();
        resetDB(); // use this when db is corrupt (shit happens) or filled by ESP32-BLE-BeaconSpam
        pruneDB(); // remove unnecessary/redundant entries
        // initial boot, perform some tests
        testOUI(); // test oui database
        testVendorNames(); // test vendornames database
        //showDataSamples(); // print some of the collected values (WARN: memory hungry)
        // note: BLE.init() will restart on cold boot
      }
      entries = getEntries();
    }


    void cacheWarmup() {

      if( hasPsram ) {
        BLEDEVCACHE_SIZE = BLEDEVCACHE_PSRAM_SIZE;
        Serial.println("[PSRAM] OK");

        OuiPsramCache = (OUIPsramCacheStruct**)ps_calloc(OUIDBSize, sizeof( OUIPsramCacheStruct ) );
        for(int i=0; i<OUIDBSize; i++) {
          OuiPsramCache[i] = (OUIPsramCacheStruct*)ps_calloc(1, sizeof( OUIPsramCacheStruct ));
          if( OuiPsramCache[i] == NULL ) {
            Serial.printf("[ERROR][%d][%d][%d] can't allocate\n", i, freeheap, freepsheap);
            continue;
          }
          OuiPsramCache[i]->mac        = (char*)ps_calloc(SHORT_MAC_LEN+1, sizeof(char));
          OuiPsramCache[i]->assignment = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
        }
        loadOUIToPSRam();

        VendorPsramCache = (VendorPsramCacheStruct**)ps_calloc(VendorDBSize, sizeof( VendorPsramCacheStruct ) );
        for(int i=0; i<VendorDBSize; i++) {
          VendorPsramCache[i] = (VendorPsramCacheStruct*)ps_calloc(1, sizeof( VendorPsramCacheStruct ));
          if( VendorPsramCache[i] == NULL ) {
            Serial.printf("[ERROR][%d][%d][%d] can't allocate\n", i, freeheap, freepsheap);
            continue;
          }
          VendorPsramCache[i]->devid  = (uint16_t*)ps_malloc(sizeof(uint16_t));
          VendorPsramCache[i]->vendor = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
        }
        loadVendorsToPSRam();

        BLEDevCache = (BlueToothDevice**)ps_calloc(BLEDEVCACHE_SIZE, sizeof( BlueToothDevice ) );
        for(uint16_t i=0; i<BLEDEVCACHE_SIZE; i++) {
          BLEDevCache[i] = (BlueToothDevice*)ps_calloc(1, sizeof( BlueToothDevice ) );
          if( BLEDevCache[i] == NULL ) {
            Serial.printf("[ERROR][%d][%d][%d] can't allocate\n", i, freeheap, freepsheap);
            continue;
          }
          BLEDevHelper.init( BLEDevCache[i] );
          delay(1);
        }
        // TODO: loadBLEDevToPSRam()

        BLEDevTmpCache = (BlueToothDevice**)ps_calloc(MAX_DEVICES_PER_SCAN, sizeof( BlueToothDevice ) );
        for(uint16_t i=0; i<MAX_DEVICES_PER_SCAN; i++) {
          BLEDevTmpCache[i] = (BlueToothDevice*)ps_calloc(1, sizeof( BlueToothDevice ) );
          if( BLEDevTmpCache[i] == NULL ) {
            Serial.printf("[ERROR][%d][%d][%d] can't allocate\n", i, freeheap, freepsheap);
            continue;
          }
          BLEDevHelper.init( BLEDevTmpCache[i] );
          delay(1);
        }

      } else {
        BLEDEVCACHE_SIZE = BLEDEVCACHE_HEAP_SIZE;
        Serial.println("[PSRAM] NOT DETECTED, will use heap"); // TODO: adjust cache sizes accordingly

        BLEDevCache = (BlueToothDevice**)calloc(BLEDEVCACHE_SIZE, sizeof( BlueToothDevice ) );
        for(uint16_t i=0; i<BLEDEVCACHE_SIZE; i++) {
          BLEDevCache[i] = (BlueToothDevice*)calloc(1, sizeof( BlueToothDevice ) );
          if( BLEDevCache[i] == NULL ) {
            Serial.printf("[ERROR][%d][%d][%d] can't allocate\n", i, freeheap, freepsheap);
            continue;
          }
          BLEDevHelper.init( BLEDevCache[i], false );
          delay(1);
        }

        BLEDevTmpCache = (BlueToothDevice**)calloc(MAX_DEVICES_PER_SCAN, sizeof( BlueToothDevice ) );
        for(uint16_t i=0; i<MAX_DEVICES_PER_SCAN; i++) {
          BLEDevTmpCache[i] = (BlueToothDevice*)calloc(1, sizeof( BlueToothDevice ) );
          if( BLEDevTmpCache[i] == NULL ) {
            Serial.printf("[ERROR][%d][%d][%d] can't allocate\n", i, freeheap, freepsheap);
            continue;
          }
          BLEDevHelper.init( BLEDevTmpCache[i], false );
          delay(1);
        }

        for(uint16_t i=0; i<OUICACHE_SIZE; i++) {
          OuiHeapCache[i].init( false );
        }

        for(uint16_t i=0; i<VENDORCACHE_SIZE; i++) {
          VendorHeapCache[i].init( false );
        }
      }

      BLEDevTmp = (BlueToothDevice*)calloc(1, sizeof( BlueToothDevice ) );
      BLEDevHelper.init(BLEDevTmp, false); // make sure the copy placeholder isn't using SPI ram

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
      cacheState();
    }


    static void cacheState() {
      BLEDevCacheUsed = 0;
      
      for( uint16_t i=0; i<BLEDEVCACHE_SIZE; i++) {
        if( !isEmpty( BLEDevCache[i]->address ) ) {
          BLEDevCacheUsed++;
        }
        delay(1);
      }
      VendorCacheUsed = 0;
      for( uint16_t i=0; i<VENDORCACHE_SIZE; i++) {
        if( VendorHeapCache[i].devid > -1 ) {
          VendorCacheUsed++;
        }
      }
      OuiCacheUsed = 0;
      for( uint16_t i=0; i<OUICACHE_SIZE; i++) {
        if( !isEmpty( OuiHeapCache[i].assignment ) ) {
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
        default: Serial.println("Can't open null DB"); UI.DBStateIconSetColor(-1); return rc;
      }
      if (rc) {
        Serial.print("Can't open database "); Serial.println(dbName);
        // SD Card removed ? File corruption ? OOM ?
        // isOOM = true;
        UI.DBStateIconSetColor(-1); // OOM or I/O error
        delay(1);
        return rc;
      } else {
        //Serial.println("Opened database successfully");
        if(readonly) {
          UI.DBStateIconSetColor(1); // R/O
        } else {
          UI.DBStateIconSetColor(2); // R+W
        }
        delay(1);
      }
      return rc;
    }

    // close the (hopefully) previously opened DB
    void close(DBName dbName) {
      UI.DBStateIconSetColor(0);
      switch(dbName) {
        case BLE_COLLECTOR_DB:    sqlite3_close(BLECollectorDB); break;
        case MAC_OUI_NAMES_DB:    sqlite3_close(OUIVendorsDB); break;
        case BLE_VENDOR_NAMES_DB: sqlite3_close(BLEVendorsDB); break;
        default: /* duh ! */ Serial.println("Can't open null DB");
      }
      delay(1);
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
      if( isEmpty( address ) || strlen( address ) > MAC_LEN+1 || strlen( address ) < 17 || address[0]==3) {
        Serial.printf("Cowardly refusing to perform an empty or invalid request : %s / %s\n", address, currentBLEAddress);
        return -1;
      }
      open(BLE_COLLECTOR_DB);
      sprintf(searchDeviceQuery, searchDeviceTemplate, address);
      //Serial.print( address ); Serial.print( " => " ); Serial.println(searchDeviceQuery);
      int rc = sqlite3_exec(BLECollectorDB, searchDeviceQuery, BLEDevDBCallback, (void*)dataBLE, &zErrMsg);
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

    // make a copy of the DB to psram to save the SD ^_^
    void loadVendorsToPSRam() {
      results = 0;
      open(BLE_VENDOR_NAMES_DB);
      UI.headerStats("Moving Vendors to PSRam");
      int rc = sqlite3_exec(BLEVendorsDB, "SELECT id, SUBSTR(vendor, 0, 32) as vendor FROM 'ble-oui' where vendor!=''", VendorDBCallback, (void*)dataVendor, &zErrMsg);
      UI.PrintProgressBar( Out.width );
      if (rc != SQLITE_OK) {
        error(zErrMsg);
        sqlite3_free(zErrMsg);
        close(BLE_VENDOR_NAMES_DB);
        //return -2;
      }
      close(BLE_VENDOR_NAMES_DB);
      for(byte i=0;i<8;i++) {
        uint32_t rnd = random(0, VendorDBSize);
        Serial.printf("[%s] Testing random vendor mac #%d: %d / %s\n", __func__, rnd, VendorPsramCache[rnd]->devid[0], VendorPsramCache[rnd]->vendor );
      }
    }

    // make a copy of the DB to psram to save the SD ^_^
    void loadOUIToPSRam() {
      results = 0;
      open(MAC_OUI_NAMES_DB);
      UI.headerStats("Moving OUI to PSRam");
      int rc = sqlite3_exec(OUIVendorsDB, "SELECT LOWER(assignment) as mac, SUBSTR(`Organization Name`, 0, 32) as ouiname FROM 'oui-light'", OUIDBCallback, (void*)dataOUI, &zErrMsg);
      UI.PrintProgressBar( Out.width );
      if (rc != SQLITE_OK) {
        error(zErrMsg);
        sqlite3_free(zErrMsg);
        close(MAC_OUI_NAMES_DB);
        //return -2;
      }
      close(MAC_OUI_NAMES_DB);
      for(byte i=0;i<8;i++) {
        uint32_t rnd = random(0, OUIDBSize);
        Serial.printf("[%s] Testing random mac #%d: %s / %s\n", __func__, rnd, OuiPsramCache[rnd]->mac, OuiPsramCache[rnd]->assignment );
      }
    }

    // shit happens
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


    int DBExec(sqlite3 *db, const char *sql, bool _print_results = false, char *_colNeedle = 0) {
      results = 0;
      print_results = _print_results;
      colNeedle = _colNeedle;
      *colValue = {'\0'};
      int rc = sqlite3_exec(db, sql, DBCallback, (void*)data, &zErrMsg);
      if (rc != SQLITE_OK) {
        error(zErrMsg);
        sqlite3_free(zErrMsg);
      }
      return rc;
    }


    DBMessage insertBTDevice( BlueToothDevice **CacheItem, uint16_t _index) {
      if(isOOM) {
        // cowardly refusing to use DB when OOM
        return DB_IS_OOM;
      }
      if( CacheItem[_index]->appearance==0 
       && isEmpty( CacheItem[_index]->name )
       && isEmpty( CacheItem[_index]->uuid )
       && isEmpty( CacheItem[_index]->ouiname )
       && isEmpty( CacheItem[_index]->manufname )
       ) {
        // cowardly refusing to insert empty result
        return INSERTION_IGNORED;
      }
      open(BLE_COLLECTOR_DB, false);

      clean( CacheItem[_index]->name );
      clean( CacheItem[_index]->ouiname );
      clean( CacheItem[_index]->manufname );
      clean( CacheItem[_index]->uuid );
      
      sprintf(insertQuery, insertQueryTemplate,
        CacheItem[_index]->appearance,
        CacheItem[_index]->name,
        CacheItem[_index]->address,
        CacheItem[_index]->ouiname,
        CacheItem[_index]->rssi,
        CacheItem[_index]->manufid,
        CacheItem[_index]->manufname,
        CacheItem[_index]->uuid
      );

      int rc = DBExec( BLECollectorDB, insertQuery );
      if (rc != SQLITE_OK) {
        Serial.print("SQlite Error occured when heap level was at:"); Serial.println(freeheap);
        Serial.println(insertQuery);
        close(BLE_COLLECTOR_DB);
        CacheItem[_index]->in_db = false;
        return INSERTION_FAILED;
      }
      close(BLE_COLLECTOR_DB);
      CacheItem[_index]->in_db = true;
      return INSERTION_SUCCESS;
    }


    void getVendor(uint16_t devid, char *dest) {
      if( hasPsram ) {
        getPsramVendor(devid, dest);
      } else {
        getHeapVendor(devid, dest);
      }
    }   


    void getOUI(const char* mac, char* dest) {
      if( hasPsram ) {
        getPsramOUI(mac, dest);
      } else {
        getHeapOUI(mac, dest);
      }
    }


    void showDataSamples() {
      open(BLE_COLLECTOR_DB);
      tft.setTextColor(WROVER_YELLOW);
      Out.println(" Collected Named Devices:");
      tft.setTextColor(WROVER_PINK);
      DBExec(BLECollectorDB, nameQuery , true);
      tft.setTextColor(WROVER_YELLOW);
      Out.println(" Collected Devices Vendors:");
      tft.setTextColor(WROVER_PINK);
      DBExec(BLECollectorDB, manufnameQuery, true);
      tft.setTextColor(WROVER_YELLOW);
      Out.println(" Collected Devices MAC's Vendors:");
      tft.setTextColor(WROVER_PINK);
      DBExec(BLECollectorDB, ouinameQuery, true);
      close(BLE_COLLECTOR_DB);
      Out.println();
    }


    unsigned int getEntries(bool _display_results = false) {
      open(BLE_COLLECTOR_DB);
      if (_display_results) {
        DBExec(BLECollectorDB, allEntriesQuery, true);
      } else {
        DBExec(BLECollectorDB, countEntriesQuery, true, (char*)"count(*)");
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
      DBExec(BLECollectorDB, dropTableQuery);
      DBExec(BLECollectorDB, createTableQuery);
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
      //DBExec(BLECollectorDB, cleanTableQuery, true);
      DBExec(BLECollectorDB, pruneTableQuery, true);
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
      DBExec(BLEVendorsDB, testVendorNamesQuery, true);
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
      DBExec(OUIVendorsDB, testOUIQuery, true);
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

  private:

    static void VendorHeapCacheSet(uint16_t cacheindex, int devid, const char* manufname) {
      VendorHeapCache[cacheindex].devid = devid;
      memset( VendorHeapCache[cacheindex].vendor, '\0', MAX_FIELD_LEN+1);
      memcpy( VendorHeapCache[cacheindex].vendor, manufname, strlen(manufname) );
      //Serial.print("[+] VendorHeapCacheSet: "); Serial.println( manufname );
    }
    static uint16_t getNextVendorCacheIndex() {
      VendorCacheIndex++;
      VendorCacheIndex = VendorCacheIndex % VENDORCACHE_SIZE;  
      return VendorCacheIndex;
    }

    // checks for existence in heap cache
    int vendorHeapExists(uint16_t devid) {
      // try fast answer first
      for(int i=0;i<VENDORCACHE_SIZE;i++) {
        if( VendorHeapCache[i].devid == devid) {
          VendorCacheHit++;
          return i;
        }
      }
      return -1;
    }

    // vendor Heap/DB lookup
    void getHeapVendor(uint16_t devid, char *dest) {
      int vendorCacheIdIfExists = vendorHeapExists( devid );
      if(vendorCacheIdIfExists>-1) {
        uint16_t vendorCacheLen = strlen( VendorHeapCache[vendorCacheIdIfExists].vendor );
        memcpy( dest, VendorHeapCache[vendorCacheIdIfExists].vendor, vendorCacheLen );
        dest[vendorCacheLen] = '\0';
        return;
      } else {
        *dest = {'\0'};
      }
      uint16_t vendorcacheindex = getNextVendorCacheIndex();
      open(BLE_VENDOR_NAMES_DB);
      char vendorRequestStr[64] = {'\0'};
      sprintf(vendorRequestStr, vendorRequestTpl, devid);
      DBExec(BLEVendorsDB, vendorRequestStr, true, (char*)"vendor");
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
        VendorHeapCacheSet(vendorcacheindex, devid, colValue);
      } else {
        VendorHeapCacheSet(vendorcacheindex, devid, "[unknown]");
      }
      memcpy( dest, VendorHeapCache[vendorcacheindex].vendor, colValueLen );
      delay(1);
    }

    // checks for existence in psram cache
    int vendorPsramExists(uint16_t devid) {
      // try fast answer first
      for(int i=0;i<VendorDBSize;i++) {
        //Serial.printf("[%s] comparing %d / %s with %d\n", __func__, VendorPsramCache[i]->devid, VendorPsramCache[i]->vendor, devid );
        if( VendorPsramCache[i]->devid[0] == devid) {
          VendorCacheHit++;
          return i;
        }
      }
      return -1;
    }

    // vendor PSRam lookup
    void getPsramVendor(uint16_t devid, char *dest) {
      *dest = {'\0'};
      int VendorCacheIdIfExists = vendorPsramExists( devid );
      if(VendorCacheIdIfExists>-1) {
        byte VendorCacheLen = strlen( VendorPsramCache[VendorCacheIdIfExists]->vendor );
        memcpy( dest, VendorPsramCache[VendorCacheIdIfExists]->vendor, VendorCacheLen );
        VendorPsramCache[VendorCacheIdIfExists]->hits++;
        dest[VendorCacheLen] = '\0';
        return;
      }
      memcpy( dest, "[unknown]", 10 ); // sizeof("[unknown]")      
    }


    static void OUIHeapCacheSet(uint16_t cacheindex, const char* shortmac, const char* assignment) {
      memset( OuiHeapCache[cacheindex].mac, '\0', SHORT_MAC_LEN+1);
      memcpy( OuiHeapCache[cacheindex].mac, shortmac, strlen(shortmac) );
      memset( OuiHeapCache[cacheindex].assignment, '\0', MAX_FIELD_LEN+1);
      memcpy( OuiHeapCache[cacheindex].assignment, assignment, strlen(assignment) );
      //Serial.print("[+] OUICacheSet: "); Serial.println( assignment );
    }
    static uint16_t getNextOUICacheIndex() {
      OuiCacheIndex++;
      OuiCacheIndex = OuiCacheIndex % OUICACHE_SIZE;  
      return OuiCacheIndex;
    }

    // checks for existence in heap cache
    int OUIHeapExists(const char* shortmac) {
      // try fast answer first
      for(int i=0;i<OUICACHE_SIZE;i++) {
        if( strcmp(OuiHeapCache[i].mac, shortmac)==0 ) {
          OuiCacheHit++;
          return i;
        }
      }
      return -1;
    }

    // OUI heap/DB lookup
    void getHeapOUI(const char* mac, char *dest) {
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
      int OUICacheIdIfExists = OUIHeapExists( shortmac );
      if(OUICacheIdIfExists>-1) {
        byte OUICacheLen = strlen( OuiHeapCache[OUICacheIdIfExists].assignment );
        memcpy( dest, OuiHeapCache[OUICacheIdIfExists].assignment, OUICacheLen );
        dest[OUICacheLen] = '\0';
        return;
      }
      uint16_t assignmentcacheindex = getNextOUICacheIndex();
      open(MAC_OUI_NAMES_DB);
      char OUIRequestStr[76];
      sprintf( OUIRequestStr, OUIRequestTpl, shortmac);
      DBExec(OUIVendorsDB, OUIRequestStr, true, (char*)"Organization Name");
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
        OUIHeapCacheSet( assignmentcacheindex, shortmac, colValue );
      } else {
        OUIHeapCacheSet( assignmentcacheindex, shortmac, "[private]" );
      }
      memcpy( dest, OuiHeapCache[assignmentcacheindex].assignment, colValueLen );
      delay(1);
    }

    // checks for existence in PSram cache
    int OUIPsramExists(const char* shortmac) {
      // try fast answer first
      for(int i=0; i<OUIDBSize; i++) {
        //Serial.printf("[%s] comparing %s / %s with %s\n", __func__, OuiPsramCache[i]->mac, OuiPsramCache[i]->assignment, shortmac );
        if( strstr(OuiPsramCache[i]->mac, shortmac) /*==0*/ ) {
          OuiCacheHit++;
          return i;
        }
      }
      return -1;
    }

    // OUI psram lookup
    void getPsramOUI(const char* mac, char *dest) {
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
      int OUICacheIdIfExists = OUIPsramExists( shortmac );
      if(OUICacheIdIfExists>-1) {
        byte OUICacheLen = strlen( OuiPsramCache[OUICacheIdIfExists]->assignment );
        memcpy( dest, OuiPsramCache[OUICacheIdIfExists]->assignment, OUICacheLen );
        OuiPsramCache[OUICacheIdIfExists]->hits++;
        dest[OUICacheLen] = '\0';
        return;
      }
      memcpy( dest, "[private]", 10 ); // sizeof("[private]")
    }

    // loads a DB entry into a BLEDevice struct
    static int BLEDevDBCallback(void *dataBLE, int argc, char **argv, char **azColName) {
      results++;
      if(results < BLEDEVCACHE_SIZE) {
        BLEDevCacheIndex = BLEDevHelper.getNextCacheIndex(BLEDevCache, BLEDevCacheIndex);
        //BLEDevHelper.reset( BLEDevCache[BLEDevCacheIndex] ); // avoid mixing new and old data
        BLEDevHelper.reset( BLEDevCache[BLEDevCacheIndex] );
        for (int i = 0; i < argc; i++) {
          //BLEDevHelper.set( BLEDevCache[BLEDevCacheIndex], azColName[i], argv[i] ? argv[i] : '\0');
          BLEDevHelper.set( BLEDevCache[BLEDevCacheIndex], azColName[i], argv[i] ? argv[i] : '\0');;
        }
        //BLEDevCache[BLEDevCacheIndex].hits = 1;
        //BLEDevCache[BLEDevCacheIndex].in_db = true;

        BLEDevCache[BLEDevCacheIndex]->hits = 1;
        BLEDevCache[BLEDevCacheIndex]->in_db = true;
        
      } else {
        Serial.print("Device Pool Size Exceeded, ignoring: ");
        for (int i = 0; i < argc; i++) {
          Serial.printf("    %s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
        }
      }
      return 0;
    }

    // loads a DB entry into a VendorPsramCache struct
    static int VendorDBCallback(void *dataVendor, int argc, char **argv, char **azColName) {
      results++;
      for (int i = 0; i < argc; i++) {
        if( strcmp( azColName[i], "id" ) == 0 ) {
          //Serial.printf("[%s][%d] Attempting to copy result # %d %s, %d\n", __func__, freepsheap, results, argv[i], atoi( argv[i] ) );
          VendorPsramCache[results-1]->devid[0] = atoi( argv[i] );
          //copy( VendorPsramCache[results-1]->devid, atoi( argv[i] ) );
        }
        if( strcmp( azColName[i], "vendor" ) == 0 ) {
          //Serial.printf("[%s][%d] Attempting to copy result # %d %s\n", __func__, freepsheap, results, argv[i] );
          copy( VendorPsramCache[results-1]->vendor, argv[i], MAX_FIELD_LEN+1);
        }
      }
      VendorPsramCache[results-1]->hits = 0;
      if(results%100==0) {
        float percent = results*100 / VendorDBSize;
        UI.PrintProgressBar( (Out.width * percent) / 100 );
      }
      return 0;
    }

    // loads a DB entry into a OuiPsramCache struct
    static int OUIDBCallback(void *dataOUI, int argc, char **argv, char **azColName) {
      results++;
      for (int i = 0; i < argc; i++) {
        if( strcmp( azColName[i], "mac" ) == 0 ) {
          copy( OuiPsramCache[results-1]->mac, argv[i], SHORT_MAC_LEN+1 );
        }
        if( strcmp( azColName[i], "ouiname" ) == 0 ) {
          copy( OuiPsramCache[results-1]->assignment, argv[i], MAX_FIELD_LEN+1);
        }
      }
      OuiPsramCache[results-1]->hits = 0;
      if(results%100==0) {
        float percent = results*100 / OUIDBSize;
        UI.PrintProgressBar( (Out.width * percent) / 100 );
        //Serial.printf("[Copied %d as %s / %s]\n", results, OuiPsramCache[results-1]->mac, OuiPsramCache[results-1]->assignment );
      }
      return 0;
    }

    // counts results from a DB query
    static int DBCallback(void *data, int argc, char **argv, char **azColName) {
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

};

#endif


DBUtils DB;
