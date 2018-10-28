# ESP32-BLECollector

Like a BLE Scanner but with persistence.

  [![ESP32 BLECollector running on Wrover-Kit](https://img.youtube.com/vi/434LDAfpGjE/0.jpg?v=2)](https://www.youtube.com/watch?v=434LDAfpGjE)


All BLE data found by the [BLE Scanner](https://github.com/nkolban/ESP32_BLE_Arduino) is collected into a [sqlite3](https://github.com/siara-cc/esp32_arduino_sqlite3_lib) format on the SD Card.

Public Mac addresses are compared against [OUI list](https://code.wireshark.org/review/gitweb?p=wireshark.git;a=blob_plain;f=manuf), while Vendor names are compared against [BLE Device list](https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers).

Two databases are provided in a db format ([mac-oui-light.db](https://github.com/tobozo/ESP32-BLECollector/blob/master/SD/mac-oui-light.db) and [ble-oui.db](https://github.com/tobozo/ESP32-BLECollector/blob/master/SD/ble-oui.db)).

The `blemacs.db` file is created on first run.
When a BLE device is found, it is populated with the vendor name (if any) and inserted in the `blemasc.db` file.


/!\ Use the "No OTA (Large Apps)" partition scheme to compile this sketch.
As a result a SD Card is required and you can't use SPIFFS.

Known issues / Roadmap
----------------------

Because sqlite3 and BLE library already use a fair amount of RAM, there isn't much room left for additional tools such as Web Server, db parser, NTP sync or data logging over network.

Also the code is far from optimized (pull requests accepted!).

More memory errors occur when adding any of the following features:

- WiFi
- ArduinoJSON
- FOTA Update from SD
- ~~OLED / TFT~~

Some ideas I'll try to implement in the upcoming changes:

- implement [GATT services](https://www.bluetooth.com/specifications/gatt/services)
- ~~move the ble-oui query outside the devicecallback (this is causing watchdog messages) and populate between scans~~
- reduce the memory problems to avoid restarting the ESP too often (currently restarts when heap is under 120k)
- have the data easily exported without removing the sd card (wifi, ble, serial)

Credits/requirements:

- https://github.com/siara-cc/esp32_arduino_sqlite3_lib
- https://github.com/nkolban/ESP32_BLE_Arduino
