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

struct gPoint{
  uint32_t time;
  uint8_t temp;
  uint8_t rh;
  uint8_t l;
  uint8_t h;
  uint8_t ltemp;
  uint8_t state:2;
  uint8_t fan:1;
  uint8_t res:5;
};

class Display
{
public:
  Display():
    m_pointsIdx(0),
    m_adjustMode(0),
    m_backlightTimer(NEX_TIMEOUT),
    m_temp_counter(10*60)
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
private:
  void refreshAll(void);
  void updateClock(void);
  void displayTime(void);
  void displayOutTemp(void);
  void updateModes(void); // update any displayed settings
  void updateAdjMode(bool bRef);  // current adjust of the 4 temp settings
  void updateRSSI(void);
  void updateNotification(bool bRef);
  void updateRunIndicator(bool bForce); // run and fan running
  void addGraphPoints(void);
  void fillGraph(void);
  void drawPoints(uint8_t *arr, uint16_t color);
  void drawPointsTemp(void);
  uint16_t stateColor(uint8_t v);
  void Lines(void);
  int tween(int8_t t1, int8_t t2, int m, int8_t h);

  uint16_t m_backlightTimer;
#define GPTS 300 // 320 px width + 10 padding
  gPoint m_points[GPTS];
public:
  uint16_t m_pointsIdx;
  int16_t m_updateFcst = 1;
  uint16_t m_temp_counter = 60;
  uint8_t m_adjustMode;
};

#endif // DISPLAY_H
