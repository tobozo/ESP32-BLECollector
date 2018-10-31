const char* data = 0; // for some reason sqlite3 db callback needs this
const char* dataBLE = 0; // for some reason sqlite3 db callback needs this
char *zErrMsg = 0; // holds DB Error message
const char BACKSLASH = '\\'; // used to escape() slashes

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
const char *pruneTableQuery = "DELETE FROM blemacs WHERE appearance='' AND name='' AND spower='0' AND uuid='' AND ouiname='[private]' AND (vname LIKE 'Apple%' or vname='[unknown]')";
// used by testVendorNames()
const char *testVendorNamesQuery = "SELECT SUBSTR(vendor,0,32)  FROM 'ble-oui' LIMIT 10";
// used by testOUI()
const char *testOUIQuery = "SELECT * FROM 'oui-light' limit 10";
// used by insertBTDevice()
const char* insertQueryTemplate = "INSERT INTO blemacs(appearance, name, address, ouiname, rssi, vdata, vname, uuid, spower, hits) VALUES('%s','%s','%s','%s','%s','%s','%s','%s','%s','1')";
static char insertQuery[1024]; // stack overflow ? pray that 1024 is enough :D

// used by getVendor()
#define VENDORCACHE_SIZE 32
struct VendorCacheStruct {
  uint16_t devid;
  String vendor;
};
VendorCacheStruct VendorCache[VENDORCACHE_SIZE];
byte VendorCacheIndex = 0;
static int VendorCacheHit = 0;

// used by getOUI()
#define OUICACHE_SIZE 32
struct OUICacheStruct {
  String mac = "";
  String assignment = "";
};
OUICacheStruct OuiCache[OUICACHE_SIZE];
byte OuiCacheIndex = 0;
static int OuiCacheHit = 0;

enum DBMessage {
  TABLE_CREATION_FAILED = -1,
  INSERTION_FAILED = -2,
  INCREMENT_FAILED = -3,
  INSERTION_SUCCESS = 1,
  INCREMENT_SUCCESS = 2
};


// adds a backslash before needle (defaults to single quotes)
String escape(String haystack, String needle = "'") {
  haystack.replace(String(BACKSLASH), String(BACKSLASH) + String(BACKSLASH)); // escape existing backslashes
  haystack.replace(String(needle), String(BACKSLASH) + String(needle)); // slash needle
  return haystack;
}


int StrToHex(char str[]) {
  return (int) strtol(str, 0, 16);
}


// loads a DB entry into a BLEDevice struct
static int BLEDev_db_callback(void *dataBLE, int argc, char **argv, char **azColName) {
  //Serial.println("BLEDev_db_callback");
  results++;
  BLEDevCacheIndex++;
  BLEDevCacheIndex = BLEDevCacheIndex % DEVICEPOOL_SIZE;
  if(results < DEVICEPOOL_SIZE) {
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
        Out.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
      }
    }
  }
  if (print_results && colNeedle == 0) {
    Out.println(out);
  }
  out = "";
  return 0;
}

void resetDB(); // db_exec needs this

int db_exec(sqlite3 *db, const char *sql, bool _print_results = false, char *_colNeedle = 0) {
  results = 0;
  print_results = _print_results;
  colNeedle = _colNeedle;
  colValue = "";
  //Serial.println(sql);
  //long start = micros();
  int rc = sqlite3_exec(db, sql, db_callback, (void*)data, &zErrMsg);
  if (rc != SQLITE_OK) {
    Out.printf("SQL error: %s\n", zErrMsg);
    if (String(zErrMsg) == "database disk image is malformed") {
      resetDB();
    }
    sqlite3_free(zErrMsg);
  } else {
    //Serial.printf("Operation done successfully\n");
  }
  //Serial.print(F("Time taken:")); //Serial.println(micros()-start);
  return rc;
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
  
  sqlite3_open("/sdcard/ble-oui.db", &BLEVendorsDB); // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
  String requestStr = "SELECT vendor FROM 'ble-oui' WHERE id='" + String(devid) + "'";
  db_exec(BLEVendorsDB, requestStr.c_str(), true, "vendor");
  requestStr = "";
  sqlite3_close(BLEVendorsDB);
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
  sqlite3_open("/sdcard/mac-oui-light.db", &OUIVendorsDB); // https://code.wireshark.org/review/gitweb?p=wireshark.git;a=blob_plain;f=manuf
  String requestStr = "SELECT * FROM 'oui-light' WHERE Assignment ='" + mac + "';";
  db_exec(OUIVendorsDB, requestStr.c_str(), true, "Organization Name");
  requestStr = "";
  sqlite3_close(OUIVendorsDB);
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
  sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB);
  tft.setTextColor(WROVER_YELLOW);
  Out.println(" \nCollected Named Devices");
  tft.setTextColor(WROVER_PINK);
  db_exec(BLECollectorDB, nameQuery , true);
  tft.setTextColor(WROVER_YELLOW);
  Out.println(" \nCollected Devices Vendors");
  tft.setTextColor(WROVER_PINK);
  db_exec(BLECollectorDB, vnameQuery, true);
  tft.setTextColor(WROVER_YELLOW);
  Out.println(" \nCollected Devices MAC's Vendors");
  tft.setTextColor(WROVER_PINK);
  db_exec(BLECollectorDB, ouinameQuery, true);
  sqlite3_close(BLECollectorDB);
  Out.println();
}


unsigned int getEntries(bool _display_results = false) {
  sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB);
  if (_display_results) {
    db_exec(BLECollectorDB, allEntriesQuery, true);
  } else {
    db_exec(BLECollectorDB, countEntriesQuery, true, "count(*)");
    results = atoi(colValue.c_str());
  }
  sqlite3_close(BLECollectorDB);
  return results;
}


void resetDB() {
  Out.println();
  Out.println("Re-creating database");
  Out.println();
  SD_MMC.remove("/blemacs.db");
  sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB);
  db_exec(BLECollectorDB, dropTableQuery);
  db_exec(BLECollectorDB, createTableQuery);
  sqlite3_close(BLECollectorDB);
  ESP.restart();
}


void pruneDB() {
  unsigned int before_pruning = getEntries();
  tft.setTextColor(WROVER_YELLOW);
  headerStats("Pruning DB");
  tft.setTextColor(WROVER_GREEN);
  sqlite3_open("/sdcard/blemacs.db", &BLECollectorDB);
  db_exec(BLECollectorDB, pruneTableQuery, true);
  sqlite3_close(BLECollectorDB);
  entries = getEntries();
  tft.setTextColor(WROVER_YELLOW);
  prune_trigger = 0;
  headerStats("DB Pruned");
  footerStats();
}


void testVendorNames() {
  sqlite3_open("/sdcard/ble-oui.db", &BLEVendorsDB); // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
  tft.setTextColor(WROVER_YELLOW);
  Out.println();
  Out.println("Testing Vendor Names Database ...");
  tft.setTextColor(WROVER_ORANGE);
  db_exec(BLEVendorsDB, testVendorNamesQuery, true);
  sqlite3_close(BLEVendorsDB);
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
  sqlite3_open("/sdcard/mac-oui-light.db", &OUIVendorsDB); // https://code.wireshark.org/review/gitweb?p=wireshark.git;a=blob_plain;f=manuf
  tft.setTextColor(WROVER_YELLOW);
  Out.println();
  Out.println("Testing MAC OUI database ...");
  tft.setTextColor(WROVER_ORANGE);
  db_exec(OUIVendorsDB, testOUIQuery, true);
  sqlite3_close(OUIVendorsDB);
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
