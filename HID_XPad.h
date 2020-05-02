// https://www.tindie.com/products/deshipu/x-pad-buttons-shield-for-d1-mini-version-60/
#define XPAD_SDA      26 // pin number
#define XPAD_SCL      27 // pin number
#define XPAD_I2C_ADDR 0x10


enum XPadButton {
  XPAD_DOWN  = 0x01,
  XPAD_UP    = 0x02,
  XPAD_RIGHT = 0x04,
  XPAD_LEFT  = 0x08,
  XPAD_B     = 0x10,
  XPAD_A     = 0x20,
  XPAD_C     = 0x40,
  XPAD_D     = 0x80
};


struct XPad {

  uint8_t _state;         //current button state
  uint8_t _lastState;     //previous button state
  uint8_t _changed;       //state changed since last read
  uint32_t _time;         //time of current state (all times are in ms)
  uint32_t _lastTime;     //time of previous state
  uint32_t _lastChange;   //time of last state change
  uint32_t _dbTime = 100; //debounce time
  uint32_t _pressTime;    //press time
  uint32_t _hold_time;    //hold time call wasreleasefor


  void init() {
    _time = millis();
    _lastState = _state;
    _changed = 0;
    _hold_time = -1;
    _lastTime = _time;
    _lastChange = _time;
    _pressTime = _time;
    Wire.begin(XPAD_SDA, XPAD_SCL);
  }


  uint8_t read(void) {
    static uint32_t ms;
    static uint8_t pinVal;

    ms = millis();
    Wire.requestFrom(XPAD_I2C_ADDR, 1);
    pinVal = Wire.read();

    if (ms - _lastChange < _dbTime) {
      _lastTime = _time;
      _time = ms;
      _changed = 0;
      return _state;
    }
    else {
      _lastTime = _time;
      _time = ms;
      _lastState = _state;
      _state = pinVal;
      if (_state != _lastState) {
        _lastChange = ms;
        _changed = 1;
        if (_state) { _pressTime = _time; }
      }
      else {
        _changed = 0;
      }
      return _state;
    }
  }

  uint8_t wasPressed(void) {
    return _state && _changed;
  }



};


XPad XPadShield;
