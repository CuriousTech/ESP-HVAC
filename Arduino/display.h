#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

#define NEX_TIMEOUT  90  // 90 seconds
#define NEX_BRIGHT   95  // 100% = full brightness
#define NEX_MEDIUM   25  // For the clock
#define NEX_DIM       3  // for the lines, 1 = very dim, 0 = off

struct Line{
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
};

union gflags
{
  struct
  {
    uint16_t fan:1;
    uint16_t state:3;
    uint16_t res:2;
    uint16_t rh:10;
  } b;
  uint16_t u;
};

struct gPoint{
  uint32_t time;
  int16_t temp;
  int16_t l;
  int16_t h;
  int16_t ot;
  gflags bits;
};

struct Forecast
{
  uint32_t tm;   // time
  int8_t temp;   // integer temperature value
};

class Display
{
public:
  Display()
  {
  }
  void init(void);
  void oneSec(void);
  bool screen(bool bOn);
  void reset(void);
  void checkNextion(void); // all the Nextion recieved commands
  void updateTemps(void);
  void drawForecast(bool bRef);
  void Note(char *cNote);
  bool getGrapthPoints(gPoint *pt, int n);
  int  minPointVal(int n);
private:
  void buttonRepeat(void);
  void refreshAll(void);
  void updateClock(void);
  void cspoint(float &x2, float &y2, float x, float y, float angle, float size);
  void displayTime(void);
  void displayOutTemp(void);
  void updateModes(void); // update any displayed settings
  void updateAdjMode(bool bRef);  // current adjust indicator of the 4 temp settings
  void updateRSSI(void);
  void updateNotification(bool bRef);
  void updateRunIndicator(bool bForce); // run and fan running
  void addGraphPoints(void);
  void fillGraph(void);
  void drawPoints(int w, uint16_t color);
  void drawPointsRh(uint16_t color);
  void drawPointsTemp(void);
  uint16_t stateColor(gflags v);
  void Lines(void);
  int  tween(int8_t t1, int8_t t2, int m, int r);

  uint16_t m_backlightTimer = NEX_TIMEOUT;
#define GPTS 800 // 320 px width - (10+10) padding
  gPoint m_points[GPTS];
  uint16_t m_pointsIdx;
  uint16_t m_temp_counter = 2*60;
  uint8_t m_btnMode;
  uint8_t m_btnDelay;
public:
#define FC_CNT 64
  Forecast m_fcData[FC_CNT];
  uint8_t  m_adjustMode; // which of 4 temps to adjust with rotary encoder
  bool     m_bUpdateFcst;
  bool     m_bUpdateFcstDone = true;
};

#endif // DISPLAY_H
