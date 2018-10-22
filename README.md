# ESP32-BLECollector

Like a BLE Scanner but with persistence.

All BLE data found by the [BLE Scanner](https://github.com/nkolban/ESP32_BLE_Arduino) is collected into a [sqlite3](https://github.com/siara-cc/esp32_arduino_sqlite3_lib) format on the SD Card.

Public Mac addresses are compared against [OUI list](https://code.wireshark.org/review/gitweb?p=wireshark.git;a=blob_plain;f=manuf), while Vendor names are compared against [BLE Device list](https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers).

/!\ Use the "No OTA (Large Apps)" partition scheme to compile this sketch.
As a result a SD Card is required and you can't use SPIFFS.

Credits/requirements:

- https://github.com/siara-cc/esp32_arduino_sqlite3_lib
- https://github.com/nkolban/ESP32_BLE_Arduino
