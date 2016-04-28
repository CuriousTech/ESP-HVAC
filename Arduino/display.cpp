#include "display.h"
#include "Nextion.h"
#include "HVAC.h"
#include <Time.h>
#include <ESP8266mDNS.h> // for WiFi.RSSI()
#include "WebHandler.h" // for timeFmt()
#include "event.h"

Nextion nex;
extern HVAC hvac;
extern eventHandler event;

void Display::init()
{
  nex.FFF(); // Just to end any debug strings in the Nextion
  screen( true ); // brighten the screen if it just reset
  refreshAll();
}

// called each second
void Display::oneSec()
{
  updateClock();
  updateRunIndicator(false); // running stuff
  displayTime();    // time update every seconds
  updateModes();    // mode, heat mode, fan mode
  updateTemps(false);    // 
  updateAdjMode(false); // update touched temp settings
  updateNotification(false);
  updateRSSI();     //
  if( m_backlightTimer ) // the dimmer thing
  {
    if(--m_backlightTimer == 0)
        screen(false);
  }

  if(--m_temp_counter <= 0)
  {
    displayOutTemp();
    addGraphPoints();
    m_temp_counter = 5*60;         // update every 5 minutes
  }
}

void Display::checkNextion() // all the Nextion recieved commands
{
  char cBuf[64];
  int len = nex.service(cBuf); // returns just the button value or 0 if no data
  uint8_t btn;
  String s;

  Lines(false); // draw lines at full speed

  if(len == 0)
    return;

  switch(cBuf[0])  // code
  {
    case 0x65: // button
      btn = cBuf[2];

      switch(cBuf[1]) // page
      {
        case 0:     // Thermostat
          switch(btn)
          {
            case 6: // cool hi
            case 7: // cool lo
            case 8: // heat hi
            case 9: // heat lo
              m_adjustMode = btn-6;
              break;
            case 23: // cool hi
            case 24: // cool lo
            case 25: // heat hi
            case 26: // heat lo
              m_adjustMode = btn-23;
              break;

            case 10: // fan
            case 18: // Fan
              hvac.setFan( !hvac.getFan() );
              updateModes(); // faster feedback
              break;
            case 11: // Mode
            case 19: // Mode
              hvac.setMode( (hvac.getSetMode() + 1) & 3 );
              updateModes(); // faster feedback
              break;
            case 12: // Heat
            case 20: // Heat
              hvac.setHeatMode( (hvac.getHeatMode() + 1) % 3 );
              updateModes(); // faster feedback
              break;
            case 13: // notification clear
              hvac.m_notif = Note_None;
              break;
            case 14: // forecast
              nex.setPage("graph");
              fillGraph();
              m_backlightTimer = NEX_TIMEOUT;
              break;
            case 2: // time
              nex.setPage("clock");
              delay(50); // 100 works
              updateClock();
              m_backlightTimer = NEX_TIMEOUT;
              break;
            case 17: // DOW
              break;
          }
          break;
        case 1: // clock
        case 2: // Selection page t1=ID 2 ~ t16=ID 17
        case 3: // Keyboard
        case 4: // graph
        case 5: // empty
          screen(true);
          break;
      }
      break;
    case 0x70:// string return from keyboard
      break;
  }
}

void Display::updateTemps(bool bRef)
{
  if(nex.getPage())
    return;
  static uint16_t last[7];  // only draw changes

  if(last[0] != hvac.m_inTemp)          nex.itemFp(2, last[0] = hvac.m_inTemp);
  if(last[1] != hvac.m_rh)              nex.itemFp(3, last[1] = hvac.m_rh);
  if(last[2] != hvac.m_targetTemp)      nex.itemFp(4, last[2] = hvac.m_targetTemp);
  if(last[3] != hvac.m_EE.coolTemp[1])  nex.itemFp(5, last[3] = hvac.m_EE.coolTemp[1]);
  if(last[4] != hvac.m_EE.coolTemp[0])  nex.itemFp(6, last[4] = hvac.m_EE.coolTemp[0]);
  if(last[5] != hvac.m_EE.heatTemp[1])  nex.itemFp(7, last[5] = hvac.m_EE.heatTemp[1]);
  if(last[6] != hvac.m_EE.heatTemp[0])  nex.itemFp(8, last[6] = hvac.m_EE.heatTemp[0]);
}

const char *_days_short[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// time and dow on main page
void Display::displayTime()
{
  if(nex.getPage()) return;

  static char lastDay = -1;
  nex.itemText(8, timeFmt());

  if(weekday() != lastDay)   // update weekday
  {
    lastDay = weekday();
    nex.itemText(7, _days_short[weekday()-1]);
  }
}

#define Fc_Left     22
#define Fc_Top      29
#define Fc_Width   196
#define Fc_Height   65

void Display::drawForecast(bool bRef)
{
  int8_t min = 126;
  int8_t max = -50;
  int8_t i;

  if(hvac.m_fcData[0].h == -1 || nex.getPage())          // no first run
    return;

  int8_t hrs = ( ((hvac.m_fcData[0].h - hour()) + 1) % 3 ) + 1;   // Set interval to 2, 5, 8, 11..
  int8_t mins = (60 - minute() + 54) % 60;   // mins to :54, retry will be :59

  if(mins > 10 && hrs > 2) hrs--;     // wrong

  m_updateFcst = ((hrs * 60) + mins);
    // Get min/max
  for(i = 0; i < 18; i++)
  {
    int8_t t = hvac.m_fcData[i].t;
    if(min > t) min = t;
    if(max < t) max = t;
  }

  if(min == max) max++;   // div by 0 check

  hvac.updatePeaks(min, max);

  int16_t y = Fc_Top+1;
  int16_t incy = (Fc_Height-4) / 3;
  int16_t dec = (max - min)/3;
  int16_t t = max;
  int16_t x;

  if(bRef) // new forecast
  {
    m_temp_counter = 2; // Todo: just for first points
    nex.refreshItem("t19");
    nex.refreshItem("t20");
    nex.refreshItem("s0");
    delay(5); // 5 works
  }
  
  // temp scale
  for(i = 0; i <= 3; i++)
  {
    nex.text(3, y-6, 18, 0, String(t)); // font height/2=6?
    y += incy;
    t -= dec;
  }

  int8_t day = weekday()-1;              // current day
  int8_t h0 = hour();                    // zeroeth hour
  int8_t pts = hvac.m_fcData[17].h - h0;
  int8_t h;
  int16_t day_x = 0;

  if(pts <= 0) return;                     // error

  for(i = 0, h = h0; i < pts; i++, h++)    // v-lines
  {
    x = Fc_Left + Fc_Width * i / pts;
    if( (h % 24) == 0) // midnight
    {
      nex.line(x, Fc_Top+1, x, Fc_Top+Fc_Height-2, 0xBDF7); // (lighter)
      if(x - 49 > Fc_Left) // fix 1st day too far left
      {
        nex.text(day_x = x - 54, Fc_Top+Fc_Height+1, 26, 1, _days_short[day]);
      }
      if(++day >6) day = 0;
    }
    if( (h % 24) == 12) // noon
    {
      nex.line(x, Fc_Top, x, Fc_Top+Fc_Height, 0x7BEF); // (darker)
    }
  }

  day_x += 84;
  if(day_x < Fc_Left+Fc_Width - (8*3) )  // last partial day
    nex.text(day_x, Fc_Top+Fc_Height+1, 26, 1, _days_short[day]);

  int16_t y2 = Fc_Top+Fc_Height - 1 - (hvac.m_fcData[0].t - min) * (Fc_Height-2) / (max-min);
  int16_t x2 = Fc_Left;

  for(i = 0; i < 18; i++) // should be 18 points
  {
    int y1 = Fc_Top+Fc_Height - 1 - (hvac.m_fcData[i].t - min) * (Fc_Height-2) / (max-min);
    int x1 = Fc_Left + (hvac.m_fcData[i].h - h0) * (Fc_Width) / pts;
    nex.line(x2, y2, x1, y1, 63488);
    x2 = x1;
    y2 = y1;
  }
  displayOutTemp();
}

// get value at current minute between hours
int Display::tween(int8_t t1, int8_t t2, int m, int8_t h)
{
  if(h == 0) h = 1; // div by zero check
  double t = (double)(t2 - t1) * (m * 100 / (60 * h)) / 100;
  return (int)((t + t1) * 10);
}

void Display::displayOutTemp()
{
  if(hvac.m_fcData[0].h == -1) // no read yet
    return;
  int8_t hd = hour() - hvac.m_fcData[0].h;      // hours past 1st value
  int16_t outTemp;

  if(hd < 0)                                    // 1st value is top of next hour
  {
     outTemp = hvac.m_fcData[0].t * 10;         // just use it
  }
  else
  {
     int m = minute();              // offset = hours past + minutes of hour

     if(hd) m += (hd * 60);              // add hours ahead (up to 2)
     outTemp = tween(hvac.m_fcData[0].t, hvac.m_fcData[1].t, m, hvac.m_fcData[1].h - hvac.m_fcData[0].h);
  }

  hvac.updateOutdoorTemp(outTemp);
  if(nex.getPage() == 0)
    nex.itemFp(1, outTemp);
}

void Display::Note(char *cNote)
{
  nex.itemText(12, cNote);
  event.alert(cNote);
}

// update the notification text box
void Display::updateNotification(bool bForce)
{
  static uint8_t note_last = Note_Init; // Needs a clear after startup
  if(!bForce && note_last == hvac.m_notif) // nothing changed
    return;
  note_last = hvac.m_notif;

  String s = "";
  switch(hvac.m_notif)
  {
    case Note_None:
      break;
    case Note_CycleLimit:
      s = "Cycle Limit";
      break;
    case Note_Filter:
      s = "Change Filter";
      break;
    case Note_Forecast:
      s = "Forecast Error"; // max chars 14 with this font
      break;
    case Note_Network:
      s = "Network Error";
      break;
    case Note_EE:
      s = "EE Written";
      break;
  }
  nex.itemText(12, s);
  if(s != "")
    event.alert(s);
}

// true: set screen backlight to bright or dim plus switch to thermostat
// false: cycle to next screensaver
bool Display::screen(bool bOn)
{
  static bool bOldOn = true;

  if(bOldOn && nex.getPage()) // not in sync
    bOldOn = false;

  if(bOn == false && nex.getPage() == 4) // last sequence was graph
    bOn = true;

  m_backlightTimer = NEX_TIMEOUT; // update the auto backlight timer
  if(bOn)
  {
    if( bOn == bOldOn )
      return false; // no change occurred
    nex.setPage("Thermostat");
    delay(100);
    refreshAll();
    nex.brightness(NEX_BRIGHT);
  }
  else
  {
    if(nex.getPage() == 1) // already clock
    {
      randomSeed(analogRead(0)+micros());
      nex.setPage("blank"); // lines
      nex.brightness(NEX_DIM);
    }
    else if(nex.getPage() == 5) // lines
    {
      nex.setPage("graph"); // chart thing (4)
      fillGraph();
      nex.brightness(50);
    }
    else
    {
      nex.setPage("clock"); // clock
      delay(100);
      updateClock();
      nex.brightness(NEX_MEDIUM);
    }
  }
  bOldOn = bOn;
  return true; // it was changed
}

// things to update on page change to thermostat
void Display::refreshAll()
{
  updateRunIndicator(true);
  drawForecast(false);
  updateNotification(true);
  updateAdjMode(true);
}

// Analog clock
void Display::updateClock()
{
  if(nex.getPage() != 1)
    return;

  nex.refreshItem("cl"); // erases lines
  delay(10);
  const float x = 159; // center
  const float y = 120;

  float size = 60;
  float angle = minute() * 6;
  float ang =  M_PI * (180-angle) / 180;
  float x2 = x + size * sin(ang);
  float y2 = y + size * cos(ang);
  nex.line(x, y, x2, y2, 63503); // (pink) minute

  size = 70;
  angle = second() * 6;
  ang =  M_PI * (180-angle) / 180;
  x2 = x + size * sin(ang);
  y2 = y + size * cos(ang);
  nex.line(x, y, x2, y2, 2016); // (green) second

  size = 48;
  angle = hour() * 30;
  ang =  M_PI * (180-angle) / 180;
  x2 = x + size * sin(ang);
  y2 = y + size * cos(ang);
  nex.line(x, y, x2, y2, 2047); // (cyan) hour
}

void Display::updateModes() // update any displayed settings
{
  const char *sFan[] = {"Auto", "On"};
  const char *sModes[] = {"Off", "Cool", "Heat", "Auto"};
  const char *sHeatModes[] = {"HP", "NG", "Auto"};
  static bool bFan = true; // set these to something other than default to trigger them all
  static uint8_t nMode = 10;
  static uint8_t heatMode = 10;

  if(nex.getPage())
    return;

  if(bFan != hvac.getFan())
    nex.itemText(9, sFan[bFan = hvac.getFan()]);

  if(nMode != hvac.getSetMode())
    nex.itemText(10, sModes[nMode = hvac.getSetMode()]);

  if(heatMode != hvac.getHeatMode())
    nex.itemText(11, sHeatModes[heatMode = hvac.getHeatMode()]);
}

void Display::updateAdjMode(bool bRef)  // current adjust indicator of the 4 temp settings
{
  static uint8_t am = 5;

  if(nex.getPage() || (bRef == false && am == m_adjustMode) )
    return;
  am = m_adjustMode;
  nex.visible("p0", (m_adjustMode == 0) ? 1:0);
  nex.visible("p1", (m_adjustMode == 1) ? 1:0);
  nex.visible("p2", (m_adjustMode == 2) ? 1:0);
  nex.visible("p3", (m_adjustMode == 3) ? 1:0);
}

void Display::updateRSSI()
{
  static uint8_t seccnt = 5;
  static int16_t rssiT;
  static int16_t rssi[5];
  int16_t rssiAvg;

  if(nex.getPage()) // must be page 0
    return;
  if(--seccnt)
    return;
  seccnt = 5;     // every 5 seconds
 
  memcpy(&rssi, &rssi[1], sizeof(int16_t)*4);
  rssi[4] = WiFi.RSSI();

  rssiAvg = 0;
  for(int i = 0; i < 5; i++)
    rssiAvg += rssi[i];

  rssiAvg /= 5;
  if(rssiAvg == rssiT)
    return;
  nex.itemText(22, String(rssiT = rssiAvg) + "dB");
}

void Display::updateRunIndicator(bool bForce) // run and fan running
{
  static bool bFanRun = true;
  static bool bOn = true; // blinker
  static bool bCurrent = true; // run indicator
  static bool bPic = false; // red/blue

  if(bForce)
  {
    bFanRun = true;
    bOn = true;
    bCurrent = true;
    bPic = false;
  }

  if(bFanRun != hvac.getFanRunning() && nex.getPage() == 0)
    nex.visible("p5", ( bFanRun = hvac.getFanRunning() ) ? 1:0); // fan on indicator

  if(hvac.getState()) // running
  {
    if(bPic != (hvac.getState() > State_Cool) ? true:false)
    {
      bPic = (hvac.getState() > State_Cool) ? true:false;
      nex.itemPic(4, bPic ? 3:1); // red or blue indicator
    }
    if(hvac.isRemoteTemp())
      bOn = !bOn; // blink indicator if remote temp
    else bOn = true; // just on
  }
  else bOn = false;

  if(bCurrent != bOn && nex.getPage() == 0)
    nex.visible("p4", (bCurrent = bOn) ? 1:0); // blinking run indicator
}

// Lines demo
void Display::Lines(bool bInit)
{
  if(nex.getPage() != 5) // must be blank page
    return;

  #define LINES 25
  static Line line[LINES], delta;
  uint16_t color;
  static uint8_t r=0, g=0, b=0;
  static int8_t cnt = 0;

  if(bInit)
  {
    randomSeed(analogRead(0) + micros());
    memset(&line, 10, sizeof(line));
    memset(&delta, 1, sizeof(delta));
    return;
  }
  // Erase oldest line
  nex.line(line[LINES - 1].x1, line[LINES - 1].y1, line[LINES - 1].x2, line[LINES - 1].y2, 0);

  // FIFO the lines
  for(int i = LINES - 2; i >= 0; i--)
    line[i+1] = line[i];

  if(--cnt <= 0)
  {
    cnt = 5; // every 5 runs
    delta.x1 = constrain(delta.x1 + random(-1,2), -4, 4); // random direction delta
    delta.x2 = constrain(delta.x2 + random(-1,2), -4, 4);
    delta.y1 = constrain(delta.y1 + random(-1,2), -4, 4);
    delta.y2 = constrain(delta.y2 + random(-1,2), -4, 4);
  }
  line[0].x1 += delta.x1; // add delta to positions
  line[0].y1 += delta.y1;
  line[0].x2 += delta.x2;
  line[0].y2 += delta.y2;

  line[0].x1 = constrain(line[0].x1, 0, 320); // keep it on the screen
  line[0].x2 = constrain(line[0].x2, 0, 320);
  line[0].y1 = constrain(line[0].y1, 0, 240);
  line[0].y2 = constrain(line[0].y2, 0, 240);

  b += random(-2, 3); // random RGB shift
  g += random(-3, 4); // green is 6 bits
  r += random(-2, 3);

  color = (((uint16_t)r << 8) & 0xF800) | (((uint16_t)g << 3) & 0x07E0) | ((uint16_t)b >> 3); // convert to 16 bit

  nex.line(line[0].x1, line[0].y1, line[0].x2, line[0].y2, color); // draw the new line

  if(line[0].x1 == 0 && delta.x1 < 0) delta.x1 = -delta.x1; // bounce off edges
  if(line[0].x2 == 0 && delta.x2 < 0) delta.x2 = -delta.x2;
  if(line[0].y1 == 0 && delta.y1 < 0) delta.y1 = -delta.y1;
  if(line[0].y2 == 0 && delta.y2 < 0) delta.y2 = -delta.y2;
  if(line[0].x1 == 320 && delta.x1 > 0) delta.x1 = -delta.x1;
  if(line[0].x2 == 320 && delta.x2 > 0) delta.x2 = -delta.x2;
  if(line[0].y1 == 240 && delta.y1 > 0) delta.y1 = -delta.y1;
  if(line[0].y2 == 240 && delta.y2 > 0) delta.y2 = -delta.y2;
}

void Display::addGraphPoints()
{
  static bool bInit = false;
  if(bInit == false)
  {
    memset(m_points, 0, sizeof(m_points));
    bInit = true;
  }
  if( hvac.m_inTemp == 0)
    return;
  if(m_pointsAdded == 299)
    memcpy(&m_points, &m_points+(4*sizeof(uint8_t)), sizeof(m_points) - (4*sizeof(uint8_t)));

  const int base = 600; // 60.0 base
  m_points[m_pointsAdded][0] = (hvac.m_inTemp - base) * 81 / 110; // 60~90 scale to 0~220
  m_points[m_pointsAdded][1] = hvac.m_rh * 55 / 250;
  m_points[m_pointsAdded][2] = (hvac.m_targetTemp - base) * 81 / 110;

  int8_t tt = hvac.m_EE.cycleThresh;
  if(hvac.getMode() == Mode_Cool) // Todo: could be auto
    tt = -tt;

  m_points[m_pointsAdded][3] = (hvac.m_targetTemp + tt - base) * 81 / 110;

  if(m_pointsAdded < 299) // 300x220
    m_pointsAdded++;
}

// Draw the last 25 hours (todo: add run times)
void Display::fillGraph()
{
  nex.text(279, 219, 33, 2, String(60));
  nex.text(279, 142, 33, 2, String(70));
  nex.text(279,  74, 33, 2, String(80));
  nex.text(279,   8, 33, 2, String(90));

  int16_t x = m_pointsAdded - 2 - (minute() / 5); // cetner over even hour
  int8_t h = hour();

  while(x > 0)
  {
    nex.text(x, 0, 16, 1, String(h)); // draw hour above chart
    x -= 12 * 6; // left 6 hours
    h -= 6;
    if( h < 0) h += 24;
  }

  uint16_t y = m_points[0][0];
  const int yOff = 240-10;

  for(int i = 1; i < m_pointsAdded; i++)  // inTemp
  {
    nex.line(i+9, yOff - y, i+10, yOff - m_points[i][0], 0xF800); // red
    y = m_points[i][0];
    while(m_points[i][0] == m_points[i+1][0] && i < m_pointsAdded) i++; // optimize
  }
  y = m_points[0][1];
  for(int i = 1; i < m_pointsAdded; i++) // rh
  {
    nex.line(i+9, yOff - y, i+10, yOff - m_points[i][1], 0x0380); // green
    y = m_points[i][1];
    while(m_points[i][1] == m_points[i+1][1] && i < m_pointsAdded) i++;
  }
  y = m_points[0][2];
  for(int i = 1; i < m_pointsAdded; i++) // on target
  {
    nex.line(i+9, yOff - y, i+10, yOff - m_points[i][2], 0x0031); // blue
    y = m_points[i][2];
    while(m_points[i][2] == m_points[i+1][2] && i < m_pointsAdded) i++;
  }
  y = m_points[0][3];
  for(int i = 1; i < m_pointsAdded; i++) // off target
  {
    nex.line(i+9, yOff - y, i+10, yOff - m_points[i][3], 0x0031);
    y = m_points[i][3];
    while(m_points[i][3] == m_points[i+1][3] && i < m_pointsAdded) i++;
  }
}
