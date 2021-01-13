// https://www.tindie.com/products/deshipu/x-pad-buttons-shield-for-d1-mini-version-60/
#define XPAD_SDA      26 // pin number
#define XPAD_SCL      27 // pin number
#define XPAD_I2C_ADDR 0x10

#define XPAD_DOWN     0x01
#define XPAD_UP       0x02
#define XPAD_RIGHT    0x04
#define XPAD_LEFT     0x08
#define XPAD_B        0x10
#define XPAD_A        0x20
#define XPAD_C        0x40
#define XPAD_D        0x80


struct XPadButton {
  const uint8_t bitValue; //expected bit returned from I2C when pushed
  uint8_t pressed = 0;
  XPadButton( uint8_t _bitValue ) : bitValue( _bitValue) { }
  uint8_t wasPressed() {
    return pressed;
  }
};


class XPad {
 public:

  uint8_t state; //current button state

  XPadButton *BtnA     = new XPadButton( XPAD_A );
  XPadButton *BtnB     = new XPadButton( XPAD_B );
  XPadButton *BtnC     = new XPadButton( XPAD_C );
  XPadButton *BtnD     = new XPadButton( XPAD_D );
  XPadButton *BtnUP    = new XPadButton( XPAD_UP );
  XPadButton *BtnDOWN  = new XPadButton( XPAD_DOWN );
  XPadButton *BtnRIGHT = new XPadButton( XPAD_RIGHT );
  XPadButton *BtnLEFT  = new XPadButton( XPAD_LEFT );

  void init() {
    Wire.begin(XPAD_SDA, XPAD_SCL);
    _time = millis();
    _lastState = state;
    _changed = 0;
    _hold_time = -1;
    _lastTime = _time;
    _lastChange = _time;
    _pressTime = _time;
    _padVal = 0;
  }

  void setPads() {
    uint8_t pressed = wasPressed();
    if( pressed ) {
      BtnA->pressed     = pressed & BtnA->bitValue;
      BtnB->pressed     = pressed & BtnB->bitValue;
      BtnC->pressed     = pressed & BtnC->bitValue;
      BtnD->pressed     = pressed & BtnD->bitValue;
      BtnUP->pressed    = pressed & BtnUP->bitValue;
      BtnDOWN->pressed  = pressed & BtnDOWN->bitValue;
      BtnRIGHT->pressed = pressed & BtnRIGHT->bitValue;
      BtnLEFT->pressed  = pressed & BtnLEFT->bitValue;
    } else {
      BtnA->pressed     = pressed;
      BtnB->pressed     = pressed;
      BtnC->pressed     = pressed;
      BtnD->pressed     = pressed;
      BtnUP->pressed    = pressed;
      BtnDOWN->pressed  = pressed;
      BtnRIGHT->pressed = pressed;
      BtnLEFT->pressed  = pressed;
    }
  }

  uint8_t update(void) {
    static uint32_t ms;

    ms = millis();
    Wire.requestFrom(XPAD_I2C_ADDR, 1);
    _padVal = Wire.read();

    if (ms - _lastChange < _dbTime) {
      _lastTime = _time;
      _time = ms;
      _changed = 0;
    } else {
      _lastTime = _time;
      _time = ms;
      _lastState = state;
      state = _padVal;
      if (state != _lastState) {
        _lastChange = ms;
        _changed = 1;
        if (state) { _pressTime = _time; }
      } else {
        _changed = 0;
      }
    }

    setPads();
    return state;
  }

  uint8_t wasPressed(void) {
    return state && _changed;
  }


 private:
  uint8_t _padVal;    //value returned by I2C (may contain multiple pushes)
  uint8_t _lastState;     //previous button state
  uint8_t _changed;       //state changed since last read
  uint32_t _time;         //time of current state (all times are in ms)
  uint32_t _lastTime;     //time of previous state
  uint32_t _lastChange;   //time of last state change
  uint32_t _dbTime = 100; //debounce time
  uint32_t _pressTime;    //press time
  uint32_t _hold_time;    //hold time call wasreleasefor

};


static XPad XPadShield;
