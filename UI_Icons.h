

enum IconStatus {
  ICON_STATUS_AVAILABLE,
  ICON_STATUS_UNAVAILABLE,
  ICON_STATUS_SET,
  ICON_STATUS_UNSET,
  ICON_STATUS_IDLE,
  ICON_STATUS_ADV_SCAN,
  ICON_STATUS_ADV_WHITELISTED,
  ICON_STATUS_ROLE_NONE,
  ICON_STATUS_ROLE_CLOCK_SHARING,
  ICON_STATUS_ROLE_CLOCK_SEEKING,
  ICON_STATUS_ROLE_FILE_SHARING,
  ICON_STATUS_ROLE_FILE_SEEKING,
  ICON_STATUS_DB_READ,
  ICON_STATUS_DB_WRITE,
  ICON_STATUS_DB_ERROR,
  ICON_STATUS_DB_IDLE,
  ICON_STATUS_HEAP,
  ICON_STATUS_ENTRIES,
  ICON_STATUS_LAST,
  ICON_STATUS_SEEN,
  ICON_STATUS_SCANS,
  ICON_STATUS_UPTIME,
  ICON_STATUS_DISABLED
};

enum IconType {
  ICON_TYPE_JPG,
  ICON_TYPE_GEOMETRIC,
  ICON_TYPE_WIDGET,
  ICON_TYPE_HYBRID,
  ICON_TYPE_TEXT
};

enum IconShapes {
  ICON_SHAPE_NONE,
  ICON_SHAPE_DISC,
  ICON_SHAPE_CIRCLE,
  ICON_SHAPE_SQUARE,
  ICON_SHAPE_TRIANGLE
};

enum IconWidgets {
  ICON_WIDGET_NONE,
  ICON_WIDGET_RSSI,
  ICON_WIDGET_PERCENT,
  ICON_WIDGET_TEXT
};

struct Icon;
struct IconSrc;
struct IconWidget;
struct IconShape;
struct Icon;
struct IconBar;

bool IconRender( Icon *icon, uint16_t offsetX, uint16_t offsetY );
void IconRender( IconSrc* src, uint16_t x, uint16_t y );
void IconRender( IconWidget* widget, uint16_t posX, uint16_t posY, uint16_t width, uint16_t height, uint16_t offsetX, uint16_t offsetY, uint16_t bgcolor );
void IconRender( IconShape *shape, uint16_t posX, uint16_t posY, uint16_t width, uint16_t height, uint16_t offsetX, uint16_t offsetY );

void (*rssiPointer)(int16_t x, int16_t y, int16_t rssi, uint16_t bgcolor, float size);
void (*percentPointer)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t percent, uint16_t barcolor, uint16_t bgcolor, uint16_t bordercolor);
void (*textAlignPointer)(const char* text, uint16_t x, uint16_t y, int16_t color, int16_t bgcolor, uint8_t textAlign);

void (*fillCirclePointer)( uint16_t x, uint16_t y, uint16_t r, uint16_t color);
void (*drawCirclePointer)( uint16_t x, uint16_t y, uint16_t r, uint16_t color);
void (*fillRectPointer)( uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void (*fillTrianglePointer)( uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);


struct IconSrc {
  const unsigned char *jpeg;
  size_t jpeg_len;
  IconSrc(const unsigned char *_jpeg, size_t _jpeg_len) {
    jpeg = _jpeg;
    jpeg_len = _jpeg_len;
  }
};


struct IconWidget {
  IconWidgets type;
  int32_t value;
  char *text;
  uint16_t color;
  uint8_t align;
  void (*cb)( uint16_t posX, uint16_t posY, uint16_t bgcolor );
  void setValue( int32_t val ) {
    if( value != val ) {
      value = val;
      if( cb ) cb( 0, 0, 0 );
    }
  }
  void setText( char* intext, uint16_t posX, uint16_t posY, uint16_t textcolor, uint16_t bgcolor, uint8_t textalign ) {
    text = intext;
    color = textcolor;
    align = textalign;
    if( cb ) cb( posX, posY, bgcolor );
  }
};


struct IconShape {
  IconShapes type;
  uint16_t color;
  IconShape( IconShapes _type, uint16_t _color ) {
    type  = _type;
    color = _color;
  }
};


struct Icon {
  const char* name;     // just a dummy name for debugging
  uint16_t    width;
  uint16_t    height;
  uint16_t    posX;
  uint16_t    posY;
  uint16_t    bgcolor;
  uint8_t     statuses; // how many different statuses this icon handles
  bool        render;   // does it need rendering ?
  IconType    type;     // jpg, geometric, widget, text, hybrid
  IconStatus  status;   // current status
  IconStatus  *states;  // available statuses

  IconSrc     **src;    // [optional] jpg images (must match statuses)
  IconShape   **shapes; // [optional] shapes (must match statuses)
  IconWidget  *widget;  // [optional] widgets (must match statuses)

  void init() {
    log_w("Inited icon '%s' [%d*%d] using %d states", name, width, height, statuses);
  }

  void setRender( bool _render = true ) {
    render = _render;
  }

  void setStatus( IconStatus _status ) {
    bool statusexists = false;
    if( _status == status ) {
      log_v("Status unchanged");
      return;
    }
    for(byte i=0; i<statuses; i++) {
      if( states[i]==_status) statusexists = true;
    }
    if( statusexists ) {
      log_v("Icon '%s' changed status from %d to %d and will be marked for rendering", name, status, _status);
      status = _status;
      setRender();
    } else {
      log_e("Invalid status query for %s icon: %d", name, _status);
    }
  }
};


struct IconBar {
  uint16_t    width;
  uint16_t    height;
  uint8_t     margin;
  size_t      totalIcons;
  Icon        **icons;
  void init( Icon **_icons, size_t _totalIcons, uint8_t _margin=2 ) {
    icons = _icons;
    totalIcons = _totalIcons;
    margin = _margin;
    width      = 0;
    height     = 0;
    for( byte i=0; i<totalIcons; i++ ) {
      icons[i]->init();
      if( icons[i]->type == ICON_TYPE_WIDGET && icons[i]->widget->type == ICON_WIDGET_TEXT ) continue;
      icons[i]->posX = width;
      width+= icons[i]->width + margin;
      if( icons[i]->height > height ) {
        height = icons[i]->height;
      }

    }
    for( byte i=0; i<totalIcons; i++ ) {
      if( icons[i]->type == ICON_TYPE_WIDGET && icons[i]->widget->type == ICON_WIDGET_TEXT ) continue;
      icons[i]->posY = height/2 - icons[i]->height/2;
    }
    log_w("Iconbar dimensions:[%d*%d] with %d icons", width, height, totalIcons);
  }
  void draw(uint16_t x, uint16_t y ) {
    uint16_t posx = 0;
    uint8_t rendered = 0;
    for( byte i=0; i<totalIcons; i++ ) {
      if( IconRender( icons[i], x, y ) ) {
        rendered++;
      }
      posx += icons[i]->width + 2;
    }
  }
};


void IconRender( IconSrc* src, uint16_t x, uint16_t y ) {
  tft.drawJpg( src->jpeg, src->jpeg_len, x, y );
}

void IconRender( IconWidget* widget, uint16_t posX, uint16_t posY, uint16_t width, uint16_t height, uint16_t offsetX, uint16_t offsetY, uint16_t bgcolor ) {
  switch( widget->type ) {
    case ICON_WIDGET_RSSI: // this widget uses relative positioning
      rssiPointer( offsetX+posX+1, offsetY+posY, widget->value, widget->color, 1.0 );
    break;
    case ICON_WIDGET_PERCENT: // this widget uses relative positioning
      percentPointer( offsetX+posX, offsetY+posY-1, width, height, widget->value, widget->color, bgcolor, BLE_DARKGREY);
    break;
    case ICON_WIDGET_TEXT: // this widget uses absolute positioning
      textAlignPointer( widget->text, posX, posY, widget->color, bgcolor, widget->align );
    break;
    default:
    break;
  }
}

void IconRender( IconShape *shape, uint16_t posX, uint16_t posY, uint16_t width, uint16_t height, uint16_t offsetX, uint16_t offsetY ) {
  switch( shape->type ) {
    case ICON_SHAPE_DISC:
      fillCirclePointer( offsetX+posX+width/2, offsetY-1+posY+height/2, (width/2)-1, shape->color );
    break;
    case ICON_SHAPE_CIRCLE:
      drawCirclePointer( offsetX+posX+width/2, offsetY-1+posY+height/2, (width/2)-1, shape->color );
    break;
    case ICON_SHAPE_SQUARE:
      fillRectPointer( offsetX+posX, offsetY+posY, width, height, shape->color );
    break;
    case ICON_SHAPE_TRIANGLE:
      fillTrianglePointer( offsetX+posX, offsetY+posY, offsetX+posX+width, offsetY+posY, (offsetX+posX+width/2), offsetY+posY+height-1, shape->color );
    break;
    default:
    break;
  }
}

bool IconRender( Icon *icon, uint16_t offsetX, uint16_t offsetY ) {
  if( !icon->render ) {
    log_v("icon '%s' Already rendered", icon->name);
    return false;
  }
  icon->setRender( false );
  if( icon->status == ICON_STATUS_DISABLED ) {
    takeMuxSemaphore();
    tft.fillRect( offsetX+icon->posX, offsetY+icon->posY, icon->width, icon->height, icon->bgcolor);
    giveMuxSemaphore();
    return true;
  }
  int statusIndex = -1;
  for( byte i=0; i<icon->statuses; i++ ) {
    if( icon->states[i] == icon->status ) {
      statusIndex = i;
      break;
    }
  }
  if( statusIndex == -1 ) {
    log_e("No valid status found for icon '%s' / status %d", icon->name, icon->status);
    return false;
  }
  log_v("Will render icon '%s' with statusIndex %d", icon->name, statusIndex);
  takeMuxSemaphore();
  switch( icon->type ) {
    case ICON_TYPE_JPG:
      IconRender( icon->src[statusIndex], offsetX+icon->posX, offsetY+icon->posY );
    break;
    case ICON_TYPE_GEOMETRIC:
      IconRender( icon->shapes[statusIndex], icon->posX, icon->posY, icon->width, icon->height, offsetX, offsetY );
    break;
    case ICON_TYPE_WIDGET:
      if( icon->src != NULL ) {
        IconRender( icon->src[statusIndex], icon->posX-icon->width/2, offsetY+icon->posY-icon->height/2 );
      }
      IconRender( icon->widget, icon->posX, icon->posY, icon->width, icon->height, offsetX, offsetY, icon->bgcolor );
    break;
    case ICON_TYPE_HYBRID:
      //todo: implement
    break;
    default:
    break;
  }
  giveMuxSemaphore();
  return true;
}



// default statuses on/off
IconStatus DefaultStatuses[]  = { ICON_STATUS_SET, ICON_STATUS_UNSET };

// time + RTC status
IconSrc *TimeIcon_SET_src   = new IconSrc( clock_jpeg,  clock_jpeg_len );
IconSrc *TimeIcon_UNSET_src = new IconSrc( clock3_jpeg, clock3_jpeg_len );
IconSrc *TimeIcon_RTC_src   = new IconSrc( clock2_jpeg, clock2_jpeg_len );
IconSrc *TimeIcons[]        = { TimeIcon_SET_src, TimeIcon_UNSET_src, TimeIcon_RTC_src };
IconStatus TimeStatuses[]   = { ICON_STATUS_SET, ICON_STATUS_UNSET, ICON_STATUS_AVAILABLE };
Icon TimeIcon = {
  .name      = "Time status Icon",
  .width     = 8,
  .height    = 8,
  .posX      = 0,
  .posY      = 0,
  .bgcolor   = HEADER_BGCOLOR,
  .statuses  = 3,
  .render    = true,
  .type      = ICON_TYPE_JPG,
  .status    = ICON_STATUS_UNSET,
  .states    = TimeStatuses,
  .src       = TimeIcons
};

// BLE activity
IconShape *Shape_BLE_OFF             = new IconShape( ICON_SHAPE_DISC, HEADER_BGCOLOR );
IconShape *Shape_BLE_IDLE            = new IconShape( ICON_SHAPE_DISC, BLE_DARKGREY );
IconShape *Shape_BLE_ADV_SCAN        = new IconShape( ICON_SHAPE_DISC, BLUETOOTH_COLOR );
IconShape *Shape_BLE_ADV_WHITELISTED = new IconShape( ICON_SHAPE_CIRCLE, BLE_GREENYELLOW );
IconShape *BLEActivityShapes[]       = { Shape_BLE_OFF, Shape_BLE_IDLE, Shape_BLE_ADV_SCAN, Shape_BLE_ADV_WHITELISTED };
IconStatus BLEActivityStatuses[]     = { ICON_STATUS_UNSET, ICON_STATUS_IDLE, ICON_STATUS_ADV_SCAN, ICON_STATUS_ADV_WHITELISTED };
Icon BLEActivityIcon = {
  .name      = "BLE Activity Icon",
  .width     = 8,
  .height    = 8,
  .posX      = 0,
  .posY      = 0,
  .bgcolor   = HEADER_BGCOLOR,
  .statuses  = 4,
  .render    = true,
  .type      = ICON_TYPE_GEOMETRIC,
  .status    = ICON_STATUS_IDLE,
  .states    = BLEActivityStatuses,
  .src       = NULL,
  .shapes    = BLEActivityShapes
};

// BLE role
IconShape *Shape_ROLE_NONE          = new IconShape( ICON_SHAPE_SQUARE, BLE_DARKGREY );
IconShape *Shape_ROLE_CLOCK_SHARING = new IconShape( ICON_SHAPE_SQUARE, BLE_DARKORANGE );
IconShape *Shape_ROLE_CLOCK_SEEKING = new IconShape( ICON_SHAPE_SQUARE, BLE_GREENYELLOW );
IconShape *Shape_ROLE_FILE_SHARING  = new IconShape( ICON_SHAPE_SQUARE, BLE_ORANGE );
IconShape *Shape_ROLE_FILE_SEEKING  = new IconShape( ICON_SHAPE_SQUARE, BLUETOOTH_COLOR );
IconShape *BLESRolehapes[]          = { Shape_ROLE_NONE, Shape_ROLE_CLOCK_SHARING, Shape_ROLE_CLOCK_SEEKING, Shape_ROLE_FILE_SHARING, Shape_ROLE_FILE_SEEKING };
IconStatus BLERoleStatuses[]        = { ICON_STATUS_ROLE_NONE, ICON_STATUS_ROLE_CLOCK_SHARING, ICON_STATUS_ROLE_CLOCK_SEEKING, ICON_STATUS_ROLE_FILE_SHARING, ICON_STATUS_ROLE_FILE_SEEKING };
Icon BLERoleIcon = {
  .name      = "BLE Role Icon",
  .width     = 6,
  .height    = 6,
  .posX      = 0,
  .posY      = 0,
  .bgcolor   = HEADER_BGCOLOR,
  .statuses  = 5,
  .render    = true,
  .type      = ICON_TYPE_GEOMETRIC,
  .status    = ICON_STATUS_ROLE_NONE,
  .states    = BLERoleStatuses,
  .src       = NULL,
  .shapes    = BLESRolehapes
};

// DB state
IconShape *Shape_DB_READ  = new IconShape( ICON_SHAPE_TRIANGLE, BLE_GREEN );
IconShape *Shape_DB_WRITE = new IconShape( ICON_SHAPE_TRIANGLE, BLE_ORANGE );
IconShape *Shape_DB_ERROR = new IconShape( ICON_SHAPE_TRIANGLE, BLE_RED );
IconShape *Shape_DB_IDLE  = new IconShape( ICON_SHAPE_TRIANGLE, BLE_DARKGREEN );
IconShape *DBShapes[]     = { Shape_DB_READ, Shape_DB_WRITE, Shape_DB_ERROR, Shape_DB_IDLE };
IconStatus DBStatuses[]   = { ICON_STATUS_DB_READ, ICON_STATUS_DB_WRITE, ICON_STATUS_DB_ERROR, ICON_STATUS_DB_IDLE };
Icon DBIcon = {
  .name      = "DB Icon",
  .width     = 5,
  .height    = 6,
  .posX      = 0,
  .posY      = 0,
  .bgcolor   = HEADER_BGCOLOR,
  .statuses  = 4,
  .render    = true,
  .type      = ICON_TYPE_GEOMETRIC,
  .status    = ICON_STATUS_DB_IDLE,
  .states    = DBStatuses,
  .src       = NULL,
  .shapes    = DBShapes
};

// GPS state
IconSrc *GPSIcon_SET_src   = new IconSrc( gps_jpg,   gps_jpg_len );
IconSrc *GPSIcon_UNSET_src = new IconSrc( nogps_jpg, nogps_jpg_len );
IconSrc *GPSIcons[]        = { GPSIcon_SET_src, GPSIcon_UNSET_src };
Icon GPSIcon = {
  .name      = "GPS status Icon",
  .width     = 10,
  .height    = 10,
  .posX      = 0,
  .posY      = 0,
  .bgcolor   = HEADER_BGCOLOR,
  .statuses  = 2,
  .render    = true,
  .type      = ICON_TYPE_JPG,
  .status    = ICON_STATUS_UNSET,
  .states    = DefaultStatuses,
  .src       = GPSIcons
};

// Filter state
IconSrc *VendorFilterIcon_SET   = new IconSrc( filter_jpeg, filter_jpeg_len );
IconSrc *VendorFilterIcon_UNSET = new IconSrc( filter_unset_jpeg, filter_unset_jpeg_len );
IconSrc *VendorFilterIcons[]    = { VendorFilterIcon_SET, VendorFilterIcon_UNSET };
Icon VendorFilterIcon = {
  .name      = "Vendor Filter Icon",
  .width     = 10,
  .height    = 8,
  .posX      = 0,
  .posY      = 0,
  .bgcolor   = HEADER_BGCOLOR,
  .statuses  = 2,
  .render    = true,
  .type      = ICON_TYPE_JPG,
  .status    = ICON_STATUS_UNSET,
  .states    = DefaultStatuses,
  .src       = VendorFilterIcons
};

// Text Counters
IconSrc *TextCounters_heap_src    = new IconSrc( disk_jpeg,   disk_jpeg_len );
IconSrc *TextCounters_entries_src = new IconSrc( ghost_jpeg,  ghost_jpeg_len );
IconSrc *TextCounters_last_src    = new IconSrc( earth_jpeg,  earth_jpeg_len );
IconSrc *TextCounters_seen_src    = new IconSrc( insert_jpeg, insert_jpeg_len );
IconSrc *TextCounters_scans_src   = new IconSrc( moai_jpeg,   moai_jpeg_len );
IconSrc *TextCounters_uptime_src  = new IconSrc( ram_jpeg,    ram_jpeg_len );
IconSrc *TextCountersIcons[]      = { TextCounters_heap_src, TextCounters_entries_src, TextCounters_last_src, TextCounters_seen_src, TextCounters_scans_src, TextCounters_uptime_src };
IconStatus TextCountersStatuses[] = { ICON_STATUS_HEAP, ICON_STATUS_ENTRIES, ICON_STATUS_LAST, ICON_STATUS_SEEN, ICON_STATUS_SCANS, ICON_STATUS_UPTIME };
IconWidget TextCountersWidget;
Icon TextCountersIcon = {
  .name      = "Text counters",
  .width     = 8, // 0 = auto
  .height    = 8, // 0 = auto
  .posX      = 0,
  .posY      = 12,
  .bgcolor   = HEADER_BGCOLOR,
  .statuses  = 6,
  .render    = true,
  .type      = ICON_TYPE_WIDGET,
  .status    = ICON_STATUS_HEAP,
  .states    = TextCountersStatuses,
  .src       = TextCountersIcons,
  .shapes    = NULL,
  .widget    = &TextCountersWidget
};
void TextCountersIconUpdateCB( uint16_t posX, uint16_t posY, uint16_t bgcolor ) {
  TextCountersIcon.posX    = posX;
  TextCountersIcon.posY    = posY;
  TextCountersIcon.bgcolor = bgcolor;
  TextCountersIcon.setRender();
}


IconWidget BLERssiWidget;
Icon BLERssiIcon = {
  .name      = "BLE Global RSSI",
  .width     = 9,
  .height    = 8,
  .posX      = 0,
  .posY      = 0,
  .bgcolor   = HEADER_BGCOLOR,
  .statuses  = 2,
  .render    = true,
  .type      = ICON_TYPE_WIDGET,
  .status    = ICON_STATUS_UNSET,
  .states    = DefaultStatuses,
  .src       = NULL,
  .shapes    = NULL,
  .widget    = &BLERssiWidget
};
void BLERssiIconUpdateCB( uint16_t posX, uint16_t posY, uint16_t bgcolor ) {
  if( posX + posY > 0 ) {
    BLERssiIcon.posX = posX;
    BLERssiIcon.posY = posY;
  }
  BLERssiIcon.setRender();
}

Icon **BLECollectorIconSet = NULL;

IconBar BLECollectorIconBar;

