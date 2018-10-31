#define BLECARD_MAC_CACHE_SIZE 4 // "virtual" BLE Card cache size, keeps mac addresses to avoid duplicate rendering
static String lastPrintedMac[BLECARD_MAC_CACHE_SIZE]; // BLECard screen cache, where the mac addresses are stored
static byte lastPrintedMacIndex = 0;

struct BlueToothDevice {
  int id;
  bool in_db = false;
  uint16_t deviceColor;
  String appearance = "";
  String name = ""; // device name
  String address = ""; // device mac address
  String ouiname = ""; // oui vendor name (from mac address, see oui.h)
  String rssi = "";
  String vdata = ""; // manufacturer data
  String vname = ""; // manufacturer name (from manufacturer data, see ble-oui.db)
  String uuid = ""; // service uuid
  String spower = "";
  time_t created_at;
  time_t updated_at;
  void set(String prop, String val) {
    bool updated = false;
    if(prop=="id")              { id = val.toInt(); updated = true; }
    else if(prop=="appearance") { appearance = val; updated = true; }
    else if(prop=="name")       { name = val;       updated = true; }
    else if(prop=="address")    { address = val;    updated = true; }
    else if(prop=="ouiname")    { ouiname = val;    updated = true; }
    else if(prop=="rssi")       { rssi = val;       updated = true; }
    else if(prop=="vdata")      { vdata = val;      updated = true; }
    else if(prop=="vname")      { vname = val;      updated = true; }
    else if(prop=="uuid")       { uuid = val;       updated = true; }
    else if(prop=="spower")     { spower = val;     updated = true; }
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
  /*
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
  }*/

};

#define DEVICEPOOL_SIZE 16 // don't store more than the memory can fit
static BlueToothDevice BLEDevCache[DEVICEPOOL_SIZE]; // will store database results here 
static byte BLEDevCacheIndex = 0;
static int BLEDevCacheHit = 0;
static int SelfCacheHit = 0;
