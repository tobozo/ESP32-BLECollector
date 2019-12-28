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

extern const int AMIGABALL_YPOS;


struct AmigaBallConfig {
  long Framelength = 25;
  byte Wires       = 0; // 0 = no wireframes
  uint16_t BGColor = tft_color565(0x22, 0x22, 0x44);
  uint16_t YPos    = AMIGABALL_YPOS;
  uint16_t XPos    = 0;
  uint16_t Width   = scrollpanel_width();
  uint16_t Height  = 132;
  uint16_t Red     = tft_color565(0xf8, 0x00, 0x00);
  uint16_t White   = tft_color565(0xff, 0xff, 0xff);

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

    byte Wires;
    byte bytecounter = 0;

    int BounceMargin;

    long Framelength;
    long startedTick = millis();
    long lastTick    = millis();
    long processTicks = 0;

    float variableScale = Scale;
    float oldScale = Scale;
    float ScaleAmplitude = 8;
    float MaxScaleAmplitude;
    float TiltDeg = 17; // 17 degrees tilting to the right
    float LeftBoundary;
    float RightBoundary;
    float XPos;
    float YPos;
    float Width;
    float Height;

    float VCentering;
    float Scale;
    float YPosAmplitude;
    uint16_t BGColor;
    uint16_t Red;
    uint16_t White;
    float lastPositionX;
    float lastPositionY;
    float positionX;
    float positionY;

    void init( AmigaBallConfig conf=amigaBallConfig ) {
      BGColor        = amigaBallConfig.BGColor;//tft_color565(0x22, 0x22, 0x44);
      Framelength    = amigaBallConfig.Framelength;//33; // millis
      XPos   = amigaBallConfig.XPos;
      YPos   = amigaBallConfig.YPos;
      Width  = amigaBallConfig.Width;
      Height = amigaBallConfig.Height;
      Red    = amigaBallConfig.Red;
      White  = amigaBallConfig.White;
      setupValues();
      tft.fillRect( XPos, YPos, Width, Height, BGColor );
    }

    void setupValues() {
      Scale = Height/5;// 
      ScaleAmplitude = Scale/3; // ball diameter will vary on this
      MaxScaleAmplitude = Scale + ScaleAmplitude;
      YPosAmplitude = (Height-(Scale+ScaleAmplitude))/2; // ball will bounce on this span pixels
      VCentering = YPos + Height - MaxScaleAmplitude;// -(YPosAmplitude/2 + Scale + ScaleAmplitude);

      BounceMargin = 4+Scale+ScaleAmplitude; // 135
      LeftBoundary = XPos + BounceMargin;
      RightBoundary = XPos + Width - BounceMargin;

      TiltRad = TiltDeg * deg2rad;
      lastPositionX = 0;
      lastPositionY = 0;
      PhaseVelocity = 2.5 * deg2rad;
      positionX = XPos + Width/2;
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

    void scaleTranslate(float s, float tx, float ty) {
      for( int i=0; i<10; i++) {
        for( int j=0; j<9; j++ ) {
          float _x = points[i][j].x * s + tx;
          float _y = points[i][j].y * s + ty;
          points[i][j].x = _x;
          points[i][j].y = _y;
        }
      }
    }

    void transform(float s, float tx, float ty) {
      tiltSphere( TiltRad );
      scaleTranslate( s, tx, ty );
    }

    void fillTiles(bool alter) {
      for( int j=0; j<8; j++ ) {
        for( int i=0; i<9; i++) {
          uint16_t color = alter ? Red : White;
          tft.fillTriangle(points[i][j].x,     points[i][j].y,     points[i+1][j].x, points[i+1][j].y, points[i+1][j+1].x, points[i+1][j+1].y, color);
          tft.fillTriangle(points[i+1][j+1].x, points[i+1][j+1].y, points[i][j+1].x, points[i][j+1].y, points[i][j].x,     points[i][j].y, color);
          alter = !alter;
        }
      }
    }

    void clearCrescent(float r0, float r1, float x0, float y0, float x1, float y1) {
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

      // circle intersection points
      float xx2 = 0; // x0 + a * vectorX / d;
      float yy2 = 0; // y0 + a * vectorY / d;
      float xx3 = 0; // x2 + h * vectorY / d;  // also x3=x2-h*(y1-y0)/d;
      float yy3 = 0; // y2 - h * vectorX / d;  // also y3=y2+h*(x1-x0)/d;

      float mobAngleRadians = atan2(h, a);
      float angleRad0 = vectAngleRadians-mobAngleRadians;
      float angleRad1 = vectAngleRadians+mobAngleRadians;
      if( angleRad0 > angleRad1 ) {
        angleRad0 = vectAngleRadians+mobAngleRadians;
        angleRad1 = vectAngleRadians-mobAngleRadians;
      }
      float angleStep = mobAngleRadians/8;

      for( float angle=angleRad0-angleStep; angle<angleRad1+angleStep; angle+=angleStep ) {
        float ct = sin(angle);
        float st = cos(angle);
        float xx0 = x0 + st * r0;
        float yy0 = y0 + ct * r0;
        float xx1 = x1 + st * r1;
        float yy1 = y1 + ct * r1;
        if( xx2 != 0 && yy2 != 0 ) {
          tft.fillTriangle(xx0, yy0, xx1, yy1, xx2, yy2, BGColor );
        }
        if( xx3!= 0 && xx3 != 0 ) {
          tft.fillTriangle(xx2, yy2, xx3, yy3, xx1, yy1, BGColor );
        }
        xx2 = xx0;
        yy2 = yy0;
        xx3 = xx1;
        yy3 = yy1;
      }
    }

    void draw(float phase, float scale, float oldscale, float x, float y) {
      calcPoints( fmod(phase, phase8Rad) );
      transform(scale, x, y);
      if(lastPositionX!=0 && lastPositionY!=0) {
        clearCrescent(scale, oldscale+1, x, y, lastPositionX, lastPositionY);
      }
      fillTiles(phase >= phase8Rad);
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
        if ( positionX >= RightBoundary /*ScreenWidth-BounceMargin*/ ) {
          isMovingRight = false;
        } else if( positionX <= LeftBoundary /*BounceMargin*/ ) {
          isMovingRight = true;
        }
        angleY = fmod( angleY + velocityY, twopi );
        float absCosAngleY = fabs( cos( angleY ) );
        variableScale = Scale + ScaleAmplitude * absCosAngleY;
        positionY = VCentering - YPosAmplitude * absCosAngleY;
        takeMuxSemaphore();
        draw( Phase, variableScale, oldScale, positionX, positionY );
        giveMuxSemaphore();
        oldScale = variableScale;
        processTicks = millis() - lastTick;
        if( processTicks < Framelength ) {
          delay( Framelength - processTicks );
        }
        if( millis() - startedTick > duration ) {
          if( clearAfter ) {
            takeMuxSemaphore();
            tft.fillRect( XPos, YPos, Width, Height, BGColor );
            giveMuxSemaphore();
          }
          AnimationDone = true;
        }
      }
    }

};


AmigaRulez AmigaBall;
