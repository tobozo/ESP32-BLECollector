# ESP32-BLECollector

Like a BLE Scanner but with persistence.

  [![ESP32 BLECollector running on Wrover-Kit](https://raw.githubusercontent.com/tobozo/ESP32-BLECollector/master/screenshots/capture3.png)][![ESP32 BLECollector running on M5Stack](https://raw.githubusercontent.com/tobozo/ESP32-BLECollector/unstable/screenshots/BLECollector-M5Stack.jpeg)

All BLE data found by the [BLE Scanner](https://github.com/wakwak-koba/ESP32_BLE_Arduino) is collected into a [sqlite3](https://github.com/siara-cc/esp32_arduino_sqlite3_lib) format on the SD Card.

Public Mac addresses are compared against [OUI list](https://code.wireshark.org/review/gitweb?p=wireshark.git;a=blob_plain;f=manuf), while Vendor names are compared against [BLE Device list](https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers).

Two databases are provided in a db format ([mac-oui-light.db](https://github.com/tobozo/ESP32-BLECollector/blob/master/SD/mac-oui-light.db) and [ble-oui.db](https://github.com/tobozo/ESP32-BLECollector/blob/master/SD/ble-oui.db)).

The `blemacs.db` file is created on first run.
When a BLE device is found, it is populated with matching oui/vendor name (if any) and eventually inserted in the `blemasc.db` file.


/!\ Use the "No OTA (Large Apps)" or "Minimal SPIFFS (Large APPS with OTA)" partition scheme to compile this sketch.
The memory cost of using sqlite and BLE libraries is quite high.
As a result you can't use sqlite with SPIFFS, only the SD Card.

Hardware requirements
---------------------
  - [mandatory] ESP32 (with or without PSRam)
  - [mandatory] SD Card breakout (or bundled in Wrover-Kit, M5Stack, LoLinD32 Pro)
  - [mandatory] TFT Library [patched](https://github.com/espressif/WROVER_KIT_LCD/pull/3/files) to support vertical scroll definition
  - [mandatory] BLE Library by @chegewara [patched](https://github.com/tobozo/ESP32-BLECollector/files/2614534/ESP32_ble_library.zip)
  - [mandatory] Micro SD (FAT32 formatted, **max 32GB**)
  - [mandatory] [mac-oui-light.db](https://github.com/tobozo/ESP32-BLECollector/blob/master/SD/mac-oui-light.db) and [ble-oui.db](https://github.com/tobozo/ESP32-BLECollector/blob/master/SD/ble-oui.db) files copied on the Micro SD Card root
  - [mandatory] ILI9341 320x240 TFT (or bundled in Wrover-Kit, M5Stack, Odroid-Go, LoLinD32 Pro)
  - [optional] I2C RTC Module (see "#define RTC_PROFILE" in settings.h)

Software requirements
---------------------
  - [mandatory] Arduino IDE
  - [mandatory] https://github.com/tobozo/ESP32-Chimera-Core
  - [mandatory] https://github.com/tobozo/M5Stack-SD-Updater
  - [mandatory] https://github.com/PaulStoffregen/Time
  - [mandatory] https://github.com/siara-cc/esp32_arduino_sqlite3_lib
  - [optional] https://github.com/mikalhart/TinyGPSPlus
  - [optional] https://github.com/gmag11/NtpClient

RTC available Profiles: 
-----------------------
  - **Hobo**: Default profile, no TinyRTC module in your build, only uptime will be displayed
  - **Rogue**: TinyRTC module adjusted after flashing (build DateTime), no WiFi, no NTP Sync
  - **Chronomaniac**: TinyRTC module adjusts itself via NTP (separate binary, requires WiFi)

Optional I2C RTC Module requirements (/!\ **Chronomaniac** profile only):
-------------------------------------
  - Wite your TinyRTC to SDA/SCL (edit `Wire.begin()` in [Timeutils.h](https://github.com/tobozo/ESP32-BLECollector/blob/master/TimeUtils.h#L173) if necessary)
  - Insert the SD Card
  - Set `#define RTC_PROFILE CHRONOMANIAC` in [Settings.h](https://github.com/tobozo/ESP32-BLECollector/blob/master/Settings.h)
  - Flash the ESP with partition scheme `Minimal SPIFFS (Large APPS with OTA)`
  - Wait for the binary to mirror itself onto the SD Card as '/BLEMenu.bin'
  - Set your `WIFI_SSID` and `WIFI_PASSWD` in [Settings.h](https://github.com/tobozo/ESP32-BLECollector/blob/master/Settings.h)
  - Set `#define RTC_PROFILE NTP_MENU` in [Settings.h](https://github.com/tobozo/ESP32-BLECollector/blob/master/Settings.h)
  - Flash the ESP using partition scheme `Minimal SPIFFS (Large APPS with OTA)` 
  - Wait the binary to mirror itself onto the SD Card as 'NTPMenu.bin'
  - A NTP sync should occur, the BLECollector menu will reload  and start scanning if successful

Contributions are welcome :-)


Known issues / Roadmap
----------------------
Because sqlite3 and BLE library already use a fair amount of RAM, there isn't much room left for additional tools such as Web Server, db parser, NTP sync or data logging over network.

The NTP sync problem is now solved by sd-loading a NTP app, and storing the time on an external RTC module.
So the next big question is about adding controls :thinking:.

Some ideas I'll try to implement in the upcoming changes:

- Implement [GATT services](https://www.bluetooth.com/specifications/gatt/services)
- Use the RTC to add timestamps (and/or) GPS Coords to entries for better pruning [as suggested by /u/playaspect](https://www.reddit.com/r/esp8266/comments/9s594c/esp32blecollector_ble_scanner_data_persistence_on/e8nipr6/?context=3)
- Have the data easily exported without removing the sd card (wifi, ble, serial)
- Auto downloading/refreshing sqlite databases


Other ESP32 security related tools:
-----------------------------------

  - https://github.com/cyberman54/ESP32-Paxcounter
  - https://github.com/G4lile0/ESP32-WiFi-Hash-Monster
  - https://github.com/justcallmekoko/ESP32Marauder


Credits/requirements:
---------------------

- https://github.com/wakwak-koba/ESP32_BLE_Arduino (specifically patched for the current version)
- thanks to https://github.com/chegewara for the help and inspiration
=======
- https://github.com/siara-cc/esp32_arduino_sqlite3_lib
- ~~https://github.com/nkolban/ESP32_BLE_Arduino~~ 
- https://github.com/wakwak-koba/arduino-esp32/tree/master/libraries/BLE (specifically patched for the current version)
- thanks to https://github.com/chegewara (see [this issue](https://github.com/tobozo/ESP32-BLECollector/issues/2))
