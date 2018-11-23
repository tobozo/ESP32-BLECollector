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

#define BLECARD_MAC_CACHE_SIZE 5
//#define BLECARD_MAC_CACHE_SIZE 5 // "virtual" BLE Card circular cache size, keeps mac addresses to avoid duplicate rendering
static char lastPrintedMac[BLECARD_MAC_CACHE_SIZE][18]; // BLECard circular screen cache
static byte lastPrintedMacIndex = 0; // index in the circular buffer

static uint16_t notInCacheCount = 0; // scan-relative
static uint16_t inCacheCount = 0; // scan-relative
static int BLEDevCacheHit = 0; // cache relative
static int SelfCacheHit = 0; // cache relative
static int AnonymousCacheHit = 0; // cache relative

// TODO: store this in psram
struct BlueToothDevice {
  bool in_db = false;
  byte hits = 0; // cache hits
  String appearance = "";
  String name = ""; // device name
  String address = ""; // device mac address
  String ouiname = ""; // oui vendor name (from mac address, see oui.h)
  String rssi = "";
  int manufid = -1; // manufacturer data (or ID)
  char manufname[MAX_FIELD_LEN+1] = {'\0'}; // manufacturer name (from manufacturer data, see ble-oui.db)
  String uuid = ""; // service uuid
  //String spower = "";
  //time_t created_at;
  //time_t updated_at;
};

#ifndef BLEDEVCACHE_SIZE // override this from Settings.h
#define BLEDEVCACHE_SIZE 10
#endif
static BlueToothDevice BLEDevCache[BLEDEVCACHE_SIZE]; // will store database results here 
static byte BLEDevCacheIndex = 0; // index in the circular buffer

void BLEDevCacheReset(byte cacheindex) {
  BLEDevCache[cacheindex].in_db = false;
  BLEDevCache[cacheindex].hits = 0;
  BLEDevCache[cacheindex].appearance = "";
  BLEDevCache[cacheindex].name = ""; 
  BLEDevCache[cacheindex].address = ""; 
  BLEDevCache[cacheindex].ouiname = ""; 
  BLEDevCache[cacheindex].rssi = ""; 
  BLEDevCache[cacheindex].manufid = -1;// manufid.reserve(32);
  memset( BLEDevCache[cacheindex].manufname, '\0', MAX_FIELD_LEN+1 );
  //BLEDevCache[cacheindex].manufname = {'\0'};
  BLEDevCache[cacheindex].uuid = ""; 
  
  BLEDevCache[cacheindex].uuid.reserve(37);
  //BLEDevCache[cacheindex].manufname.reserve(16);
  BLEDevCache[cacheindex].rssi.reserve(8);
  BLEDevCache[cacheindex].ouiname.reserve(32);
  BLEDevCache[cacheindex].address.reserve(18);
  BLEDevCache[cacheindex].name.reserve(16);
  BLEDevCache[cacheindex].appearance.reserve(16);
  //spower = "";
  //created_at = RTC.now().unixtime()
  //updated_at = RTC.now().unixtime()
}

static void BLEDevCacheSet(byte cacheindex, const char* prop, const char* val) {
  bool updated = false;
  if(strcmp(prop, "rowid")==0)           { /*id = val.toInt();*/}
  else if(strcmp(prop, "appearance")==0) { BLEDevCache[cacheindex].appearance = String(val);}
  else if(strcmp(prop, "name")==0)       { BLEDevCache[cacheindex].name = String(val);      }
  else if(strcmp(prop, "address")==0)    { BLEDevCache[cacheindex].address = String(val);   }
  else if(strcmp(prop, "ouiname")==0)    { BLEDevCache[cacheindex].ouiname = String(val);   }
  else if(strcmp(prop, "rssi")==0)       { BLEDevCache[cacheindex].rssi = String(val);      }
  else if(strcmp(prop, "manufid")==0)    { BLEDevCache[cacheindex].manufid = String(val).toInt();}
  else if(strcmp(prop, "manufname")==0)  { memcpy(BLEDevCache[cacheindex].manufname, val, strlen(val)); }
  else if(strcmp(prop, "uuid")==0)       { BLEDevCache[cacheindex].uuid = String(val);      }
  //else if(prop=="spower")     { spower = val;    }
  //else if(prop=="created_at") { created_at = val;}
  //else if(prop=="updated_at") { created_at = val;}
}

static byte getNextBLEDevCacheIndex() {
  byte minCacheValue = 255;
  byte maxCacheValue = 0;
  byte defaultIndex = BLEDevCacheIndex;
  defaultIndex++;
  defaultIndex=defaultIndex%BLEDEVCACHE_SIZE;
  byte outIndex = defaultIndex;
  // find first index with least hits
  for(int i=defaultIndex;i<defaultIndex+BLEDEVCACHE_SIZE;i++) {
    byte tempIndex = i%BLEDEVCACHE_SIZE;
    if(BLEDevCache[tempIndex].address=="") return tempIndex;
    if(BLEDevCache[tempIndex].hits > maxCacheValue) {
      maxCacheValue = BLEDevCache[tempIndex].hits;
    }
    if(BLEDevCache[tempIndex].hits < minCacheValue) {
      minCacheValue = BLEDevCache[tempIndex].hits;
      outIndex = tempIndex;
    }
  }
  return defaultIndex;      
}
