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


static bool isEmpty(const char* str ) {
  if ( !str ) return true;
  if ( str[0] == '\0' ) return true;
  return ( strcmp( str, "" ) == 0 );
}

void unset( char* str) {
  str[0] = {'\0'};
}



#define BLECARD_MAC_CACHE_SIZE 5
//#define BLECARD_MAC_CACHE_SIZE 5 // "virtual" BLE Card circular cache size, keeps mac addresses to avoid duplicate rendering
static char lastPrintedMac[BLECARD_MAC_CACHE_SIZE][MAC_LEN+1]; // BLECard circular screen cache
static byte lastPrintedMacIndex = 0; // index in the circular buffer

static uint16_t notInCacheCount = 0; // scan-relative
static uint16_t inCacheCount = 0; // scan-relative
static int BLEDevCacheHit = 0; // cache relative
static int SelfCacheHit = 0; // cache relative
static int AnonymousCacheHit = 0; // cache relative
static int scan_rounds = 0; // how many scans
static uint16_t scan_cursor = 0; // what scan index is being processed

// TODO: store this in psram
struct BlueToothDevice {
  bool in_db          = false;
  bool is_anonymous   = true;
  uint16_t hits       = 0; // cache hits
  uint16_t appearance = 0; // BLE Icon
  int rssi            = 0; // RSSI
  int manufid         = -1;// manufacturer data (or ID)
  char* name      = NULL;// device name
  char* address   = NULL;// device mac address
  char* ouiname   = NULL;// oui vendor name (from mac address, see oui.h)
  char* manufname = NULL;// manufacturer name (from manufacturer data, see ble-oui.db)
  char* uuid      = NULL;// service uuid
  //String spower = "";
  //time_t created_at;
  //time_t updated_at;
  /*
  void init( bool hasPsram=false ) {
    in_db      = false;
    is_anonymous = true;
    hits       = 0;
    appearance = 0;
    rssi       = 0;
    manufid    = -1;
    if( hasPsram ) {    
      name      = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
      address   = (char*)ps_calloc(MAC_LEN+1, sizeof(char));
      ouiname   = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
      manufname = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
      uuid      = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
    } else {
      name      = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
      address   = (char*)calloc(MAC_LEN+1, sizeof(char));
      ouiname   = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
      manufname = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
      uuid      = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
    }
  }
  void reset() {
    in_db      = false;
    is_anonymous = true;
    hits       = 0;
    appearance = 0;
    rssi       = 0;
    manufid    = -1;
    memset( name,      0, MAX_FIELD_LEN+1 );
    memset( address,   0, MAC_LEN+1 );
    memset( ouiname,   0, MAX_FIELD_LEN+1 );
    memset( manufname, 0, MAX_FIELD_LEN+1 );
    memset( uuid,      0, MAX_FIELD_LEN+1 );
  }
  void set(const char* prop, bool val) {
    if(strcmp(prop, "in_db")==0) { in_db = val;}
  }
  void set(const char* prop, const int val) {
    if(!prop) return;
    if     (strcmp(prop, "appearance")==0) { appearance = val;}
    else if(strcmp(prop, "rssi")==0)       { rssi = val;} // coming from thaw()
    else if(strcmp(prop, "manufid")==0)    { manufid = val;}
  }
  void set(const char* prop, const char* val) {
    if(!prop) return;
    if     (strcmp(prop, "name")==0)       { copy( name, val, MAX_FIELD_LEN ); }
    else if(strcmp(prop, "address")==0)    { copy( address, val, MAC_LEN ); }
    else if(strcmp(prop, "ouiname")==0)    { copy( ouiname, val, MAX_FIELD_LEN ); }
    else if(strcmp(prop, "manufname")==0)  { copy( manufname, val, MAX_FIELD_LEN ); }
    else if(strcmp(prop, "uuid")==0)       { copy( uuid, val, MAX_FIELD_LEN ); }
    else if(strcmp(prop, "rssi")==0)       { rssi = atoi(val);} // coming from BLE
  }
  void copy(char* dest, const char* source, byte maxlen) {
    byte sourcelen = strlen(source);
    if( sourcelen < maxlen ) {
      maxlen = sourcelen;
    }
    memcpy( dest, source, maxlen );
    dest[maxlen] = '\0'; // null terminate
  }
  void store( BLEAdvertisedDevice advertisedDevice ) {
    reset();// avoid mixing new and old data
    set("address", advertisedDevice.getAddress().toString().c_str());
    set("rssi", advertisedDevice.getRSSI());
    set("ouiname", "[unpopulated]");
    if ( advertisedDevice.haveName() ) {
      set("name", advertisedDevice.getName().c_str());
    } else {
      set("name", '\0');
    }
    if ( advertisedDevice.haveAppearance() ) {
      set("appearance", advertisedDevice.getAppearance());
    } else {
      set("appearance", 0);
    }
    if ( advertisedDevice.haveManufacturerData() ) {
      uint8_t* mdp = (uint8_t*)advertisedDevice.getManufacturerData().data();
      //std::string md = advertisedDevice.getManufacturerData();
      //char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
      uint8_t vlsb = mdp[0];
      uint8_t vmsb = mdp[1];
      uint16_t vint = vmsb * 256 + vlsb;
      set("manufname", "[unpopulated]");
      set("manufid", vint);
    } else {
      set("manufname", '\0');
      set("manufid", -1);
    }
    if ( advertisedDevice.haveServiceUUID() ) {
      set("uuid", advertisedDevice.getServiceUUID().toString().c_str());
    } else {
      set("uuid", '\0');
    }
  }*/
};

#ifndef BLEDEVCACHE_SIZE // override this from Settings.h
#define BLEDEVCACHE_SIZE 10
#endif
static BlueToothDevice BLEDevCache[BLEDEVCACHE_SIZE]; // will store database results here 
static BlueToothDevice BLEDevTmpCache[BLEDEVCACHE_SIZE]; // will store temporary database results here 
static uint16_t BLEDevCacheIndex = 0; // index in the circular buffer
static uint16_t BLEDevTmpCacheIndex = 0; // index in the circular buffer


class BlueToothDeviceHelper {
  public:
    static void init( BlueToothDevice &_BLEDevCache, bool hasPsram=false ) {
      _BLEDevCache.in_db      = false;
      _BLEDevCache.is_anonymous = true;
      _BLEDevCache.hits       = 0;
      _BLEDevCache.appearance = 0;
      _BLEDevCache.rssi       = 0;
      _BLEDevCache.manufid    = -1;
      if( hasPsram ) {    
        _BLEDevCache.name      = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
        _BLEDevCache.address   = (char*)ps_calloc(MAC_LEN+1, sizeof(char));
        _BLEDevCache.ouiname   = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
        _BLEDevCache.manufname = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
        _BLEDevCache.uuid      = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
      } else {
        _BLEDevCache.name      = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
        _BLEDevCache.address   = (char*)calloc(MAC_LEN+1, sizeof(char));
        _BLEDevCache.ouiname   = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
        _BLEDevCache.manufname = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
        _BLEDevCache.uuid      = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
      }
    }
    static void reset( BlueToothDevice &_BLEDevCache ) {
      _BLEDevCache.in_db      = false;
      _BLEDevCache.is_anonymous = true;
      _BLEDevCache.hits       = 0;
      _BLEDevCache.appearance = 0;
      _BLEDevCache.rssi       = 0;
      _BLEDevCache.manufid    = -1;
      memset( _BLEDevCache.name,      0, MAX_FIELD_LEN+1 );
      memset( _BLEDevCache.address,   0, MAC_LEN+1 );
      memset( _BLEDevCache.ouiname,   0, MAX_FIELD_LEN+1 );
      memset( _BLEDevCache.manufname, 0, MAX_FIELD_LEN+1 );
      memset( _BLEDevCache.uuid,      0, MAX_FIELD_LEN+1 );
    }
    static void set(BlueToothDevice &_BLEDevCache, const char* prop, bool val) {
      if(strcmp(prop, "in_db")==0) { _BLEDevCache.in_db = val;}
    }
    static void set(BlueToothDevice &_BLEDevCache, const char* prop, const int val) {
      if(!prop) return;
      if     (strcmp(prop, "appearance")==0) { _BLEDevCache.appearance = val;}
      else if(strcmp(prop, "rssi")==0)       { _BLEDevCache.rssi = val;} // coming from thaw()
      else if(strcmp(prop, "manufid")==0)    { _BLEDevCache.manufid = val;}
    }
    static void set(BlueToothDevice &_BLEDevCache, const char* prop, const char* val) {
      if(!prop) return;
      if     (strcmp(prop, "name")==0)       { copy( _BLEDevCache.name, val, MAX_FIELD_LEN ); }
      else if(strcmp(prop, "address")==0)    { copy( _BLEDevCache.address, val, MAC_LEN ); }
      else if(strcmp(prop, "ouiname")==0)    { copy( _BLEDevCache.ouiname, val, MAX_FIELD_LEN ); }
      else if(strcmp(prop, "manufname")==0)  { copy( _BLEDevCache.manufname, val, MAX_FIELD_LEN ); }
      else if(strcmp(prop, "uuid")==0)       { copy( _BLEDevCache.uuid, val, MAX_FIELD_LEN ); }
      else if(strcmp(prop, "rssi")==0)       { _BLEDevCache.rssi = atoi(val);} // coming from BLE
    }
    static void copy(char* dest, const char* source, byte maxlen) {
      byte sourcelen = strlen(source);
      if( sourcelen < maxlen ) {
        maxlen = sourcelen;
      }
      memcpy( dest, source, maxlen );
      if( maxlen > 0 && dest[maxlen-1]!='\0' ) {
        dest[maxlen] = '\0'; // append null terminate
      }
    }
    static void store( BlueToothDevice &_BLEDevCache, BLEAdvertisedDevice advertisedDevice ) {
      reset(_BLEDevCache);// avoid mixing new and old data
      set(_BLEDevCache, "address", advertisedDevice.getAddress().toString().c_str());
      set(_BLEDevCache, "rssi", advertisedDevice.getRSSI());
      set(_BLEDevCache, "ouiname", "[unpopulated]");
      if ( advertisedDevice.haveName() ) {
        set(_BLEDevCache, "name", advertisedDevice.getName().c_str());
      } else {
        set(_BLEDevCache, "name", '\0');
      }
      if ( advertisedDevice.haveAppearance() ) {
        set(_BLEDevCache, "appearance", advertisedDevice.getAppearance());
      } else {
        set(_BLEDevCache, "appearance", 0);
      }
      if ( advertisedDevice.haveManufacturerData() ) {
        uint8_t* mdp = (uint8_t*)advertisedDevice.getManufacturerData().data();
        //std::string md = advertisedDevice.getManufacturerData();
        //char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
        uint8_t vlsb = mdp[0];
        uint8_t vmsb = mdp[1];
        uint16_t vint = vmsb * 256 + vlsb;
        set(_BLEDevCache, "manufname", "[unpopulated]");
        set(_BLEDevCache, "manufid", vint);
      } else {
        set(_BLEDevCache, "manufname", '\0');
        set(_BLEDevCache, "manufid", -1);
      }
      if ( advertisedDevice.haveServiceUUID() ) {
        set(_BLEDevCache, "uuid", advertisedDevice.getServiceUUID().toString().c_str());
      } else {
        set(_BLEDevCache, "uuid", '\0');
      }
    }
};


BlueToothDeviceHelper BLEDevHelper;

static uint16_t getNextBLEDevCacheIndex(BlueToothDevice *_BLEDevCache, uint16_t _BLEDevCacheIndex) {
  uint16_t minCacheValue = 65535;
  uint16_t maxCacheValue = 0;
  uint16_t defaultIndex = _BLEDevCacheIndex;
  defaultIndex++;
  defaultIndex = defaultIndex%BLEDEVCACHE_SIZE;
  uint16_t outIndex = defaultIndex;
  // find first index with least hits
  for(int i=defaultIndex;i<defaultIndex+BLEDEVCACHE_SIZE;i++) {
    uint16_t tempIndex = i%BLEDEVCACHE_SIZE;
    if( isEmpty( _BLEDevCache[tempIndex].address ) ) {
      return tempIndex;
    }
    if( _BLEDevCache[tempIndex].hits > maxCacheValue ) {
      maxCacheValue = _BLEDevCache[tempIndex].hits;
    }
    if( _BLEDevCache[tempIndex].hits < minCacheValue ) {
      minCacheValue = _BLEDevCache[tempIndex].hits;
      outIndex = tempIndex;
    }
  }
  return outIndex;
}
