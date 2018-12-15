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

static uint16_t BLEDevCacheUsed = 0; // for statistics
static uint16_t VendorCacheUsed = 0; // for statistics
static uint16_t OuiCacheUsed = 0; // for statistics


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
};


static uint16_t BLEDevCacheIndex = 0; // index in the circular buffer
static uint16_t BLEDevTmpCacheIndex = 0; // index in the circular buffer

BlueToothDevice** BLEDevCache = NULL; // store returning devices here
BlueToothDevice** BLEDevTmpCache = NULL; // store scanned devices before analysis
BlueToothDevice*  BLEDevTmp = NULL; // temporary placeholder used to render BLE Card, explicitly outside SPIram

static int BLEDEVCACHE_SIZE; // will be set after PSRam detection

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


class BlueToothDeviceHelper {
  public:

    static void init( BlueToothDevice *CacheItem, bool hasPsram=true ) {
      CacheItem->in_db      = false;
      CacheItem->is_anonymous = true;
      CacheItem->hits       = 0;
      CacheItem->appearance = 0;
      CacheItem->rssi       = 0;
      CacheItem->manufid    = -1;
      if( hasPsram ) {    
        CacheItem->name      = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
        CacheItem->address   = (char*)ps_calloc(MAC_LEN+1, sizeof(char));
        CacheItem->ouiname   = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
        CacheItem->manufname = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
        CacheItem->uuid      = (char*)ps_calloc(MAX_FIELD_LEN+1, sizeof(char));
      } else {
        CacheItem->name      = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
        CacheItem->address   = (char*)calloc(MAC_LEN+1, sizeof(char));
        CacheItem->ouiname   = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
        CacheItem->manufname = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
        CacheItem->uuid      = (char*)calloc(MAX_FIELD_LEN+1, sizeof(char));
      }
    }

    static void reset( BlueToothDevice *CacheItem ) {
      CacheItem->in_db      = false;
      CacheItem->is_anonymous = true;
      CacheItem->hits       = 0;
      CacheItem->appearance = 0;
      CacheItem->rssi       = 0;
      CacheItem->manufid    = -1;
      memset( CacheItem->name,      0, MAX_FIELD_LEN+1 );
      memset( CacheItem->address,   0, MAC_LEN+1 );
      memset( CacheItem->ouiname,   0, MAX_FIELD_LEN+1 );
      memset( CacheItem->manufname, 0, MAX_FIELD_LEN+1 );
      memset( CacheItem->uuid,      0, MAX_FIELD_LEN+1 );
    }

    static void set(BlueToothDevice *CacheItem, const char* prop, bool val) {
      if(strcmp(prop, "in_db")==0) { CacheItem->in_db = val;}
    }

    static void set(BlueToothDevice *CacheItem, const char* prop, const int val) {
      if(!prop) return;
      if     (strcmp(prop, "appearance")==0) { CacheItem->appearance = val;}
      else if(strcmp(prop, "rssi")==0)       { CacheItem->rssi = val;} // coming from thaw()
      else if(strcmp(prop, "manufid")==0)    { CacheItem->manufid = val;}
    }

    static void set(BlueToothDevice *CacheItem, const char* prop, const char* val) {
      if(!prop) return;
      if     (strcmp(prop, "name")==0)       { copy( CacheItem->name, val, MAX_FIELD_LEN ); }
      else if(strcmp(prop, "address")==0)    { copy( CacheItem->address, val, MAC_LEN ); }
      else if(strcmp(prop, "ouiname")==0)    { copy( CacheItem->ouiname, val, MAX_FIELD_LEN ); }
      else if(strcmp(prop, "manufname")==0)  { copy( CacheItem->manufname, val, MAX_FIELD_LEN ); }
      else if(strcmp(prop, "uuid")==0)       { copy( CacheItem->uuid, val, MAX_FIELD_LEN ); }
      else if(strcmp(prop, "rssi")==0)       { CacheItem->rssi = atoi(val);} // coming from BLE
    }

    // stores in cache a given advertised device
    static void store( BlueToothDevice *CacheItem, BLEAdvertisedDevice advertisedDevice ) {
      reset(CacheItem);// avoid mixing new and old data
      //Serial.printf("[%s] OUI Lookup for %s returned : %s\n", __func__, advertisedDevice.getAddress().toString().c_str(), ouiStrDirty.c_str() );
      set(CacheItem, "address", advertisedDevice.getAddress().toString().c_str());
      set(CacheItem, "rssi", advertisedDevice.getRSSI());
      set(CacheItem, "ouiname", "[unpopulated]");

      if ( advertisedDevice.haveName() ) {
        set(CacheItem, "name", advertisedDevice.getName().c_str());
      } else {
        set(CacheItem, "name", '\0');
      }
      if ( advertisedDevice.haveAppearance() ) {
        set(CacheItem, "appearance", advertisedDevice.getAppearance());
      } else {
        set(CacheItem, "appearance", 0);
      }
      if ( advertisedDevice.haveManufacturerData() ) {
        uint8_t* mdp = (uint8_t*)advertisedDevice.getManufacturerData().data();
        //std::string md = advertisedDevice.getManufacturerData();
        //char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
        uint8_t vlsb = mdp[0];
        uint8_t vmsb = mdp[1];
        uint16_t vint = vmsb * 256 + vlsb;
        set(CacheItem, "manufname", "[unpopulated]");
        set(CacheItem, "manufid", vint);
      } else {
        set(CacheItem, "manufname", '\0');
        set(CacheItem, "manufid", -1);
      }
      if ( advertisedDevice.haveServiceUUID() ) {
        set(CacheItem, "uuid", advertisedDevice.getServiceUUID().toString().c_str());
      } else {
        set(CacheItem, "uuid", '\0');
      }
    }

    // determines whether a device is worth saving or not
    static bool isAnonymous( BlueToothDevice *CacheItem ) {
      if( CacheItem->uuid && strlen( CacheItem->uuid ) >= 0 ) return false; // uuid's are interesting, let's collect
      if( CacheItem->name && strlen( CacheItem->name ) > 0 ) return false; // has name, let's collect
      if( CacheItem->appearance !=0 ) return false; // has icon, let's collect
      if( strcmp( CacheItem->ouiname, "[unpopulated]" ) == 0 ) return false; // don't know yet, let's keep
      if( strcmp( CacheItem->manufname, "[unpopulated]" ) == 0 ) return false; // don't know yet, let's keep
      if( strcmp( CacheItem->ouiname, "[private]" ) == 0 || isEmpty( CacheItem->ouiname ) ) return true; // don't care
      if( strcmp( CacheItem->manufname, "[unknown]" ) == 0 || isEmpty( CacheItem->manufname ) ) return true; // don't care
      if( !isEmpty( CacheItem->manufname ) && !isEmpty( CacheItem->ouiname ) ) return false; // anonymous but qualified device, let's collect
      return true;
    }

    static uint16_t getNextCacheIndex(BlueToothDevice **CacheItem, uint16_t CacheItemIndex) {
      uint16_t minCacheValue = 65535;
      uint16_t maxCacheValue = 0;
      uint16_t defaultIndex = CacheItemIndex;
      defaultIndex++;
      defaultIndex = defaultIndex%BLEDEVCACHE_SIZE;
      uint16_t outIndex = defaultIndex;
      // find first index with least hits
      for(int i=defaultIndex;i<defaultIndex+BLEDEVCACHE_SIZE;i++) {
        uint16_t tempIndex = i%BLEDEVCACHE_SIZE;
        if( isEmpty( CacheItem[tempIndex]->address ) ) {
          return tempIndex;
        }
        if( CacheItem[tempIndex]->hits > maxCacheValue ) {
          maxCacheValue = CacheItem[tempIndex]->hits;
        }
        if( CacheItem[tempIndex]->hits < minCacheValue ) {
          minCacheValue = CacheItem[tempIndex]->hits;
          outIndex = tempIndex;
        }
        delay(1);
      }
      return outIndex;
    }


};


BlueToothDeviceHelper BLEDevHelper;
