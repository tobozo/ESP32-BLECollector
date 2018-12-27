/*

  ESP32 Amigaball - A port of the famous Amiga Boing Ball Demo
  ported from https://github.com/niklasekstrom/boing_ball_python/

  MIT License

  Copyright (c) 2018 tobozo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  -----------------------------------------------------------------------------

*/




struct AmigaBallConfig {
  long Framelength = 25;
  byte Wires = 0; // 0 = no wireframes
  uint16_t BGColor = tft.color565(0x22, 0x22, 0x44);
  uint16_t ScreenWidth = tft.width();
  uint16_t ScreenHeight = tft.height();
  uint16_t VCentering = 245;
  uint16_t Scale = 32;
  uint16_t Amplitude = 50;
} amigaBallConfig;


class AmigaRulez {
  public:

    struct Points {
      float x = 0.00;
      float y = 0.00;
    };

    Points points[10][10];

    float deg2rad   = PI/180.0;
    float phase8Rad = PI/8.0; // 22.5 deg
    float phase4Rad = PI/4.0; // 45 deg
    float phase2Rad = PI/2.0; // 90 deg
    float twopi     = PI*2;
    float Phase     = 0.0;
    float velocityX = 2.1;
    float velocityY = 0.07;
    float angleY    = 0.0;
    
    float PhaseVelocity;
    float perspective[4];
    float XtoYratio;
    float YtoXratio;
    float TiltRad;

    bool AnimationDone;
    bool isMovingRight;
    bool doWireFrame = false;
    
    byte Wires;
    byte bytecounter = 0;

    int BounceMargin;

    long Framelength;
    long startedTick = millis();
    long lastTick    = millis();
    long processTicks = 0;

    uint16_t TiltDeg = 17; // 17 degrees tilting to the right
    uint16_t ScreenWidth;
    uint16_t ScreenHeight;
    uint16_t VCentering;
    uint16_t Scale;
    uint16_t Amplitude;
    uint16_t BGColor;
    uint16_t lastPositionX;
    uint16_t lastPositionY;
    uint16_t hCenter;
    uint16_t canvasVstart;
    uint16_t canvasVend;
    uint16_t canvasHeight;
    uint16_t cVsteps;
    uint16_t canvasHstart = 0;
    uint16_t canvasHend;
    uint16_t canvasWidth;
    uint16_t cHsteps;
    uint16_t boxVstart;
    uint16_t boxVend;
    uint16_t boxHeight;
    uint16_t vSteps;
    uint16_t boxHstart;
    uint16_t boxHend;
    uint16_t boxWidth;
    uint16_t hSteps;
    uint16_t vectorX;
    uint16_t vectorY;
    uint16_t positionX;
    uint16_t positionY;

    void init( AmigaBallConfig conf=amigaBallConfig ) {
      BGColor      = amigaBallConfig.BGColor;//tft.color565(0x22, 0x22, 0x44);
      Framelength  = amigaBallConfig.Framelength;//33; // millis
      ScreenWidth  = amigaBallConfig.ScreenWidth;//tft.width(); // 640
      ScreenHeight = amigaBallConfig.ScreenHeight;//tft.height(); // 480
      VCentering   = amigaBallConfig.VCentering;// 245.0;
      Scale        = amigaBallConfig.Scale;// 32.0;
      Amplitude    = amigaBallConfig.Amplitude;// 50;
      Wires        = amigaBallConfig.Wires;
      setupValues();
    }

    void setupValues() {
      BounceMargin = Scale+8; // 135
      TiltRad = TiltDeg * deg2rad;
      lastPositionX = 0;
      lastPositionY = 0;
      hCenter = ScreenWidth / 2;
      canvasVstart = VCentering - (Scale + Amplitude );
      canvasVend   = VCentering + (Scale );
      canvasHeight = canvasVend - canvasVstart;
      canvasHend   = ScreenWidth;
      canvasWidth  = canvasHend - canvasHstart;
      boxVstart = VCentering - (Scale/4 + Amplitude );
      boxVend   = VCentering + (Scale/4 );
      boxHeight = boxVend - boxVstart;
      boxHstart = hCenter - (Scale + Amplitude );
      boxHend   = hCenter + (Scale + Amplitude );
      boxWidth  = boxHend - boxHstart;
      if( Wires > 0 ) {
        doWireFrame = true;
        cVsteps = canvasHeight / Wires;
        cHsteps = canvasWidth / Wires;
        vSteps  = boxHeight / Wires;
        hSteps  = boxWidth / Wires;
      } else {
        doWireFrame = false;
      }
      vectorX = boxHstart-canvasHstart;
      vectorY = boxVstart-canvasVstart;
      XtoYratio = vectorX / vectorY;
      YtoXratio = vectorY / vectorX;
      perspective[0] = 0;
      perspective[1] = Scale/8;
      perspective[2] = Scale/4;
      perspective[3] = Scale/2;
      PhaseVelocity = 2.5 * deg2rad;
      positionX = ScreenWidth/2;
      positionY;
      isMovingRight = true;
    }

    float getLat(float phase, int i) {
      if(i == 0) {
        return -phase2Rad;
      } else if(i == 9) {
        return phase2Rad;
      } else {
        return -phase2Rad + phase + (i-1) * phase8Rad;
      }
    }

    void calcPoints(float phase) {
      float sin_lat[10] = {0};// = {}
      for(int i=0;i<10;i++) {
        float lat = getLat(phase, i);
        sin_lat[i] = sin( lat );
      }
      for(int j=0;j<9;j++) {
        float lon = -phase2Rad + j * phase8Rad;
        float _y = sin( lon );
        float _l = cos( lon );
        for(int i=0;i<10;i++) {
          float _x = sin_lat[i] * _l;
          points[i][j].x = _x;
          points[i][j].y = _y;
        }
      }
    }

    void tiltSphere(float ang) {
      float st = sin( ang );
      float ct = cos( ang );
      for( int i=0; i<10; i++) {
        for( int j=0; j<9; j++) {
          float _x = points[i][j].x * ct - points[i][j].y * st;
          float _y = points[i][j].x * st + points[i][j].y * ct;
          points[i][j].x = _x;
          points[i][j].y = _y;
        }
      }
    }

    float scaleTranslate(float s, uint16_t tx, uint16_t ty) {
      for( int i=0; i<10; i++) {
        for( int j=0; j<9; j++ ) {
          float _x = points[i][j].x * s + tx;
          float _y = points[i][j].y * s + ty;
          points[i][j].x = _x;
          points[i][j].y = _y;
        }
      }
    }

    void transform(float s, uint16_t tx, uint16_t ty) {
      tiltSphere( TiltRad );
      scaleTranslate( s, tx, ty );
    }

    void fillTiles(bool alter) {
      for( int j=0; j<8; j++ ) {
        for( int i=0; i<9; i++) {
          uint16_t color = alter ? BLE_RED : BLE_WHITE;
          tft.fillTriangle(points[i][j].x,     points[i][j].y,     points[i+1][j].x, points[i+1][j].y, points[i+1][j+1].x, points[i+1][j+1].y, color);
          tft.fillTriangle(points[i+1][j+1].x, points[i+1][j+1].y, points[i][j+1].x, points[i][j+1].y, points[i][j].x,     points[i][j].y, color);
          alter = !alter;
        }
      }
    }

    void drawWireFrame() {
      for( int i=0; i<=Wires; i++ ) {
        tft.drawFastHLine(boxHstart, boxVstart + i*vSteps, boxWidth, BLE_PURPLE);
        tft.drawLine(boxHstart, boxVstart + i*vSteps, canvasHstart, canvasVstart + i*cVsteps, BLE_PURPLE);
        tft.drawLine(boxHend,   boxVstart + i*vSteps, canvasHend,   canvasVstart + i*cVsteps, BLE_PURPLE);
        tft.drawFastVLine(boxHstart + i*hSteps, boxVstart, boxHeight, BLE_PURPLE);
        tft.drawLine(boxHstart + i*hSteps, boxVstart, canvasHstart + i*cHsteps, canvasVstart, BLE_PURPLE);
        tft.drawLine(boxHstart + i*hSteps, boxVend,   canvasHstart + i*cHsteps, canvasVend, BLE_PURPLE);
      }
      for( int i=0; i<4; i++ ) {
        uint16_t  _y = perspective[i]+vSteps/2;
        uint16_t  _x =  ((Scale/2) - (_y * YtoXratio))*2;
        tft.drawFastHLine(_x, boxVstart - _y, canvasWidth-(_x*2), BLE_PURPLE);
        tft.drawFastHLine(_x, boxVend   + _y, canvasWidth-(_x*2), BLE_PURPLE);
        uint16_t  boxH = (boxVend   + _y) - (boxVstart - _y);
        tft.drawFastVLine(_x,               boxVstart - _y, boxH, BLE_PURPLE);
        tft.drawFastVLine(canvasWidth - _x, boxVstart - _y, boxH, BLE_PURPLE);
      }
    }

    void clearCrescent(float r0, float r1, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
      float vectorX = x1 - x0;
      float vectorY = y1 - y0;
      float vectAngleRadians = atan2(vectorY, vectorX); // angle in radians
      float d = sqrt( vectorX*vectorX + vectorY*vectorY ); // hypothenuse length
      // https://stackoverflow.com/questions/3349125/circle-circle-intersection-points
      if( d > r0+r1 ) return; // If d > r0 + r1 then there are no solutions, the circles are separate.
      if( d < abs(r0-r1) ) return; // If d < |r0 - r1| then there are no solutions because one circle is contained within the other.
      if( d == 0.0 && r0 == r1 ) return; // If d = 0 and r0 = r1 then the circles are coincident and there are an infinite number of solutions.
      float a = ( ( r0*r0 ) - ( r1*r1 ) + ( d*d ) ) / ( 2*d );
      float h = sqrt( ( r0*r0 ) - ( a*a ) );

      // circle intersection points (only the first one is needed here)
      uint16_t xx2 = x0 + a * vectorX / d;
      uint16_t yy2 = y0 + a * vectorY / d;
      uint16_t xx3 = 0; // x2 + h * vectorY / d;  // also x3=x2-h*(y1-y0)/d;
      uint16_t yy3 = 0; // y2 - h * vectorX / d;  // also y3=y2+h*(x1-x0)/d;
      
      float mobAngleRadians = atan2(h, a);
      float angleRad0 = vectAngleRadians-mobAngleRadians;
      float angleRad1 = vectAngleRadians+mobAngleRadians;
      if( angleRad0 > angleRad1 ) {
        angleRad0 = vectAngleRadians+mobAngleRadians;
        angleRad1 = vectAngleRadians-mobAngleRadians;
      }
      float angleStep = mobAngleRadians/4.5;
      
      for( float angle=angleRad0; angle<angleRad1; angle+=angleStep ) {
        float ct = sin(angle);
        float st = cos(angle);
        uint16_t xx0 = x0 + st * r0;
        uint16_t yy0 = y0 + ct * r0;
        uint16_t xx1 = x1 + st * r1;
        uint16_t yy1 = y1 + ct * r1;
        if( xx2 != 0 && yy2 != 0 ) {
          tft.fillTriangle(xx0, yy0, xx1, yy1, xx2, yy2, BGColor );
        }
        if( xx3!= 0 && xx3 != 0 ) {
          tft.fillTriangle(xx2, yy2, xx3, yy3, xx1, yy1, BGColor );
        }
        /*
        {
          tft.drawLine(xx0, yy0, xx1, yy1, BGColor );
        }
        */
        xx2 = xx0;
        yy2 = yy0;
        xx3 = xx1;
        yy3 = yy1;
      }
    }

    void draw(float phase, float scale, uint16_t x, uint16_t y) {
      calcPoints( fmod(phase, phase8Rad) );
      transform(scale, x, y);
      if(doWireFrame && bytecounter++%8==0) drawWireFrame();
      fillTiles(phase >= phase8Rad);
      if(lastPositionX!=0 && lastPositionY!=0) {
        clearCrescent(scale, scale+2, x, y, lastPositionX, lastPositionY);
      }
      lastPositionX = x;
      lastPositionY = y;
    }

    void animate( long duration = 5000, bool clearAfter = true ) {

      AnimationDone = false;
      startedTick = millis();
      lastTick = millis();
      processTicks = 0;

      while( !AnimationDone ) {
        lastTick = millis();
        if( isMovingRight ) {
          Phase = fmod( Phase + ( phase4Rad - PhaseVelocity ), phase4Rad );
          positionX += velocityX;
        } else {
          Phase = fmod( Phase + PhaseVelocity, phase4Rad );
          positionX -= velocityX;
        }
        if ( positionX >= ScreenWidth-BounceMargin ) {
          isMovingRight = false;
        } else if( positionX <= BounceMargin ) {
          isMovingRight = true;
        }
        angleY = fmod( angleY + velocityY, twopi );
        positionY = VCentering - Amplitude * fabs( cos( angleY ) );
        draw( Phase, Scale, positionX, positionY );
        processTicks = millis() - lastTick;
        if( processTicks < Framelength ) {
          delay( Framelength - processTicks );
        }
        if( millis() - startedTick > duration ) {
          if( clearAfter ) {
            tft.fillRect( 0, canvasVstart, ScreenWidth, canvasHeight, BGColor );
          }
          AnimationDone = true;
        }
      }
    }

};


AmigaRulez AmigaBall;
