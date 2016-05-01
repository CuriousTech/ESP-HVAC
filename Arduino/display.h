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

class Display
{
public:
	Display():
	  m_pointsAdded(0),
	  m_adjustMode(0),
	  m_backlightTimer(NEX_TIMEOUT),
    m_updateFcst(2), // 1-2 minutes from boot
    m_temp_counter(10*60)
  {
  }
  void init(void);
  void oneSec(void);
  bool screen(bool bOn);
  void checkNextion(void); // all the Nextion recieved commands
  void updateTemps(void);
  void drawForecast(bool bRef);
  void Note(char *cNote);
private:
  void refreshAll(void);
  void updateClock(void);
  void displayTime(void);
  void displayOutTemp(void);
  void updateModes(void); // update any displayed settings
  void updateAdjMode(bool bRef);  // current adjust of the 4 temp settings
  void updateRSSI(void);
  void updateNotification(bool bForce);
  void updateRunIndicator(bool bForce); // run and fan running
  void addGraphPoints(void);
  void fillGraph(void);
  void drawPoints(uint8_t arr, uint16_t color);
	void Lines(void);
  int tween(int8_t t1, int8_t t2, int m, int8_t h);

  uint16_t m_backlightTimer;
  uint16_t m_pointsAdded;
  uint8_t m_points[300][5];
public:
  uint16_t m_updateFcst;
  uint16_t m_temp_counter;
  uint8_t m_adjustMode;
};

#endif // DISPLAY_H
