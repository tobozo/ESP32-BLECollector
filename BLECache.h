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

#define BLECARD_MAC_CACHE_SIZE 8 // "virtual" BLE Card cache size, keeps mac addresses to avoid duplicate rendering
//static String lastPrintedMac[BLECARD_MAC_CACHE_SIZE]; // BLECard screen cache, where the mac addresses are stored
static char lastPrintedMac[BLECARD_MAC_CACHE_SIZE][18]; // BLECard screen cache, where the mac addresses are stored
static byte lastPrintedMacIndex = 0; // index in the circular buffer

//static char currentBLEAddress[18] = "00:00:00:00:00:00"; // used to proxy BLE search term to DB query

// TODO: store this in psram

struct BlueToothDevice {
  bool in_db = false;
  byte hits = 0;
  String appearance = "";
  String name = ""; // device name
  String address = ""; // device mac address
  String ouiname = ""; // oui vendor name (from mac address, see oui.h)
  String rssi = "";
  uint16_t vdata = 0; // manufacturer data (or ID)
  String vname = ""; // manufacturer name (from manufacturer data, see ble-oui.db)
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
  BLEDevCache[cacheindex].appearance = ""; BLEDevCache[cacheindex].appearance.reserve(16);
  BLEDevCache[cacheindex].name = ""; BLEDevCache[cacheindex].name.reserve(16);
  BLEDevCache[cacheindex].address = ""; BLEDevCache[cacheindex].address.reserve(18);
  BLEDevCache[cacheindex].ouiname = ""; BLEDevCache[cacheindex].ouiname.reserve(32);
  BLEDevCache[cacheindex].rssi = ""; BLEDevCache[cacheindex].rssi.reserve(8);
  BLEDevCache[cacheindex].vdata = 0;// vdata.reserve(32);
  BLEDevCache[cacheindex].vname = ""; BLEDevCache[cacheindex].vname.reserve(16);
  BLEDevCache[cacheindex].uuid = ""; BLEDevCache[cacheindex].uuid.reserve(37);
  //spower = "";
}

static void BLEDevCacheSet(byte cacheindex, const char* prop, const char* val) {
  bool updated = false;
  if(strcmp(prop, "rowid")==0)           { /*id = val.toInt();*/ updated = true; }
  else if(strcmp(prop, "appearance")==0) { BLEDevCache[cacheindex].appearance = String(val); updated = true; }
  else if(strcmp(prop, "name")==0)       { BLEDevCache[cacheindex].name = String(val);       updated = true; }
  else if(strcmp(prop, "address")==0)    { BLEDevCache[cacheindex].address = String(val);    updated = true; }
  else if(strcmp(prop, "ouiname")==0)    { BLEDevCache[cacheindex].ouiname = String(val);    updated = true; }
  else if(strcmp(prop, "rssi")==0)       { BLEDevCache[cacheindex].rssi = String(val);       updated = true; }
  else if(strcmp(prop, "vdata")==0)      { BLEDevCache[cacheindex].vdata = String(val).toInt(); updated = true; }
  else if(strcmp(prop, "vname")==0)      { BLEDevCache[cacheindex].vname = String(val);      updated = true; }
  else if(strcmp(prop, "uuid")==0)       { BLEDevCache[cacheindex].uuid = String(val);       updated = true; }
  //else if(prop=="spower")     { spower = val;     updated = true; }
  //else if(prop=="created_at") { created_at = val; updated = true; }
  //else if(prop=="updated_at") { created_at = val; updated = true; }
  else { }
  if(updated) {
    //Serial.print("[OK]");
  } else {
    //Serial.print("[MISS]");
  }
  //Serial.println("Setting " + String(prop) + " to '" + val + "' ... ");
}

static byte getNextBLEDevCacheIndex() {
  byte min = 255;
  byte defaultIndex = BLEDevCacheIndex;
  defaultIndex++;
  defaultIndex=defaultIndex%BLEDEVCACHE_SIZE;
  byte outIndex = defaultIndex;
  // find first index with least hits
  for(int i=defaultIndex;i<defaultIndex+BLEDEVCACHE_SIZE;i++) {
    byte tempIndex = i%BLEDEVCACHE_SIZE;
    if(BLEDevCache[tempIndex].address=="") return tempIndex;
    if(BLEDevCache[tempIndex].hits < min) {
      min = BLEDevCache[tempIndex].hits;
      outIndex = tempIndex;
    }
  }
  return defaultIndex;      
}


static int BLEDevCacheHit = 0;
static int SelfCacheHit = 0;
static int AnonymousCacheHit = 0;
