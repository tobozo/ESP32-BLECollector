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
static String lastPrintedMac[BLECARD_MAC_CACHE_SIZE]; // BLECard screen cache, where the mac addresses are stored
static byte lastPrintedMacIndex = 0; // index in the circular buffer

// TODO: store this in psram

struct BlueToothDevice {
  //int id;
  bool in_db = false;
  uint16_t borderColor;
  uint16_t textColor;
  String appearance = "";
  String name = ""; // device name
  String address = ""; // device mac address
  String ouiname = ""; // oui vendor name (from mac address, see oui.h)
  String rssi = "";
  String vdata = ""; // manufacturer data
  String vname = ""; // manufacturer name (from manufacturer data, see ble-oui.db)
  String uuid = ""; // service uuid
  //String spower = "";
  //time_t created_at;
  //time_t updated_at;
  void reset() {
    in_db = false;
    borderColor;
    textColor;
    appearance = "";
    name = "";
    address = "";
    ouiname = "";
    rssi = "";
    vdata = "";
    vname = "";
    uuid = "";
    //spower = "";
  }
  void set(String prop, String val) {
    bool updated = false;
    if(prop=="id")              { /*id = val.toInt();*/ updated = true; }
    else if(prop=="appearance") { appearance = val; updated = true; }
    else if(prop=="name")       { name = val;       updated = true; }
    else if(prop=="address")    { address = val;    updated = true; }
    else if(prop=="ouiname")    { ouiname = val;    updated = true; }
    else if(prop=="rssi")       { rssi = val;       updated = true; }
    else if(prop=="vdata")      { vdata = val;      updated = true; }
    else if(prop=="vname")      { vname = val;      updated = true; }
    else if(prop=="uuid")       { uuid = val;       updated = true; }
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

#ifndef BLEDEVCACHE_SIZE // override this from Settings.h
#define BLEDEVCACHE_SIZE 10
#endif
static BlueToothDevice BLEDevCache[BLEDEVCACHE_SIZE]; // will store database results here 
static byte BLEDevCacheIndex = 0; // index in the circular buffer

static int BLEDevCacheHit = 0;
static int SelfCacheHit = 0;
static int AnonymousCacheHit = 0;
