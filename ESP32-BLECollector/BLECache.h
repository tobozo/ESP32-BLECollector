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


/*\
 *

  // TODO: store cache items as vectors in PSRam

  template <class T>
  struct PSallocator {
    typedef T value_type;
    PSallocator() = default;
    template <class U> constexpr PSallocator(const PSallocator<U>&) noexcept {}
    [[nodiscard]] T* allocate(std::size_t n) {
      if(n > std::size_t(-1) / sizeof(T)) throw std::bad_alloc();
      if(auto p = static_cast<T*>(ps_malloc(n*sizeof(T)))) return p;
      throw std::bad_alloc();
    }
    void deallocate(T* p, std::size_t) noexcept { std::free(p); }
  };
  template <class T, class U>
  bool operator==(const PSallocator<T>&, const PSallocator<U>&) { return true; }
  template <class T, class U>
  bool operator!=(const PSallocator<T>&, const PSallocator<U>&) { return false; }

  std::vector<int, PSallocator<int> > v;

 *
\*/

#pragma GCC diagnostic ignored "-Wunused-variable"

struct BLEGATTService
{
  const char* name;
  const char* type;
  uint32_t    assignedNumber;
};

static const BLEGATTService BLE_unknownService = {"Unknown", "", 0 };

static const BLEGATTService BLE_gattServices[] =
{
  {"Alert Notification Service",    "org.bluetooth.service.alert_notification",             0x1811},
  {"Automation IO",                 "org.bluetooth.service.automation_io",                  0x1815},
  {"Battery Service",               "org.bluetooth.service.battery_service",                0x180F},
  {"Blood Pressure",                "org.bluetooth.service.blood_pressure",                 0x1810},
  {"Body Composition",              "org.bluetooth.service.body_composition",               0x181B},
  {"Bond Management",               "org.bluetooth.service.bond_management",                0x181E},
  {"Continuous Glucose Monitoring", "org.bluetooth.service.continuous_glucose_monitoring",  0x181F},
  {"Current Time Service",          "org.bluetooth.service.current_time",                   0x1805},
  {"Cycling Power",                 "org.bluetooth.service.cycling_power",                  0x1818},
  {"Cycling Speed and Cadence",     "org.bluetooth.service.cycling_speed_and_cadence",      0x1816},
  {"Device Information",            "org.bluetooth.service.device_information",             0x180A},
  {"Environmental Sensing",         "org.bluetooth.service.environmental_sensing",          0x181A},
  {"Generic Access",                "org.bluetooth.service.generic_access",                 0x1800},
  {"Generic Attribute",             "org.bluetooth.service.generic_attribute",              0x1801},
  {"Glucose",                       "org.bluetooth.service.glucose",                        0x1808},
  {"Health Thermometer",            "org.bluetooth.service.health_thermometer",             0x1809},
  {"Heart Rate",                    "org.bluetooth.service.heart_rate",                     0x180D},
  {"HTTP Proxy",                    "org.bluetooth.service.http_proxy",                     0x1823},
  {"Human Interface Device",        "org.bluetooth.service.human_interface_device",         0x1812},
  {"Immediate Alert",               "org.bluetooth.service.immediate_alert",                0x1802},
  {"Indoor Positioning",            "org.bluetooth.service.indoor_positioning",             0x1821},
  {"Internet Protocol Support",     "org.bluetooth.service.internet_protocol_support",      0x1820},
  {"Link Loss",                     "org.bluetooth.service.link_loss",                      0x1803},
  {"Location and Navigation",       "org.bluetooth.service.location_and_navigation",        0x1819},
  {"Next DST Change Service",       "org.bluetooth.service.next_dst_change",                0x1807},
  {"Object Transfer",               "org.bluetooth.service.object_transfer",                0x1825},
  {"Phone Alert Status Service",    "org.bluetooth.service.phone_alert_status",             0x180E},
  {"Pulse Oximeter",                "org.bluetooth.service.pulse_oximeter",                 0x1822},
  {"Reference Time Update Service", "org.bluetooth.service.reference_time_update",          0x1806},
  {"Running Speed and Cadence",     "org.bluetooth.service.running_speed_and_cadence",      0x1814},
  {"Scan Parameters",               "org.bluetooth.service.scan_parameters",                0x1813},
  {"Transport Discovery",           "org.bluetooth.service.transport_discovery",            0x1824},
  {"Tx Power",                      "org.bluetooth.service.tx_power",                       0x1804},
  {"User Data",                     "org.bluetooth.service.user_data",                      0x181C},
  {"Weight Scale",                  "org.bluetooth.service.weight_scale",                   0x181D},
  BLE_unknownService // terminator
};

static bool isEmpty(const char* str )
{
  if ( !str ) return true;
  if ( str[0] == '\0' ) return true;
  return ( strcmp( str, "" ) == 0 );
}

static char *formatUnit( int64_t number )
{
  *unitOutput = {'\0'};
  if( number > 999999 ) {
    sprintf(unitOutput, "%lldM", number/1000000);
  } else if( number > 999 ) {
    sprintf(unitOutput, "%lldK", number/1000);
  } else {
    sprintf(unitOutput, "%lld", number);
  }
  return unitOutput;
}

#define BLECARD_MAC_CACHE_SIZE 8 // "virtual" BLE Card circular cache size, keeps mac addresses to avoid duplicate rendering
                                 // the value is based on the max BLECards visible in the scroll area, don't set a too low value
struct macScrollView
{
  char address[MAC_LEN+1];
  uint16_t blockHeight = 0;
  int scrollPosY = 0;
  //int initialPosY = 0;
  uint16_t borderColor = 0;
  int32_t cacheIndex = -1;
};
static macScrollView MacScrollView[BLECARD_MAC_CACHE_SIZE]; // BLECard circular screen cache
static byte lastPrintedMacIndex = 0; // index in the circular buffer

static uint16_t notInCacheCount = 0; // scan-relative
static uint16_t inCacheCount = 0; // scan-relative
static int BLEDevCacheHit = 0; // cache relative
//static int SelfCacheHit = 0; // cache relative
static int AnonymousCacheHit = 0; // cache relative
static int scan_rounds = 0; // how many scans
static uint16_t scan_cursor = 0; // what scan index is being processed

static uint16_t BLEDevCacheUsed = 0; // for statistics
static uint16_t VendorCacheUsed = 0; // for statistics
static uint16_t OuiCacheUsed = 0; // for statistics

static DateTime lastSyncDateTime;
static DateTime nowDateTime;

struct BlueToothDevice
{
  bool in_db          = false;
  bool is_anonymous   = true;
  uint16_t hits       = 0; // cache hits
  uint16_t appearance = 0; // BLE Icon
  int rssi            = 0; // RSSI
  int manufid         = -1;// manufacturer data (or ID)
  uint8_t addr_type;
  char* name      = NULL;// device name
  char* address   = NULL;// device mac address
  char* ouiname   = NULL;// oui vendor name (from mac address, see oui.h)
  char* manufname = NULL;// manufacturer name (from manufacturer data, see ble-oui.db)
  char* uuid      = NULL;// service uuid
  DateTime created_at = 0;
  DateTime updated_at = 0;
};

struct BlueToothDeviceLink
{
  uint16_t cacheIndex;
  BlueToothDevice *device;
};

static uint16_t BLEDevCacheIndex = 0; // index in the circular buffer
//static uint16_t BLEDevScanCacheIndex = 0; // index in the circular buffer

BlueToothDevice** BLEDevRAMCache = NULL; // store returning devices here
BlueToothDevice** BLEDevScanCache = NULL; // store scanned devices before analysis
BlueToothDevice*  BLEDevTmp = NULL; // temporary placeholder used to render BLE Card, explicitly outside SPIram
BlueToothDevice*  BLEDevDBCache = NULL; // temporary placeholder used to hold DB result

static int BLEDEVCACHE_SIZE; // will be set after PSRam detection

static void copy(char* dest, const char* source, byte maxlen)
{
  if( source == nullptr || source == NULL ) return;
  byte sourcelen = strlen(source);
  if( sourcelen < maxlen ) {
    maxlen = sourcelen;
  }
  memcpy( dest, source, maxlen );
  if( maxlen > 0 && dest[maxlen-1]!='\0' ) {
    dest[maxlen] = '\0'; // append null terminate
  }
}

BLEUUID checkUrlUUID = (uint16_t)0xfeaa;


class BlueToothDeviceHelper
{
  public:

    static void init( BlueToothDevice *CacheItem, bool hasPsram=true )
    {
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
      CacheItem->created_at = 0;
      CacheItem->updated_at = 0;
    }

    static void reset( BlueToothDevice *CacheItem )
    {
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
      CacheItem->created_at = 0;
      CacheItem->updated_at = 0;
    }

    static void set( BlueToothDevice *CacheItem, const char* prop, bool val )
    {
      if(!prop) return;
      else if(strcmp(prop, "in_db")==0) { CacheItem->in_db = val;}
      else if(strcmp(prop, "is_anonymous")==0) { CacheItem->is_anonymous = val;}
    }
    static void set( BlueToothDevice *CacheItem, const char* prop, uint8_t val )
    {
      //log_d( "setting address type for %s", BLEAddrTypeToString( val ) ); // https://github.com/nkolban/ESP32_BLE_Arduino/blob/934702b6169b92c71cbc850876fd17fb9ee3236d/src/BLEAdvertisedDevice.h#L44
      if(!prop) return;
      else if(strcmp(prop, "addr_type")==0)   { CacheItem->addr_type = (uint8_t)(val); }
    }
    static void set( BlueToothDevice *CacheItem, const char* prop, DateTime val )
    {
       if(!prop) return;
       else if(strcmp(prop, "created_at")==0) { CacheItem->created_at = val;}
       else if(strcmp(prop, "updated_at")==0) { CacheItem->updated_at = val;}
    }
    static void set( BlueToothDevice *CacheItem, const char* prop, const int val )
    {
      if(!prop) return;
      else if(strcmp(prop, "appearance")==0) { CacheItem->appearance = val;}
      else if(strcmp(prop, "rssi")==0)       { CacheItem->rssi = val;} // coming from thaw()
      else if(strcmp(prop, "manufid")==0)    { CacheItem->manufid = val;}
      else if(strcmp(prop, "hits")==0)       { CacheItem->hits = val;}
    }
    static void set( BlueToothDevice *CacheItem, const char* prop, const char* val )
    {
      if(!prop) return;
      else if(strcmp(prop, "name")==0)       { copy( CacheItem->name, val, MAX_FIELD_LEN ); }
      else if(strcmp(prop, "address")==0)    { copy( CacheItem->address, val, MAC_LEN ); }
      else if(strcmp(prop, "ouiname")==0)    { copy( CacheItem->ouiname, val, MAX_FIELD_LEN ); }
      else if(strcmp(prop, "manufname")==0)  { copy( CacheItem->manufname, val, MAX_FIELD_LEN ); }
      else if(strcmp(prop, "uuid")==0)       { copy( CacheItem->uuid, val, MAX_FIELD_LEN ); }
      else if(strcmp(prop, "rssi")==0)       { CacheItem->rssi = atoi(val);} // coming from BLE
      else if(strcmp(prop, "hits")==0)       { CacheItem->hits = atoi(val);} // coming from DB
      else if(strcmp(prop, "created_at")==0) { CacheItem->created_at = DateTime( atoi(val) );}
      else if(strcmp(prop, "updated_at")==0) { CacheItem->updated_at = DateTime( atoi(val) );}
    }


    static void mergeItems( BlueToothDevice *SourceItem, BlueToothDevice *DestItem )
    {
      copyItem( SourceItem, DestItem, false );
    }

    static void copyItem( BlueToothDevice *SourceItem, BlueToothDevice *DestItem, bool overwrite=true )
    { // overwrite=false will merge
      if(!overwrite) {
        if( strcmp(DestItem->address, SourceItem->address)!=0 ) {
          log_e("Warning: trying to merge items with different addresses, Source: %s, Dest: %s\n", SourceItem->address, DestItem->address );
        }
      }
      set( DestItem, "address",    SourceItem->address );
      if(overwrite) set( DestItem, "in_db",        SourceItem->in_db );
      if(overwrite) set( DestItem, "is_anonymous", SourceItem->is_anonymous );
      if(overwrite) set( DestItem, "hits",         SourceItem->hits );
      if(overwrite) set( DestItem, "rssi",         SourceItem->rssi );
      if(overwrite) set( DestItem, "addr_type",    SourceItem->addr_type );
      if(overwrite || DestItem->appearance==0)            set( DestItem, "appearance", SourceItem->appearance );
      if(overwrite || DestItem->manufid==-1)              set( DestItem, "manufid",    SourceItem->manufid );
      if(overwrite || isEmpty(DestItem->name))            set( DestItem, "name",       SourceItem->name );
      if(overwrite || isEmpty(DestItem->ouiname))         set( DestItem, "ouiname",    SourceItem->ouiname );
      if(overwrite || isEmpty(DestItem->manufname))       set( DestItem, "manufname",  SourceItem->manufname );
      if(overwrite || DestItem->uuid==0)                  set( DestItem, "uuid",       SourceItem->uuid );
      if(overwrite || DestItem->created_at.unixtime()==0) set( DestItem, "created_at", SourceItem->created_at );
      if(overwrite || DestItem->updated_at.unixtime()==0) set( DestItem, "updated_at", SourceItem->updated_at );
    }

    // stores in cache a given advertised device
    static void store( BlueToothDevice *CacheItem, BLEAdvertisedDevice *advertisedDevice )
    {
      reset(CacheItem);// avoid mixing new and old data
      set(CacheItem, "address", advertisedDevice->getAddress().toString().c_str());
      set(CacheItem, "rssi", advertisedDevice->getRSSI());
      set(CacheItem, "addr_type", advertisedDevice->getAddressType());
      if(  advertisedDevice->getAddressType() == BLE_ADDR_RANDOM ) {
        set(CacheItem, "ouiname", "[random]");
      } else {
        set(CacheItem, "ouiname", "[unpopulated]");
      }
      if ( advertisedDevice->haveName() ) {
        set(CacheItem, "name", advertisedDevice->getName().c_str());
      }
      if ( advertisedDevice->haveAppearance() ) {
        set(CacheItem, "appearance", advertisedDevice->getAppearance());
      }
      if ( advertisedDevice->haveManufacturerData() ) {
        uint8_t* mdp = (uint8_t*)advertisedDevice->getManufacturerData().data();
        uint8_t vlsb = mdp[0];
        uint8_t vmsb = mdp[1];
        uint16_t vint = vmsb * 256 + vlsb;
        set(CacheItem, "manufname", "[unpopulated]");
        set(CacheItem, "manufid", vint);
      }

      if( advertisedDevice->haveServiceData() ) {
        //log_d("[%d][%s] Has Service Data[%d]", freeheap, CacheItem->address, strlen( advertisedDevice->getServiceData().c_str() ) );
        //log_d("[%s] GATT ServiceDataUUID: '%s'", CacheItem->address, advertisedDevice->getServiceDataUUID().toString().c_str());
        /*
        const char* serviceData = advertisedDevice->getServiceData().c_str();
        int datalen = strlen( advertisedDevice->getServiceData().c_str() );
        if( datalen > 0  ) {
          Serial.printf("[%s] Service Data[%d]: [", CacheItem->address, datalen );
          Serial.print( advertisedDevice->getServiceData().c_str() );
          Serial.print("] ");
          for( int i=0; i<datalen; i++ ) {
            Serial.printf("%02x",  serviceData[i] );
          }
          Serial.println();
        }*/
      }

      if ( advertisedDevice->haveServiceUUID() ) {
        set(CacheItem, "uuid", advertisedDevice->getServiceUUID().toString().c_str());
        BLEGATTService srv = gattServiceDescription( CacheItem->uuid );

        //const char* serviceStr = gattServiceDescription( advertisedDevice->getServiceUUID() );
        if( strcmp( srv.name, "Unknown" ) != 0 ) {
          log_w("Gatt Service UUID to string %s = %s", advertisedDevice->getServiceUUID().toString().c_str(), srv.name );
        }

        // beacon check
        uint8_t *payLoad = advertisedDevice->getPayload();

        if (advertisedDevice->getServiceUUID().equals(checkUrlUUID)) {
          if (payLoad[11] == 0x10) {
            Serial.println("Found an EddystoneURL beacon!");
            BLEEddystoneURL foundEddyURL = BLEEddystoneURL();
            std::string eddyContent((char *)&payLoad[11]); // incomplete EddystoneURL struct!

            foundEddyURL.setData(eddyContent);
            std::string bareURL = foundEddyURL.getURL();
            if (bareURL[0] == 0x00) {
              size_t payLoadLen = advertisedDevice->getPayloadLength();
              Serial.println("DATA-->");
              for (int idx = 0; idx < payLoadLen; idx++) {
                Serial.printf("0x%08X ", payLoad[idx]);
              }
              Serial.println("\nInvalid Data");
            } else {
              Serial.printf("Found URL: %s\n", foundEddyURL.getURL().c_str());
              Serial.printf("Decoded URL: %s\n", foundEddyURL.getDecodedURL().c_str());
              Serial.printf("TX power %d\n", foundEddyURL.getPower());
              Serial.println("\n");
            }
          } else if (payLoad[11] == 0x20) {
            Serial.println("Found an EddystoneTLM beacon!");
            BLEEddystoneTLM foundEddyURL = BLEEddystoneTLM();
            std::string eddyContent((char *)&payLoad[11]); // incomplete EddystoneURL struct!

            eddyContent = "01234567890123";

            for (int idx = 0; idx < 14; idx++) {
              eddyContent[idx] = payLoad[idx + 11];
            }

            foundEddyURL.setData(eddyContent);
            Serial.printf("Reported battery voltage: %dmV\n", foundEddyURL.getVolt());
            Serial.printf("Reported temperature from TLM class: %.2fC\n", (double)foundEddyURL.getTemp());
            int temp = (int)payLoad[16] + (int)(payLoad[15] << 8);
            float calcTemp = temp / 256.0f;
            Serial.printf("Reported temperature from data: %.2fC\n", calcTemp);
            Serial.printf("Reported advertise count: %d\n", foundEddyURL.getCount());
            Serial.printf("Reported time since last reboot: %ds\n", foundEddyURL.getTime());
            Serial.println("\n");
            Serial.print(foundEddyURL.toString().c_str());
            Serial.println("\n");
          }
        }
        //log_w("Gatt Service UUID to string %s = %s", advertisedDevice->getServiceUUID().toString().c_str(), gattServiceDescription( advertisedDevice->getServiceUUID() ) );
        //uint16_t sUUID = (uint16_t)advertisedDevice->getServiceUUID().getNative();
        //Serial.printf("[%s] GATT ServiceUUID:     '%s'\n", CacheItem->address, advertisedDevice->getServiceUUID().toString().c_str() );
      }
      if( TimeIsSet ) {
        CacheItem->created_at = nowDateTime;
        //log_v("Stored created_at DateTime %d", (unsigned long)nowDateTime.unixtime());
      }
      CacheItem->hits = 1;
    }

    // determines whether a device is worth saving or not
    static bool isAnonymous( BlueToothDevice *CacheItem )
    {
      // if( !isEmpty( CacheItem->uuid )) return false; // uuid's are interesting, let's collect
      if( !isEmpty( CacheItem->name )) return false; // has name, let's collect
      if( CacheItem->appearance !=0 ) return false; // has icon, let's collect
      if( strcmp( CacheItem->ouiname, "[unpopulated]" ) == 0 || strcmp( CacheItem->manufname, "[unpopulated]" ) == 0 ) return false; // don't know yet, let's keep
      if( strcmp( CacheItem->ouiname, "[private]" ) == 0 || strcmp( CacheItem->ouiname, "[random]" ) == 0 || isEmpty( CacheItem->ouiname ) ) return true; // don't care
      if( strcmp( CacheItem->manufname, "[unknown]" ) == 0 || isEmpty( CacheItem->manufname ) ) return true; // don't care
      if( !isEmpty( CacheItem->manufname ) && !isEmpty( CacheItem->ouiname ) ) return false; // anonymous but qualified device, let's collect
      return true;
    }

    static const char *BLEAddrTypeToString( uint8_t type )
    {
      // implented here because the BLELibrary hides this value under debug symbols
      switch (type) {
        case BLE_ADDR_PUBLIC:
          return "BLE_ADDR_TYPE_PUBLIC";
        case BLE_ADDR_RANDOM:
          return "BLE_ADDR_TYPE_RANDOM";
        /*case BLE_ADDR_RPA_PUBLIC:
          return "BLE_ADDR_TYPE_RPA_PUBLIC";
        case BLE_ADDR_RPA_RANDOM:
          return "BLE_ADDR_TYPE_RPA_RANDOM";*/
        default:
          log_e("Unknown addrtype : %d", type );
          return "Unknown uint8_t";
      }
    }

    static const BLEGATTService gattServiceDescription( const char* serviceUUIDStr )
    {
      //const char* serviceUUIDStr = serviceUUID.toString().c_str();
      if( serviceUUIDStr == NULL ) return BLE_unknownService;
      if( strcmp(serviceUUIDStr, "<NULL>") == 0 ) return BLE_unknownService; // wtf ??
      size_t heapbefore = freeheap;

      uint32_t serviceId = 0;

      char* prefix = substr( serviceUUIDStr, 0, 2 );

      if( strcmp( prefix, "0x" ) == 0 ) {
        // e.g. serviceUUID = "0xfe9f"
        int uuidWidth = strlen( serviceUUIDStr ) - 2;
        char* gattUUIDchar = substr( serviceUUIDStr, 2, uuidWidth );
        serviceId = (int)strtol(gattUUIDchar, NULL, 16);
        log_d("[gattServiceDescription(%s)] UUIDchar = '%s', serviceId = '%d'", serviceUUIDStr, gattUUIDchar, serviceId );
        free(gattUUIDchar);
      } else {
        // e.g. serviceUUID = "cbbfe0e1-f7f3-4206-84e0-84cbb3d09dfc"
        char* gattUUIDblock = substr( serviceUUIDStr, 0, 8 );
        char* gattUUIDchar = substr( gattUUIDblock, 4, 4 );
        serviceId = (int)strtol(gattUUIDchar, NULL, 16);
        log_d("[gattServiceDescription(%s)] UUIDblock = '%s', UUIDchar = '%s', serviceId = '%d'", serviceUUIDStr, gattUUIDblock, gattUUIDchar, serviceId );
        free(gattUUIDblock);
        free(gattUUIDchar);
      }

      free( prefix );

      size_t heapafter = freeheap;
      int heapdiff = heapbefore - heapafter;
      byte i = 0;
      while( true ) {
        if (BLE_gattServices[i].assignedNumber == 0) {
          break;
        }
        if (BLE_gattServices[i].assignedNumber == serviceId) {
          log_d("[%d (%d)] Gatt Service UUID to string %04x => %s", freeheap, heapdiff, serviceId, BLE_gattServices[i].name );
          return BLE_gattServices[i]/*.name*/;
        }
        i++;
      }
      return BLE_unknownService;
    } // gattServiceDescription

    static uint16_t getNextCacheIndex( BlueToothDevice **CacheItem, uint16_t CacheItemIndex )
    {
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
