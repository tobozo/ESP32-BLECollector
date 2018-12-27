
#include <FS.h>
#include <Adafruit_GFX.h>   // Core graphics library
#include "WROVER_KIT_LCD.h" // Latest version must have the VScroll def patch: https://github.com/espressif/WROVER_KIT_LCD/pull/3/files
WROVER_KIT_LCD tft;
#include <SD_MMC.h>
fs::SDMMCFS &BLE_FS = SD_MMC;
const char* BLE_FS_TYPE = "sdcard";


/*
// TODO: Odroid GO, M5Stack
#define _CONFIG_H_ // cancel user config

#define BUTTON_A_PIN 32
#define BUTTON_B_PIN 33

#define BUTTON_MENU 13
#define BUTTON_SELECT 27
#define BUTTON_VOLUME 0
#define BUTTON_START 39
#define BUTTON_JOY_Y 35
#define BUTTON_JOY_X 34

#define SPEAKER_PIN 26
#define TONE_PIN_CHANNEL 0

#define TFT_LED_PIN 5
#define TFT_MOSI 23
#define TFT_MISO 25
#define TFT_SCLK 19
#define TFT_CS 22  // Chip select control pin
#define TFT_DC 21  // Data Command control pin
#define TFT_RST 18  // Reset pin (could connect to Arduino RESET pin)
#include <odroid_go.h>
// #include <M5Stack.h>
//fs::SDFS BLE_FS = SD;

ILI9341 &tft = GO.lcd;

#include <SD_MMC.h>
fs::SDMMCFS &BLE_FS = SD_MMC;
*/

// UI palette
#define BLE_WHITE       0xFFFF
#define BLE_BLACK       0x0000
#define BLE_GREEN       0x07E0
#define BLE_YELLOW      0xFFE0
#define BLE_GREENYELLOW 0xAFE5
#define BLE_CYAN        0x07FF
#define BLE_ORANGE      0xFD20
#define BLE_DARKGREY    0x7BEF
#define BLE_LIGHTGREY   0xC618
#define BLE_RED         0xF800
#define BLE_DARKGREEN   0x03E0
#define BLE_PURPLE      0x780F
#define BLE_PINK        0xF81F

// top and bottom non-scrolly zones
#define HEADER_BGCOLOR tft.color565(0x22, 0x22, 0x22)
#define FOOTER_BGCOLOR tft.color565(0x22, 0x22, 0x22)
// BLECard info styling
#define IN_CACHE_COLOR tft.color565(0x37, 0x6b, 0x37)
#define NOT_IN_CACHE_COLOR tft.color565(0xa4, 0xa0, 0x5f)
#define ANONYMOUS_COLOR tft.color565(0x88, 0x88, 0x88)
#define NOT_ANONYMOUS_COLOR tft.color565(0xee, 0xee, 0xee)
// one carefully chosen blue
#define BLUETOOTH_COLOR tft.color565(0x14, 0x54, 0xf0)
#define BLE_DARKORANGE tft.color565(0x80, 0x40, 0x00)
// middle scrolly zone
#define BLECARD_BGCOLOR tft.color565(0x22, 0x22, 0x44)
static uint16_t BGCOLOR = tft.color565(0x22, 0x22, 0x44);
