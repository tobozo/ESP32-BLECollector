


#if defined( ARDUINO_M5Stack_Core_ESP32 )
  #warning M5STACK CLASSIC DETECTED !!
  #define M5STACK
#elif defined( ARDUINO_M5STACK_FIRE )
  #warning M5STACK FIRE DETECTED !
  #define M5STACK
#elif defined( ARDUINO_ODROID_ESP32 )
  #warning ODROID DETECTED !!
  #define ODROIDGO
#elif defined ( ARDUINO_ESP32_DEV ) 
  #warning WROVER DETECTED !!
  #define WROVER_KIT
#else
  #warning NOTHING DETECTED !!
  //#define WROVER_KIT
  //#define ODROIDGO
  
  //#define M5STACK
  
  //#define DDUINO32XS
  
  //#undef M5STACK
  //#define D32PRO
#endif

#if defined(WROVER_KIT)
  // Adafruit_GFX based driver with SD_MMC
  #include "Display.WROVER_KIT.h"
#elif defined(M5STACK)
  // TFT_eSPI based driver with shared SD
  #include "Display.M5STACK.h"
#elif defined(D32PRO)
  // TFT_eSPI based driver with shared SD
  #include "Display.D32PRO.h"
#elif defined(ODROIDGO)
  // ???
  #include "Display.ODROIDGO.h"
#elif defined(DDUINO32XS)
  #include "Display.DDuino32XS.h"
#else
  #error "Please select a display profile"
#endif

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
#define HEADER_BGCOLOR tft_color565(0x22, 0x22, 0x22)
#define FOOTER_BGCOLOR tft_color565(0x22, 0x22, 0x22)
// BLECard info styling
#define IN_CACHE_COLOR tft_color565(0x37, 0x6b, 0x37)
#define NOT_IN_CACHE_COLOR tft_color565(0xa4, 0xa0, 0x5f)
#define ANONYMOUS_COLOR tft_color565(0x88, 0x88, 0x88)
#define NOT_ANONYMOUS_COLOR tft_color565(0xee, 0xee, 0xee)
// one carefully chosen blue
#define BLUETOOTH_COLOR tft_color565(0x14, 0x54, 0xf0)
#define BLE_DARKORANGE tft_color565(0x80, 0x40, 0x00)
// middle scrolly zone
#define BLECARD_BGCOLOR tft_color565(0x22, 0x22, 0x44)
static uint16_t BGCOLOR = tft_color565(0x22, 0x22, 0x44);
