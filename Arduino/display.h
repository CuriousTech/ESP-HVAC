#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "Forecast.h"
 
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
  uint32_t u;
  struct
  {
    uint32_t fan:1;
    uint32_t state:3;
    uint32_t rh:10;
    uint32_t tmdiff:9;
    int32_t sens4:9;
  };
};

union temps
{
  uint32_t u;
  struct
  {
    uint32_t inTemp:11;
    uint32_t target:10;
    int32_t  outTemp:11;
  };
};

struct gPoint{
  temps t;
  int8_t sens0;
  int8_t sens1;
  int8_t sens2;
  int8_t sens3;
  gflags bits;
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
  bool drawForecast(bool bRef);
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

  uint16_t m_backlightTimer = NEX_TIMEOUT;
#define GPTS 640 // 320 px width - (10+10) padding
  gPoint m_points[GPTS];
  uint16_t m_pointsIdx = 0;
  uint16_t m_temp_counter = 2*60;
  uint8_t m_btnMode = 0;
  uint8_t m_btnDelay = 0;
  int m_tempLow; // 66.0 base
  int m_tempHigh; // 90.0 top
  int m_tempMax;
public:
  uint32_t m_lastPDate = 0;
  uint8_t m_adjustMode = 0; // which of 4 temps to adjust with rotary encoder/buttons
};

#endif // DISPLAY_H
