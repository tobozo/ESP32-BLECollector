/*\
 * UI Icons
\*/


#include "Assets.h" // bitmaps


enum IconSrcStatusType {
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
  ICON_STATUS_filter,
  ICON_STATUS_filter_unset,
  ICON_STATUS_disk,
  ICON_STATUS_ghost,
  ICON_STATUS_earth,
  ICON_STATUS_insert,
  ICON_STATUS_moai,
  ICON_STATUS_ram,
  ICON_STATUS_clock,
  ICON_STATUS_clock2,
  ICON_STATUS_clock3,
  ICON_STATUS_zzz,
  ICON_STATUS_update,
  ICON_STATUS_service,
  ICON_STATUS_espressif,
  ICON_STATUS_apple16,
  ICON_STATUS_crosoft,
  ICON_STATUS_generic,
  ICON_STATUS_nic16,
  ICON_STATUS_ibm8,
  ICON_STATUS_speaker,
  ICON_STATUS_name,
  ICON_STATUS_BLECollector,
  ICON_STATUS_ble,
  ICON_STATUS_db,
  ICON_STATUS_tbz,
  ICON_STATUS_disk00,
  ICON_STATUS_disk01,
  ICON_STATUS_gps,
  ICON_STATUS_nogps,
  ICON_STATUS_DISABLED
};

enum IconType {
  ICON_TYPE_JPG,
  ICON_TYPE_GEOMETRIC,
  ICON_TYPE_WIDGET
};

enum IconShapeType {
  ICON_SHAPE_NONE,
  ICON_SHAPE_DISC,
  ICON_SHAPE_CIRCLE,
  ICON_SHAPE_SQUARE,
  ICON_SHAPE_TRIANGLE
};

enum IconWidgetType {
  ICON_WIDGET_NONE,
  ICON_WIDGET_RSSI,
  ICON_WIDGET_PERCENT,
  ICON_WIDGET_TEXT
};

struct Icon;
struct IconSrc;
struct IconWidget;
struct IconShape;

struct IconSrcStatus;
struct IconWidgetStatus;
struct IconShapeStatus;

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


struct IconSrcStatus {
  IconSrcStatusType status;
  IconSrc           *src;
  IconSrcStatus( IconSrcStatusType s, IconSrc *sr ) : status(s), src(sr) { };
  IconSrcStatus( IconSrc *sr, IconSrcStatusType s ) : status(s), src(sr) { };
};

struct IconWidgetStatus {
  IconWidget        *widget;
  IconSrcStatusType status;
  IconWidgetStatus( IconWidget *w, IconSrcStatusType s ) : widget(w), status(s) { };
  IconWidgetStatus( IconSrcStatusType s, IconWidget *w ) : widget(w), status(s) { };
};

struct IconShapeStatus {
  IconShape         *shape;
  IconSrcStatusType status;
  IconShapeStatus( IconShape *s, IconSrcStatusType st ) : shape(s), status(st) { };
  IconShapeStatus( IconSrcStatusType st, IconShape *s ) : shape(s), status(st) { };
};

struct IconSrc {
  const unsigned char *jpeg;
  size_t   jpeg_len;
  uint16_t width;
  uint16_t height;
  IconSrc(const unsigned char *j, size_t l, uint16_t w, uint16_t h) : jpeg{j}, jpeg_len{l}, width{w}, height{h} { };
};


struct IconWidget {
  IconWidgetType type;
  int32_t        value;
  char           *text;
  uint16_t       color;
  uint8_t        align;
  void (*cb)( uint16_t posX, uint16_t posY, uint16_t bgcolor );
  void setValue( int32_t val ) {
    if( value != val ) {
      value = val;
      if( cb ) cb( 0, 0, 0 );
    }
  }
  void setText( char* intext, uint16_t posX, uint16_t posY, uint16_t textcolor, uint16_t bgcolor, uint8_t textalign ) {
    text  = intext;
    color = textcolor;
    align = textalign;
    if( cb ) cb( posX, posY, bgcolor );
  }
};


struct IconShape {
  IconShapeType type;
  uint16_t      color;
  IconShape( IconShapeType _type, uint16_t _color ) {
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
  bool        render;   // does it need rendering ?
  IconType          type;     // jpg, geometric, widget, text, hybrid
  IconSrcStatusType status;   // current status
  IconSrcStatus     **srcStatus;    // [optional] jpg images (must match statuses)
  IconShapeStatus   **shapeStatus; // [optional] shapes (must match statuses)
  IconWidgetStatus  **widgetStatus;  // [optional] widgets (must match statuses)
  uint8_t     statuses; // how many different statuses this icon handles
  // constructors
  Icon( const char*_n, uint16_t _w, uint16_t _h, IconType _t, IconSrcStatusType _s, IconSrcStatus **_sr, uint8_t _st )
    :        name{_n}, width{_w},   height{_h},  type{_t},    status{_s},           srcStatus{_sr},      statuses{_st} {
  };
  Icon( const char*_n, uint16_t _w, uint16_t _h, IconType _t, IconSrcStatusType _s, IconShapeStatus **_sh, uint8_t _st )
    :        name{_n}, width{_w},   height{_h},  type{_t},    status{_s},           shapeStatus{_sh},      statuses{_st} {
  };
  Icon( const char*_n, uint16_t _w, uint16_t _h, IconType _t, IconSrcStatusType _s, IconWidgetStatus **_wi, uint8_t _st )
    :        name{_n}, width{_w},   height{_h},  type{_t},    status{_s},           widgetStatus{_wi},      statuses{_st} {
  };
  Icon( const char*_n, uint16_t _w, uint16_t _h, IconType _t, IconSrcStatusType _s, IconSrcStatus **_sr, IconWidgetStatus **_wi, uint8_t _st )
    :        name{_n}, width{_w},  height{_h},   type{_t},    status{_s},           srcStatus{_sr},      widgetStatus{_wi},      statuses{_st} {
  };
  void init() {
    render = true;
    posX = 0;
    posY = 0;
    bgcolor = HEADER_BGCOLOR;
    log_w("Inited icon '%s' [%d*%d] using %d states", name, width, height, statuses);
  }
  void setRender( bool _render = true ) {
    render = _render;
  }
  void setStatus( int8_t _status ) {
    IconSrcStatusType *newstatus = reinterpret_cast<IconSrcStatusType*>(&_status);
    setStatus( *newstatus );
  }
  void setStatus( IconSrcStatusType _status ) {
    bool statusexists = false;
    if( _status == status ) {
      log_v("Status unchanged");
      return;
    }
    for(byte i=0; i<statuses; i++) {
      switch( type ) {
        case ICON_TYPE_JPG:       if( srcStatus[i]->status    == _status) statusexists = true; break;
        case ICON_TYPE_GEOMETRIC: if( shapeStatus[i]->status  == _status) statusexists = true; break;
        case ICON_TYPE_WIDGET:    if( widgetStatus[i]->status == _status) statusexists = true; break;
      }
    }
    if( statusexists ) {
      log_v("Icon '%s' changed status from %d to %d and will be marked for rendering", name, status, _status);
      status = _status;
      setRender();
    } else {
      log_e("Invalid status query for '%s' icon: %d", name, _status);
    }
  }
};


struct IconBar {
  uint16_t    width;
  uint16_t    height;
  uint8_t     margin = 2;
  size_t      totalIcons;
  Icon        **icons;
  void init() {
    if( totalIcons == 0 ) {
      log_e("Nothing to init!!");
      return;
    }
    for( byte i=0; i<totalIcons; i++ ) {
      icons[i]->init();
      if( icons[i]->type == ICON_TYPE_WIDGET && icons[i]->widgetStatus[0]->widget->type == ICON_WIDGET_TEXT ) continue;
      icons[i]->posX = width;
      width+= icons[i]->width + margin;
      if( icons[i]->height > height ) {
        height = icons[i]->height;
      }

    }
    for( byte i=0; i<totalIcons; i++ ) {
      if( icons[i]->type == ICON_TYPE_WIDGET && icons[i]->widgetStatus[0]->widget->type == ICON_WIDGET_TEXT ) continue;
      icons[i]->posY = height/2 - icons[i]->height/2;
    }
    log_w("Iconbar dimensions:[%d*%d] with %d icons", width, height, totalIcons);
  }
  void pushIcon( Icon *icon ) {
    icons = (Icon**)realloc( (Icon**)icons , (totalIcons + 1) * (sizeof(Icon*)));
    icons[totalIcons] = icon;
    totalIcons++;
  }
  void setMargin( uint8_t _margin ) {
    margin = _margin;
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

// renderers
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
    switch( icon->type ) {
      case ICON_TYPE_JPG:       if( icon->srcStatus[i]->status    == icon->status ) statusIndex = i; break;
      case ICON_TYPE_GEOMETRIC: if( icon->shapeStatus[i]->status  == icon->status ) statusIndex = i; break;
      case ICON_TYPE_WIDGET:    if( icon->widgetStatus[i]->status == icon->status ) statusIndex = i; break;
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
      IconRender( icon->srcStatus[statusIndex]->src, offsetX+icon->posX, offsetY+icon->posY );
    break;
    case ICON_TYPE_GEOMETRIC:
      IconRender( icon->shapeStatus[statusIndex]->shape, icon->posX, icon->posY, icon->width, icon->height, offsetX, offsetY );
    break;
    case ICON_TYPE_WIDGET:
      if( icon->srcStatus != NULL && icon->srcStatus[statusIndex] != NULL ) {
        IconRender( icon->srcStatus[statusIndex]->src, icon->posX-icon->width/2, offsetY+icon->posY-icon->height/2 );
      }
      IconRender( icon->widgetStatus[statusIndex]->widget, icon->posX, icon->posY, icon->width, icon->height, offsetX, offsetY, icon->bgcolor );
    break;
    default:
    break;
  }
  giveMuxSemaphore();
  return true;
}

bool IconRender( Icon *icon, IconSrcStatusType status, uint16_t x, uint16_t y ) {
  icon->setStatus( status );
  return IconRender(icon,  x, y );
}

// end struct/enum, begin data

// 8x8 icons
IconSrc *VendorFilterIcon_SET     = new IconSrc( filter_jpeg,            filter_jpeg_len,            10, 8 );
IconSrc *VendorFilterIcon_UNSET   = new IconSrc( filter_unset_jpeg,      filter_unset_jpeg_len,      10, 8 );
IconSrc *TextCounters_heap_src    = new IconSrc( disk_jpeg,              disk_jpeg_len,              8,  8 );
IconSrc *TextCounters_entries_src = new IconSrc( ghost_jpeg,             ghost_jpeg_len,             8,  8 );
IconSrc *TextCounters_last_src    = new IconSrc( earth_jpeg,             earth_jpeg_len,             8,  8 );
IconSrc *TextCounters_seen_src    = new IconSrc( insert_jpeg,            insert_jpeg_len,            8,  8 );
IconSrc *TextCounters_scans_src   = new IconSrc( moai_jpeg,              moai_jpeg_len,              8,  8 );
IconSrc *TextCounters_uptime_src  = new IconSrc( ram_jpeg,               ram_jpeg_len,               8,  8 );
IconSrc *TimeIcon_SET_src         = new IconSrc( clock_jpeg,             clock_jpeg_len,             8,  8 );
IconSrc *TimeIcon_UNSET_src       = new IconSrc( clock3_jpeg,            clock3_jpeg_len,            8,  8 );
IconSrc *TimeIcon_RTC_src         = new IconSrc( clock2_jpeg,            clock2_jpeg_len,            8,  8 );
IconSrc *Icon8x8_zzz_src          = new IconSrc( zzz_jpeg,               zzz_jpeg_len,               8,  8 );
IconSrc *Icon8x8_update_src       = new IconSrc( update_jpeg,            update_jpeg_len,            8,  8 );
IconSrc *Icon8x8_service_src      = new IconSrc( service_jpeg,           service_jpeg_len,           8,  8 );
IconSrc *Icon8x8_espressif_src    = new IconSrc( espressif_jpeg,         espressif_jpeg_len,         8,  8 );
IconSrc *Icon8x8_apple16_src      = new IconSrc( apple16_jpeg,           apple16_jpeg_len,           8,  8 );
IconSrc *Icon8x8_crosoft_src      = new IconSrc( crosoft_jpeg,           crosoft_jpeg_len,           8,  8 );
IconSrc *Icon8x8_generic_src      = new IconSrc( generic_jpeg,           generic_jpeg_len,           8,  8 );
// ?x8 icons
IconSrc *Icon8h_nic16_src         = new IconSrc( nic16_jpeg,             nic16_jpeg_len,             13, 8 );
IconSrc *Icon8h_ibm8_src          = new IconSrc( ibm8_jpg,               ibm8_jpg_len,               20, 8 );
IconSrc *Icon8h_speaker_src       = new IconSrc( speaker_icon_jpg,       speaker_icon_jpg_len,       6,  8 );
IconSrc *Icon8h_name_src          = new IconSrc( name_jpeg,              name_jpeg_len,              7,  8 );
IconSrc *Icon8h_BLECollector_src  = new IconSrc( BLECollector_Title_jpg, BLECollector_Title_jpg_len, 82, 8 );
// ?x? icons
IconSrc *Icon_ble_src             = new IconSrc( ble_jpeg,               ble_jpeg_len,               7,  11 );
IconSrc *Icon_db_src              = new IconSrc( db_jpeg,                db_jpeg_len,                12, 11 );
IconSrc *Icon_tbz_src             = new IconSrc( tbz_28x28_jpg,          tbz_28x28_jpg_len,          28, 28 );
IconSrc *SDLoaderIcon_SET_src     = new IconSrc( disk00_jpg,             disk00_jpg_len,             30, 30 );
IconSrc *SDLoaderIcon_UNSET_src   = new IconSrc( disk01_jpg,             disk01_jpg_len,             30, 30 );
IconSrc *GPSIcon_SET_src          = new IconSrc( gps_jpg,                gps_jpg_len,                10, 10 );
IconSrc *GPSIcon_UNSET_src        = new IconSrc( nogps_jpg,              nogps_jpg_len,              10, 10 );

// shape based icons
IconShape *Shape_BLE_OFF             = new IconShape( ICON_SHAPE_DISC, BLE_DARKBLUE );
IconShape *Shape_BLE_IDLE            = new IconShape( ICON_SHAPE_DISC, BLE_DARKGREY );
IconShape *Shape_BLE_ADV_SCAN        = new IconShape( ICON_SHAPE_DISC, BLUETOOTH_COLOR );
IconShape *Shape_BLE_ADV_WHITELISTED = new IconShape( ICON_SHAPE_CIRCLE, BLE_GREENYELLOW );

IconShape *Shape_ROLE_NONE           = new IconShape( ICON_SHAPE_SQUARE, BLE_DARKORANGE );
IconShape *Shape_ROLE_CLOCK_SHARING  = new IconShape( ICON_SHAPE_SQUARE, BLE_ORANGE );
IconShape *Shape_ROLE_CLOCK_SEEKING  = new IconShape( ICON_SHAPE_SQUARE, BLE_GREENYELLOW );
IconShape *Shape_ROLE_FILE_SHARING   = new IconShape( ICON_SHAPE_SQUARE, BLE_ORANGE );
IconShape *Shape_ROLE_FILE_SEEKING   = new IconShape( ICON_SHAPE_SQUARE, BLUETOOTH_COLOR );

IconShape *Shape_DB_READ             = new IconShape( ICON_SHAPE_TRIANGLE, BLE_GREEN );
IconShape *Shape_DB_WRITE            = new IconShape( ICON_SHAPE_TRIANGLE, BLE_ORANGE );
IconShape *Shape_DB_ERROR            = new IconShape( ICON_SHAPE_TRIANGLE, BLE_RED );
IconShape *Shape_DB_IDLE             = new IconShape( ICON_SHAPE_TRIANGLE, BLE_DARKGREEN );

// { widgets }
IconWidget BLERssiWidget;
IconWidget TextCountersWidget;

// { status + src }
IconSrcStatus IconSrcStatus_filter(        ICON_STATUS_filter,       VendorFilterIcon_SET );
IconSrcStatus IconSrcStatus_filter_unset ( ICON_STATUS_filter_unset, VendorFilterIcon_UNSET );
IconSrcStatus IconSrcStatus_disk(          ICON_STATUS_disk,         TextCounters_heap_src );
IconSrcStatus IconSrcStatus_ghost(         ICON_STATUS_ghost,        TextCounters_entries_src );
IconSrcStatus IconSrcStatus_earth(         ICON_STATUS_earth,        TextCounters_last_src );
IconSrcStatus IconSrcStatus_insert(        ICON_STATUS_insert,       TextCounters_seen_src );
IconSrcStatus IconSrcStatus_moai(          ICON_STATUS_moai,         TextCounters_scans_src );
IconSrcStatus IconSrcStatus_ram(           ICON_STATUS_ram,          TextCounters_uptime_src );
IconSrcStatus IconSrcStatus_clock(         ICON_STATUS_clock,        TimeIcon_SET_src );
IconSrcStatus IconSrcStatus_clock2(        ICON_STATUS_clock2,       TimeIcon_UNSET_src );
IconSrcStatus IconSrcStatus_clock3(        ICON_STATUS_clock3,       TimeIcon_RTC_src );
IconSrcStatus IconSrcStatus_zzz(           ICON_STATUS_zzz,          Icon8x8_zzz_src );
IconSrcStatus IconSrcStatus_update(        ICON_STATUS_update,       Icon8x8_update_src );
IconSrcStatus IconSrcStatus_service(       ICON_STATUS_service,      Icon8x8_service_src );
IconSrcStatus IconSrcStatus_espressif(     ICON_STATUS_espressif,    Icon8x8_espressif_src );
IconSrcStatus IconSrcStatus_apple16(       ICON_STATUS_apple16,      Icon8x8_apple16_src );
IconSrcStatus IconSrcStatus_crosoft(       ICON_STATUS_crosoft,      Icon8x8_crosoft_src );
IconSrcStatus IconSrcStatus_generic(       ICON_STATUS_generic,      Icon8x8_generic_src );
IconSrcStatus IconSrcStatus_nic16(         ICON_STATUS_nic16,        Icon8h_nic16_src );
IconSrcStatus IconSrcStatus_ibm8(          ICON_STATUS_ibm8,         Icon8h_ibm8_src );
IconSrcStatus IconSrcStatus_speaker(       ICON_STATUS_speaker,      Icon8h_speaker_src );
IconSrcStatus IconSrcStatus_name(          ICON_STATUS_name,         Icon8h_name_src );
IconSrcStatus IconSrcStatus_BLECollector(  ICON_STATUS_BLECollector, Icon8h_BLECollector_src );
IconSrcStatus IconSrcStatus_ble(           ICON_STATUS_ble,          Icon_ble_src );
IconSrcStatus IconSrcStatus_db(            ICON_STATUS_db,           Icon_db_src );
IconSrcStatus IconSrcStatus_tbz(           ICON_STATUS_tbz,          Icon_tbz_src );
IconSrcStatus IconSrcStatus_disk00(        ICON_STATUS_disk00,       SDLoaderIcon_SET_src );
IconSrcStatus IconSrcStatus_disk01(        ICON_STATUS_disk01,       SDLoaderIcon_UNSET_src );
IconSrcStatus IconSrcStatus_gps(           ICON_STATUS_gps,          GPSIcon_SET_src );
IconSrcStatus IconSrcStatus_nogps(         ICON_STATUS_nogps,        GPSIcon_UNSET_src );

// { status + shape }
IconShapeStatus Shape_BLE_OFF_status(              Shape_BLE_OFF,             ICON_STATUS_UNSET );
IconShapeStatus Shape_BLE_IDLE_status(             Shape_BLE_IDLE,            ICON_STATUS_IDLE );
IconShapeStatus Shape_BLE_ADV_SCAN_status(         Shape_BLE_ADV_SCAN,        ICON_STATUS_ADV_SCAN );
IconShapeStatus Shape_BLE_ADV_WHITELISTED_status(  Shape_BLE_ADV_WHITELISTED, ICON_STATUS_ADV_WHITELISTED );
IconShapeStatus Shape_ROLE_NONE_status(            Shape_ROLE_NONE,           ICON_STATUS_ROLE_NONE );
IconShapeStatus Shape_ROLE_CLOCK_SHARING_status(   Shape_ROLE_CLOCK_SHARING,  ICON_STATUS_ROLE_CLOCK_SHARING );
IconShapeStatus Shape_ROLE_CLOCK_SEEKING_status(   Shape_ROLE_CLOCK_SEEKING,  ICON_STATUS_ROLE_CLOCK_SEEKING );
IconShapeStatus Shape_ROLE_FILE_SHARING_status(    Shape_ROLE_FILE_SHARING,   ICON_STATUS_ROLE_FILE_SHARING );
IconShapeStatus Shape_ROLE_FILE_SEEKING_status(    Shape_ROLE_FILE_SEEKING,   ICON_STATUS_ROLE_FILE_SEEKING );
IconShapeStatus Shape_DB_READ_status(              Shape_DB_READ,             ICON_STATUS_DB_READ );
IconShapeStatus Shape_DB_WRITE_status(             Shape_DB_WRITE,            ICON_STATUS_DB_WRITE );
IconShapeStatus Shape_DB_ERROR_status(             Shape_DB_ERROR,            ICON_STATUS_DB_ERROR );
IconShapeStatus Shape_DB_IDLE_status(              Shape_DB_IDLE,             ICON_STATUS_DB_IDLE );

// { status + widget }
IconWidgetStatus TextCounter_heap_status(     &TextCountersWidget, ICON_STATUS_HEAP );
IconWidgetStatus TextCounter_entries_status(  &TextCountersWidget, ICON_STATUS_ENTRIES );
IconWidgetStatus TextCounter_last_status(     &TextCountersWidget, ICON_STATUS_LAST );
IconWidgetStatus TextCounter_seen_status(     &TextCountersWidget, ICON_STATUS_SEEN );
IconWidgetStatus TextCounter_scans_status(    &TextCountersWidget, ICON_STATUS_SCANS );
IconWidgetStatus TextCounter_uptime_status(   &TextCountersWidget, ICON_STATUS_UPTIME );
IconWidgetStatus BLERssi_SET_status(          &BLERssiWidget,      ICON_STATUS_SET );
IconWidgetStatus BLERssi_UNSET_status(        &BLERssiWidget,      ICON_STATUS_UNSET );

IconSrcStatus *SDLoaderIconSrcStatuses[]       = { &IconSrcStatus_disk00, &IconSrcStatus_disk01 };
IconSrcStatus *TimeIconsStatuses[]             = { &IconSrcStatus_clock, &IconSrcStatus_clock2, &IconSrcStatus_clock3 };
IconShapeStatus *BLEActivityShapeStatuses[]    = { &Shape_BLE_OFF_status, &Shape_BLE_IDLE_status, &Shape_BLE_ADV_SCAN_status, &Shape_BLE_ADV_WHITELISTED_status };
IconShapeStatus *BLERoleIconStatuses[]         = { &Shape_ROLE_NONE_status, &Shape_ROLE_CLOCK_SHARING_status, &Shape_ROLE_CLOCK_SEEKING_status, &Shape_ROLE_FILE_SHARING_status, &Shape_ROLE_FILE_SEEKING_status };
IconShapeStatus *Shape_DBStatuses[]            = { &Shape_DB_READ_status, &Shape_DB_WRITE_status, &Shape_DB_ERROR_status, &Shape_DB_IDLE_status };
IconSrcStatus *GPSIconSrcStatuses[]            = { &IconSrcStatus_gps, &IconSrcStatus_nogps };
IconSrcStatus *VendorFilterIconSrcStatuses[]   = { &IconSrcStatus_filter, &IconSrcStatus_filter_unset };
IconWidgetStatus *TextCounterWidgetStatuses[]  = { &TextCounter_heap_status, &TextCounter_entries_status, &TextCounter_last_status, &TextCounter_seen_status, &TextCounter_scans_status, &TextCounter_uptime_status };
IconSrcStatus    *TextCounterIconSrcStatuses[] = { &IconSrcStatus_disk,      &IconSrcStatus_ghost,        &IconSrcStatus_earth,     &IconSrcStatus_insert,    &IconSrcStatus_moai,       &IconSrcStatus_ram };
IconWidgetStatus *BLERssiStatuses[]            = { &BLERssi_SET_status, &BLERssi_UNSET_status };

Icon SDLoaderIcon( "Initial load SD status", 30, 30, ICON_TYPE_JPG, ICON_STATUS_disk00, SDLoaderIconSrcStatuses, sizeof SDLoaderIconSrcStatuses / sizeof SDLoaderIconSrcStatuses[0] );
Icon TimeIcon( "Time status Icon", 8, 8, ICON_TYPE_JPG, ICON_STATUS_clock, TimeIconsStatuses, sizeof TimeIconsStatuses / sizeof TimeIconsStatuses[0] );
Icon BLEActivityIcon( "BLE Activity Icon", 8, 8, ICON_TYPE_GEOMETRIC, ICON_STATUS_IDLE, BLEActivityShapeStatuses, sizeof BLEActivityShapeStatuses / sizeof BLEActivityShapeStatuses[0] );
Icon BLERoleIcon( "BLE Role Icon", 6, 6, ICON_TYPE_GEOMETRIC, ICON_STATUS_ROLE_NONE, BLERoleIconStatuses, sizeof BLERoleIconStatuses / sizeof BLERoleIconStatuses[0] );
Icon DBIcon( "DB Icon", 5, 6, ICON_TYPE_GEOMETRIC, ICON_STATUS_DB_IDLE, Shape_DBStatuses, sizeof Shape_DBStatuses / sizeof Shape_DBStatuses[0] );
Icon GPSIcon( "GPS status Icon", 10, 10, ICON_TYPE_JPG, ICON_STATUS_nogps, GPSIconSrcStatuses, sizeof GPSIconSrcStatuses / sizeof GPSIconSrcStatuses[0] );
Icon VendorFilterIcon( "Vendor Filter Icon", 10, 8, ICON_TYPE_JPG, ICON_STATUS_filter_unset, VendorFilterIconSrcStatuses, sizeof VendorFilterIconSrcStatuses / sizeof VendorFilterIconSrcStatuses[0] );
Icon TextCountersIcon( "Text counters", 8, 8, ICON_TYPE_WIDGET, ICON_STATUS_HEAP, TextCounterIconSrcStatuses, TextCounterWidgetStatuses, sizeof TextCounterIconSrcStatuses / sizeof TextCounterIconSrcStatuses[0] );
Icon BLERssiIcon( "BLE Global RSSI", 9, 8, ICON_TYPE_WIDGET, ICON_STATUS_UNSET, BLERssiStatuses, sizeof BLERssiStatuses / sizeof BLERssiStatuses[0] );

void TextCountersIconUpdateCB( uint16_t posX, uint16_t posY, uint16_t bgcolor ) {
  TextCountersIcon.posX    = posX;
  TextCountersIcon.posY    = posY;
  TextCountersIcon.bgcolor = bgcolor;
  TextCountersIcon.setRender();
}

void BLERssiIconUpdateCB( uint16_t posX, uint16_t posY, uint16_t bgcolor ) {
  if( posX + posY > 0 ) {
    BLERssiIcon.posX = posX;
    BLERssiIcon.posY = posY;
  }
  BLERssiIcon.setRender();
}


IconBar BLECollectorIconBar;
