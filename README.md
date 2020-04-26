# ESP32-BLECollector

A BLE Scanner with persistence.

  ![ESP32 BLECollector running on Wrover-Kit](https://raw.githubusercontent.com/tobozo/ESP32-BLECollector/master/screenshots/capture3.png) ![ESP32 BLECollector running on M5Stack](https://raw.githubusercontent.com/tobozo/ESP32-BLECollector/unstable/screenshots/BLECollector-M5Stack.jpeg)

🎬 [Demo video](https://youtu.be/w5V80PobVWs)
------------

BLECollector is just a passive BLE scanner with a fancy UI.
All BLE data found by the BLE Scanner is collected into a [sqlite3](https://github.com/siara-cc/esp32_arduino_sqlite3_lib) format on the SD Card.

Public Mac addresses are compared against [OUI list](https://code.wireshark.org/review/gitweb?p=wireshark.git;a=blob_plain;f=manuf), while Vendor names are compared against [BLE Device list](https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers).

Those two database files are provided in a db format ([mac-oui-light.db](https://github.com/tobozo/ESP32-BLECollector/blob/master/SD/mac-oui-light.db) and [ble-oui.db](https://github.com/tobozo/ESP32-BLECollector/blob/master/SD/ble-oui.db)).

On first run, a default `blemacs.db` file is created, this is where BLE data will be stored.
When a BLE device is found by the scanner, it is populated with the matching oui/vendor name (if any) and eventually inserted in the `blemasc.db` file.

⚠️ This sketch is big! Use the "No OTA (Large Apps)" or "Minimal SPIFFS (Large APPS with OTA)" partition scheme to compile it.
The memory cost of using sqlite and BLE libraries is quite high.

⚠️ Builds using ESP32-Wrover can eventually choose the 3.6MB SPIFFS partition scheme, and have the BLECollector working without the SD Card. Experimental support only since SPIFFS tends to get slower and buggy when the partition becomes full.


Hardware requirements
---------------------
  - [mandatory] ESP32 or ESP32-WROVER (WROVER is recommended)
  - [mandatory] SD Card (breakout or bundled in Wrover-Kit, M5Stack, Odroid-Go, LoLinD32 Pro)
  - [mandatory] Micro SD (FAT32 formatted, **max 4GB**)
  - [mandatory] [mac-oui-light.db](https://github.com/tobozo/ESP32-BLECollector/blob/master/SD/mac-oui-light.db) and [ble-oui.db](https://github.com/tobozo/ESP32-BLECollector/blob/master/SD/ble-oui.db) files copied on the Micro SD Card root
  - [mandatory] ST7789/ILI9341 320x240 TFT (or bundled in Wrover-Kit, M5Stack, Odroid-Go, LoLinD32 Pro)
  - [optional] (but recommended) I2C RTC Module (see `#define HAS_EXTERNAL_RTC` in Settings.h)
  - [optional] Serial GPS Module (see `#define HAS_GPS` in Settings.h)

Software requirements
---------------------
  - [mandatory] Arduino IDE
  - [mandatory] [NimBLE Library](https://github.com/h2zero/NimBLE-Arduino/archive/master.zip) supersedes the BLE (legacy or custom) version, install it manually in the Arduino/Libraries folder
  - [mandatory] [ESP32-Chimera-Core 'LGFX'](https://github.com/tobozo/ESP32-Chimera-Core/archive/lgfx_test.zip) (replaces M5Stack core, still in beta-testing)
  - [mandatory] https://github.com/tobozo/M5Stack-SD-Updater
  - [mandatory] https://github.com/PaulStoffregen/Time
  - [mandatory] https://github.com/siara-cc/esp32_arduino_sqlite3_lib
  - [optional] https://github.com/mikalhart/TinyGPSPlus

Behaviours (auto-selected):
---------------------------
  - **Hobo**: when no TinyRTC module exists in your build, only uptime will be displayed
  - **Rogue**: TinyRTC module adjusted after flashing (build DateTime), shares time over BLE
  - **Chronomaniac**: TinyRTC module adjusts itself via GPS, shares time over BLE

Optional I2C RTC Module requirements
------------------------------------
  - Wire your TinyRTC to SDA/SCL (edit `Wire.begin()` in [Timeutils.h](https://github.com/tobozo/ESP32-BLECollector/blob/master/TimeUtils.h#L173) if necessary)
  - Insert the SD Card
  - Set `#define HAS_EXTERNAL_RTC true` in [Settings.h](https://github.com/tobozo/ESP32-BLECollector/blob/master/Settings.h)
  - Flash the ESP with partition scheme `Minimal SPIFFS (Large APPS with OTA)`

Optional Serial GPS Module requirements
---------------------------------------
  - Wire your GPS module to TX1/RX1 (edit `GPS_RX` and `GPS_TX` in GPS.h
  - Set `#define HAS_GPS true` in [Settings.h](https://github.com/tobozo/ESP32-BLECollector/blob/master/Settings.h)
  - Flash the ESP with partition scheme `Minimal SPIFFS (Large APPS with OTA)`
  - Wait for the GPS to find a fix
  - issue the command `gpstime` in the serial console

Time Sharing
------------
  - Once the time is set using RTC and/or GPS, the BLECollector will start the TimeSharing service and advertise a DateTime characteristic for other BLECollectors to sync with.
  - Builds with no RTC/GPS will try to identify this service during their scan duty cycle and subscribe for notifications.

File Sharing (still experimental)
------------
  - This feature is currently limited to sharing the two necessary .db files, if at least one of those files is missing or corrupted at boot, the BLECollector will start a BLE File server and wait for another BLECollector to send those files.
  - Sending files is still a manual operation, just issue the `blesend` command when another BLECollector is ready to receive the files.
  - Possible outcomes of this feature: sharing/propagating the collected data (e.g. whitelists/blacklists)


Serial command interface
------------

  Available Commands:

    01)             help : Print this list
    02)            start : Start/resume scan
    03)             stop : Stop scan
    04)     toggleFilter : Toggle vendor filter on the TFT (persistent)
    05)       toggleEcho : Toggle BLECards in the Serial Console (persistent)
    06)             dump : Dump returning BLE devices to the display and updates DB
    07)    setBrightness : Set brightness to [value] (0-255)
    08)               ls : Show [dir] Content on the SD
    09)               rm : Delete [file] from the SD
    10)          restart : Restart BLECollector ('restart now' to skip replication)
    11)         bleclock : Broadcast time to another BLE Device (implicit)
    12)          bletime : Get time from another BLE Device (explicit)
    13)       blereceive : Update .db files from another BLE app
    14)          blesend : Share .db files with anothe BLE app
    15)       screenshot : Make a screenshot and save it on the SD
    16)       screenshow : Show screenshot
    17)           toggle : toggle a bool value
    18)          gpstime : sync time from GPS
    19)          resetDB : Hard Reset DB + forced restart
    20)          pruneDB : Soft Reset DB without restarting (hopefully)
    21)          stopBLE : Stop BLE and start WiFi (experimental)
    22)      setWiFiSSID : Set WiFi SSID
    23)      setWiFiPASS : Set WiFi Password


Ftp Server (still experimental)
------------

  This feature will run a ftp server so the .db files can be retrieved from a ftp client over the WiFi network.
  Since FTP sends password as clear text, the username and password are "esp32" and "esp32" (without the quotes).
  This feature assumes the ESP32 had a previous successful connection to a known AP, or had its ssid/password previously set using `setWiFiSSID` and `setWiFiPASS` serial interface commands.
  To start the ftp server, just issue the `stopBLE` command line from the serial interface.
  Required lftp commands, "/path/to/your/local/folder" refers to your PC/Mac :

    $ lftp ftp://esp32@esp32-blecollector
    Password: esp32
    lftp:~> set ftp:passive-mode on
    lftp:~> set ftp:use-feat false
    lftp:~> set ftp:ssl-allow false
    lftp:~> lcd /path/to/your/local/folder
    lftp:~> mirror /

Limitations:

  - /!\ This ftp Server has only been tested with lftp as a client
  - /!\ This ftp Server can only be started from the serial interface


Contributions are welcome :-)


Known issues / Roadmap
----------------------
Because sqlite3 and BLE library already use a fair amount of RAM, there isn't much room left for additional tools such as Web Server, db parser, or NTP sync, so WiFi features are kept out of the scope of this project.

As a compensation, the M5Stack-SD-Updater is built-in and will allow hot-loading of any other .bin (default is menu.bin) located on the SD Card, making it easy to separate functional concerns into different apps.

Some ideas I'll try to implement in the upcoming changes:

- Add GPS Coords to entries for better pruning [as suggested by /u/playaspect](https://www.reddit.com/r/esp8266/comments/9s594c/esp32blecollector_ble_scanner_data_persistence_on/e8nipr6/?context=3)
- Have the data easily exported without removing the sd card (wifi, ble, serial)
- Auto downloading/refreshing sqlite databases


Other ESP32 security related tools:
-----------------------------------

  - https://github.com/cyberman54/ESP32-Paxcounter
  - https://github.com/G4lile0/ESP32-WiFi-Hash-Monster
  - https://github.com/justcallmekoko/ESP32Marauder


Credits/requirements:
---------------------

- https://github.com/siara-cc/esp32_arduino_sqlite3_lib
- https://github.com/fa1ke5/ESP32_FTPServer_SD_MMC
- huge thanks to https://github.com/chegewara for maintaining the initial [BLE library](https://github.com/tobozo/ESP32-BLECollector/releases/download/1.2/BLE.zip) that made this project possible
