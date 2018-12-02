/* ported from https://github.com/niklasekstrom/boing_ball_python/blob/master/boing_ball.py */

struct AmigaBallConfig {
  float BGColor = tft.color565(0x22, 0x22, 0x44);
  float Framelength = 25;
  float ScreenWidth = tft.width();
  float ScreenHeight = tft.height();
  float VCentering = 245.0;
  float Scale = 32.0;
  float Amplitude = 50;
  byte Wires = 5;
} amigaBallConfig;


class AmigaRulez {
  public:

    struct Points {
      float x = 0.00;
      float y = 0.00;
    };
    
    Points points[10][10];
    
    float deg2rad = PI / 180.0;
    float phase8Rad = PI/8; // 22.5 deg
    float phase4Rad = PI/4; // 45 deg
    float phase2Rad = PI/2; // 90 deg
    float AmigaBallScreenWidth;
    float AmigaBallScreenHeight;
    float AmigaBallVCentering;
    float AmigaBallScale;
    float AmigaBallAmplitude;
    float AmigaBallTiltDeg;
    float AmigaBallTiltRad;
    float oldx;
    float oldy;
    float hCenter;
    float canvasVstart;
    float canvasVend;
    float canvasHeight;
    float cVsteps;
    float canvasHstart;
    float canvasHend;
    float canvasWidth;
    float cHsteps;
    float boxVstart;
    float boxVend;
    float boxHeight;
    float vSteps;
    float boxHstart;
    float boxHend;
    float boxWidth;
    float hSteps;
    float vectorx;
    float vectory;
    float xtoyratio;
    float ytoxratio;
    float perspective[4];
    byte bytecounter = 0;
    byte AmigaBallWires;
    uint16_t AmigaBallBGColor;
    int AmigaBallBounceMargin;
    long AmigaBallFramelength;

    void init( AmigaBallConfig conf=amigaBallConfig ) {
      AmigaBallBGColor      = amigaBallConfig.BGColor;//tft.color565(0x22, 0x22, 0x44);
      AmigaBallFramelength  = amigaBallConfig.Framelength;//33; // millis
      AmigaBallScreenWidth  = amigaBallConfig.ScreenWidth;//tft.width(); // 640
      AmigaBallScreenHeight = amigaBallConfig.ScreenHeight;//tft.height(); // 480
      AmigaBallVCentering   = amigaBallConfig.VCentering;// 245.0;
      AmigaBallScale        = amigaBallConfig.Scale;// 32.0;
      AmigaBallAmplitude    = amigaBallConfig.Amplitude;// 50;
      AmigaBallWires        = amigaBallConfig.Wires;
      setupValues();
    }

    void setupValues() {
      AmigaBallBounceMargin = AmigaBallScale+8; // 135
      AmigaBallTiltDeg = 17.0; // 17 degrees tilting to the right
      AmigaBallTiltRad = AmigaBallTiltDeg * deg2rad;
      oldx = 0;
      oldy = 0;
      hCenter = AmigaBallScreenWidth / 2;
      canvasVstart = AmigaBallVCentering - (AmigaBallScale + AmigaBallAmplitude );
      canvasVend   = AmigaBallVCentering + (AmigaBallScale );
      canvasHeight = canvasVend - canvasVstart;
      cVsteps      = canvasHeight / AmigaBallWires;
      canvasHstart = 0;
      canvasHend   = AmigaBallScreenWidth;
      canvasWidth  = canvasHend - canvasHstart;
      cHsteps      = canvasWidth / AmigaBallWires;
      boxVstart = AmigaBallVCentering - (AmigaBallScale/4 + AmigaBallAmplitude );
      boxVend   = AmigaBallVCentering + (AmigaBallScale/4 );
      boxHeight = boxVend - boxVstart;
      vSteps    = boxHeight / AmigaBallWires;
      boxHstart = hCenter - (AmigaBallScale + AmigaBallAmplitude );
      boxHend   = hCenter + (AmigaBallScale + AmigaBallAmplitude );
      boxWidth  = boxHend - boxHstart;
      hSteps    = boxWidth / AmigaBallWires;
      vectorx = boxHstart-canvasHstart;
      vectory = boxVstart-canvasVstart;
      xtoyratio = vectorx / vectory;
      ytoxratio = vectory / vectorx;
      perspective[0] = 0;
      perspective[1] = AmigaBallScale/8;
      perspective[2] = AmigaBallScale/4;
      perspective[3] = AmigaBallScale/2;
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
        float y = sin( lon );
        float l = cos( lon );
        for(int i=0;i<10;i++) {
          float x = sin_lat[i] * l;
          points[i][j].x = x;
          points[i][j].y = y;
        }
      }
    }

    void tiltSphere(float ang) {
      float st = sin( ang );
      float ct = cos( ang );
      for( int i=0; i<10; i++) {
        for( int j=0; j<9; j++) {
          float x = points[i][j].x * ct - points[i][j].y * st;
          float y = points[i][j].x * st + points[i][j].y * ct;
          points[i][j].x = x;
          points[i][j].y = y;
        }
      }
    }

    float scaleTranslate(float s, float tx, float ty) {
      for( int i=0; i<10; i++) {
        for( int j=0; j<9; j++ ) {
          float x = points[i][j].x * s + tx;
          float y = points[i][j].y * s + ty;
          points[i][j].x = x;
          points[i][j].y = y;
        }
      }
    }

    void transform(float s, float tx, float ty) {
      tiltSphere( AmigaBallTiltRad );
      scaleTranslate( s, tx, ty );
    }

    void fillTiles(bool alter) {
      for( int j=0; j<8; j++ ) {
        for( int i=0; i<9; i++) {
          Points p1 = points[i][j];
          Points p2 = points[i+1][j];
          Points p3 = points[i+1][j+1];
          Points p4 = points[i][j+1];
          uint16_t color = alter ? WROVER_RED : WROVER_WHITE;
          tft.fillTriangle(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, color);
          //tft.fillTriangle(p2.x, p2.y, p3.x, p3.y, p4.x, p4.y, color);
          tft.fillTriangle(p3.x, p3.y, p4.x, p4.y, p1.x, p1.y, color);
          //tft.fillTriangle(p4.x, p4.y, p1.x, p1.y, p2.x, p2.y, color);
          alter = !alter;
        }
      }
    }

    void drawWireFrame() {
      for( int i=0; i<=AmigaBallWires; i++ ) {
        tft.drawFastHLine(boxHstart, boxVstart + i*vSteps, boxWidth, WROVER_PURPLE);
        tft.drawLine(boxHstart, boxVstart + i*vSteps, canvasHstart, canvasVstart + i*cVsteps, WROVER_PURPLE);
        tft.drawLine(boxHend,   boxVstart + i*vSteps, canvasHend,   canvasVstart + i*cVsteps, WROVER_PURPLE);
        tft.drawFastVLine(boxHstart + i*hSteps, boxVstart, boxHeight, WROVER_PURPLE);
        tft.drawLine(boxHstart + i*hSteps, boxVstart, canvasHstart + i*cHsteps, canvasVstart, WROVER_PURPLE);
        tft.drawLine(boxHstart + i*hSteps, boxVend, canvasHstart + i*cHsteps, canvasVend, WROVER_PURPLE);
      }
      for( int i=0; i<4; i++ ) {
        float y = perspective[i]+vSteps/2;
        float x =  ((AmigaBallScale/2) - (y * ytoxratio))*2;
        tft.drawFastHLine(x, boxVstart - y, canvasWidth-(x*2), WROVER_PURPLE);
        tft.drawFastHLine(x, boxVend   + y, canvasWidth-(x*2), WROVER_PURPLE);
        float boxH = (boxVend   + y) - (boxVstart - y);
        tft.drawFastVLine(x,             boxVstart - y, boxH, WROVER_PURPLE);
        tft.drawFastVLine(canvasWidth-x, boxVstart - y, boxH, WROVER_PURPLE);
      }
    }

    void clearCrescent(float r0, float r1, float x0, float y0, float x1, float y1) {
      float vectorx = x1 - x0;
      float vectory = y1 - y0;
      float vectAngleRadians = atan2(vectory, vectorx); // angle in radians
      float d = sqrt( vectorx*vectorx + vectory*vectory );
      // https://stackoverflow.com/questions/3349125/circle-circle-intersection-points
      if( d > r0+r1 ) return; // If d > r0 + r1 then there are no solutions, the circles are separate.
      if( d < abs(r0-r1) ) return; // If d < |r0 - r1| then there are no solutions because one circle is contained within the other.
      if( d == 0.0 && r0 == r1 ) return; // If d = 0 and r0 = r1 then the circles are coincident and there are an infinite number of solutions.
      float a = ( ( r0*r0 ) - ( r1*r1 ) + ( d*d ) ) / ( 2*d );
      float h = sqrt( ( r0*r0 ) - ( a*a ) );
      /*
      // circle intersection points (not needed here)
      float x2 = x0 + a * vectorx / d;   
      float y2 = y0 + a * vectory / d;  
      float x3 = x2 + h * vectory / d;       // also x3=x2-h*(y1-y0)/d
      float y3 = y2 - h * vectorx / d;       // also y3=y2+h*(x1-x0)/d
      */
      float mobAngleRadians = atan2(h, a);
      float angleRad0 = vectAngleRadians-mobAngleRadians;
      float angleRad1 = vectAngleRadians+mobAngleRadians;
      if( angleRad0 > angleRad1 ) {
        angleRad0 = vectAngleRadians+mobAngleRadians;
        angleRad1 = vectAngleRadians-mobAngleRadians;
      }
      float angleStep = mobAngleRadians/4.5;
      
      float xx2 = 0;
      float yy2 = 0;
      float xx3 = 0;
      float yy3 = 0;
      
      for( float angle=angleRad0; angle<angleRad1; angle+=angleStep ) {
        float ct = sin(angle);
        float st = cos(angle);
        float xx0 = x0 + st * r0;
        float yy0 = y0 + ct * r0;
        float xx1 = x1 + st * r1;
        float yy1 = y1 + ct * r1;
        if( xx2 != 0 && yy2 != 0 ) {
          tft.fillTriangle(xx0, yy0, xx1, yy1, xx2, yy2, AmigaBallBGColor );
          tft.fillTriangle(xx2, yy2, xx3, yy3, xx1, yy1, AmigaBallBGColor );
        } else {
          tft.drawLine(xx0, yy0, xx1, yy1, AmigaBallBGColor );
        }
        xx2 = xx0;
        yy2 = yy0;
        xx3 = xx1;
        yy3 = yy1;
      }
    }

    void draw(float phase, float scale, float x, float y) {
      calcPoints( fmod(phase, phase8Rad) );
      transform(scale, x, y);
      if(bytecounter++%4==0) drawWireFrame();
      fillTiles(phase >= phase8Rad);
      if(oldx!=0 && oldy!=0) {
        clearCrescent(scale, scale+2, x, y, oldx, oldy);
      }
      oldx = x;
      oldy = y;
    }

    void animate( long duration = 5000 ) {
      bool done = false;
      float phase = 0.0;
      float phase_velocity = 2.5 * deg2rad;
      float x = AmigaBallScreenWidth/2;
      float y;
      float x_velocity = 2.1;
      bool right = true;
      float y_ang = 0.0;
      float y_velocity = 0.07;
      long started = millis();
      long last = millis();
      long processtime = 0;

      while( !done ) {
        last = millis();
        if( right ) {
          phase = fmod( phase + ( phase4Rad - phase_velocity ), phase4Rad );
          x += x_velocity;
        } else {
          phase = fmod( phase + phase_velocity, phase4Rad );
          x -= x_velocity;
        }
        if ( x >= AmigaBallScreenWidth-AmigaBallBounceMargin ) {
          right = false;
        } else if( x <= AmigaBallBounceMargin ) {
          right = true;
        }
        y_ang = fmod( y_ang + y_velocity, 2*PI );
        y = AmigaBallVCentering - AmigaBallAmplitude * fabs( cos( y_ang ) );
        draw( phase, AmigaBallScale, x, y );
        processtime = millis() - last;
        if( processtime < AmigaBallFramelength ) {
          delay( AmigaBallFramelength - processtime );
        }
        if( millis() - started > duration ) {
          tft.fillRect( 0, canvasVstart, AmigaBallScreenWidth, canvasHeight, AmigaBallBGColor );
          done = true;
        }
      }
    }

};


AmigaRulez AmigaBall;
