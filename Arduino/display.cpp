#include "display.h"
#include "Nextion.h"
#include "HVAC.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include <TimeLib.h>
#include <ESP8266mDNS.h> // for WiFi.RSSI()
#include "eeMem.h"
#include "WiFiManager.h"

Nextion nex;
extern HVAC hvac;
extern WiFiManager wifi;
extern void WsSend(char *,const char *);

void Display::init()
{
  if(wifi.isCfg() ) // don't interfere with SSID config
    return;
  memset(m_points, 255, sizeof(m_points));
  nex.FFF(); // Just to end any debug strings in the Nextion
  nex.reset();
  screen( true ); // brighten the screen if it just reset
  refreshAll();
  nex.itemPic(9, ee.bLock ? 20:21);
}

// called each second
void Display::oneSec()
{
  if(wifi.isCfg() )
    return;
  updateClock();
  updateRunIndicator(false); // running stuff
  displayTime();    // time update every seconds
  updateModes();    // mode, heat mode, fan mode
  updateTemps();    // 
  updateAdjMode(false); // update touched temp settings
  updateNotification(false);
  updateRSSI();     //
  if( m_backlightTimer ) // the dimmer thing
  {
    if(--m_backlightTimer == 0)
        screen(false);
  }
  static uint8_t lastState;
  static bool lastFan;
  if(--m_temp_counter <= 0 || hvac.getState() != lastState || hvac.getFanRunning() != lastFan)
  {
    displayOutTemp();
    addGraphPoints();
    lastState = hvac.getState();
    lastFan = hvac.getFanRunning();
  }
  if(m_bUpdateFcstDone)
  {
    WsSend("Forecast success", "print");
    screen(true);
    drawForecast(true);
    m_bUpdateFcstDone = false;
  }
}

void Display::buttonRepeat()
{
 m_btnMode;
 m_btnDelay;
  int8_t m = (m_adjustMode < 2) ? Mode_Cool : Mode_Heat; // lower 2 are cool
  int8_t hilo = (m_adjustMode ^ 1) & 1; // hi or low of set
  int16_t t = hvac.getSetTemp(m, hilo );
  
  t += (m_btnMode==1) ? 1:-1; // inc by 0.1
  hvac.setTemp(m, t, hilo);
  
  if(hvac.m_bLink) // adjust both high and low
  {
    t = hvac.getSetTemp(m, hilo^1 ) + ((m_btnMode==1) ? 1:-1); // adjust opposite hi/lo the same
    hvac.setTemp(m, t, hilo^1);
  }
  updateTemps();
}

void Display::checkNextion() // all the Nextion recieved commands
{
  char cBuf[64];
  int len = nex.service(cBuf); // returns just the button value or 0 if no data
  uint8_t btn;
  String s;
  static uint8_t textIdx = 0;

  Lines(); // draw lines at full speed

  if(len == 0)
  {
    if(m_btnMode)
      if(--m_btnDelay <= 0)
      {
        buttonRepeat();
        m_btnDelay = 20; // repeat speed
      }
    return;
  }

  switch(cBuf[0])  // code
  {
    case 0x65: // button
      btn = cBuf[2];
      nex.brightness(NEX_BRIGHT);
 
      switch(cBuf[1]) // page
      {
        case Page_Thermostat:
          m_backlightTimer = NEX_TIMEOUT;
          switch(btn)
          {
            case 6: // cool hi
            case 7: // cool lo
            case 8: // heat hi
            case 9: // heat lo
              hvac.m_bLink = (m_adjustMode == btn-6);
              m_adjustMode = btn-6;
              break;
            case 15: // cool hi
            case 16: // cool lo
            case 17: // heat hi
            case 18: // heat lo
              hvac.m_bLink = (m_adjustMode == btn-15);
              m_adjustMode = btn-15;
              break;

            case 26: // Up button
              if(cBuf[3]) // press
              {
                m_btnMode = 1;
                buttonRepeat();
                m_btnDelay = 40; // first repeat
              }
              else m_btnMode = 0; // release
              break;
            case 27: // Down button
              if(cBuf[3])
              {
                m_btnMode = 2;
                buttonRepeat();
                m_btnDelay = 40;
              }
              else m_btnMode = 0;
              break;

            case 22: // fan
              if(ee.bLock) break;
              hvac.setFan( (hvac.getFan() == FM_On) ? FM_Auto : FM_On ); // Todo: Add 3rd icon
              updateModes(); // faster feedback
              break;
            case 23: // Mode
              if(ee.bLock) break;
              hvac.setMode( (hvac.getSetMode() + 1) & 3 );
              updateModes(); // faster feedback
              break;
            case 24: // Heat
              if(ee.bLock) break;
              hvac.setHeatMode( (hvac.getHeatMode() + 1) % 3 );
              updateModes(); // faster feedback
              break;
            case 10: // notification clear
              if(ee.bLock) break;
              hvac.m_notif = Note_None;
              break;
            case 11: // forecast
              nex.setPage("graph");
              fillGraph();
              break;
            case 2: // time
              nex.setPage("clock");
              delay(10); // 20 works
              updateClock();
              break;
            case 12: // DOW
              if(ee.bLock) break;
              textIdx = 0;
              nex.setPage("keyboard"); // go to keyboard
              nex.itemText(1, "Enter Zipcode");
              break;
            case 13: // temp scale
              if(ee.bLock) break;
              textIdx = 1;
              nex.setPage("keyboard"); // go to keyboard
              nex.itemText(1, "Enter Password");
              break;
            case 5:  // target temp
            case 19:
              if(ee.bLock) break;
              hvac.enableRemote();
              break;
            case 1: // out
              break;
            case 3: // in
            case 4: // rh
              hvac.m_bLocalTempDisplay = !hvac.m_bLocalTempDisplay; // toggle remote temp display
              updateTemps();
              break;
            case 21: // humidifier indicator
              break;
            case 25: // lock
//#define PWLOCK  // uncomment for password entry unlock
#ifdef PWLOCK
              if(ee.bLock)
              {
                textIdx = 2;
                nex.itemText(0, ""); // clear last text
                nex.setPage("keyboard"); // go to keyboard
                nex.itemText(1, "Enter Password");
              }
              else
                ee.bLock = true;
#else
              ee.bLock = !ee.bLock;
#endif
              nex.itemPic(9, ee.bLock ? 20:21);
              break;
          }
          break;
        case Page_SSID: // Selection page t1=ID 2 ~ t16=ID 17
          wifi.setSSID(cBuf[2]-2);
          nex.refreshItem("t0"); // Just to terminate any debug strings in the Nextion
          nex.setPage("keyboard"); // go to keyboard
          nex.itemText(1, "Enter Password");
          textIdx = 3; // AP password
          break;

        default: // all pages go back
          screen(true);
          break;
      }
      break;
    case 0x70:// string return from keyboard
      switch(textIdx)
      {
        case 0: // zipcode edit
          if(strlen(cBuf + 1) < 5)
            break;
          strncpy(ee.zipCode, cBuf + 1, sizeof(ee.zipCode));
          break;
        case 1: // password edit
          if(strlen(cBuf + 1) < 5)
            return;
          strncpy(ee.password, cBuf + 1, sizeof(ee.password) );
          break;
        case 2: // password unlock
          if(!strcmp(ee.password, cBuf + 1) )
            ee.bLock = false;
          nex.itemText(0, ""); // clear password
          nex.itemPic(9, ee.bLock ? 20:21);
          break;
        case 3: // AP password
          nex.setPage("Thermostat");
          wifi.setPass(cBuf + 1);
          break;
      }
      screen(true); // back to main page
      break;
  }
}

void Display::updateTemps()
{
  static uint16_t last[7];  // only draw changes
  static bool bRmt = false;

  if(nex.getPage())
  {
    memset(last, 0, sizeof(last));
    bRmt = false;
    return;
  }

  if(bRmt != hvac.showLocalTemp())
  {
    bRmt = hvac.showLocalTemp();
    nex.itemColor("f2", bRmt ? rgb16(31, 0, 15) : rgb16(0, 63, 31));
    nex.itemColor("f3", bRmt ? rgb16(31, 0, 15) : rgb16(0, 63, 31));
  }

  uint16_t temp = hvac.showLocalTemp() ? hvac.m_localTemp : hvac.m_inTemp;
  uint16_t rh = hvac.showLocalTemp() ? hvac.m_localRh : hvac.m_rh;

  if(last[0] != temp)               nex.itemFp(2, last[0] = temp);
  if(last[1] != rh)                 nex.itemFp(3, last[1] = rh);
  if(last[2] != hvac.m_targetTemp)  nex.itemFp(4, last[2] = hvac.m_targetTemp);
  if(last[3] != ee.coolTemp[1])     nex.itemFp(5, last[3] = ee.coolTemp[1]);
  if(last[4] != ee.coolTemp[0])     nex.itemFp(6, last[4] = ee.coolTemp[0]);
  if(last[5] != ee.heatTemp[1])     nex.itemFp(7, last[5] = ee.heatTemp[1]);
  if(last[6] != ee.heatTemp[0])     nex.itemFp(8, last[6] = ee.heatTemp[0]);
}

const char *_days_short[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// time and dow on main page
void Display::displayTime()
{
  static int8_t lastDay = -1;

  if(nex.getPage())
  {
    lastDay = -1;
    return;  // t7 and t8 are only on thermostat (for now)
  }

  String sTime = String( hourFormat12() );
  sTime += ":";
  if(minute() < 10) sTime += "0";
  sTime += minute();
  sTime += ":";
  if(second() < 10) sTime += "0";
  sTime += second();
  sTime += " ";
  sTime += isPM() ? "PM":"AM";

  nex.itemText(8, sTime);

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
  int i;

  if(m_fcData[1].tm == 0) // no data yet
  {
    if(m_bUpdateFcstDone)
      m_bUpdateFcst = true;
    return;
  }

  int fcCnt;
  for(fcCnt = 1; fcCnt < FC_CNT; fcCnt++) // get length (0 = end)
    if(m_fcData[fcCnt].tm == 0)
      break;

//  fcCnt = min(40, fcCnt); // 5 day limit
  if(fcCnt > 40) fcCnt = 40;

  if(bRef)
  {
    // Update min/max
    int8_t tmin = m_fcData[0].temp;
    int8_t tmax = m_fcData[0].temp;

    if(tmin == 0) // initial value
      tmin = m_fcData[1].temp;

    int rng = fcCnt;
    if(rng > ee.fcRange) rng = ee.fcRange;

    // Get min/max of current forecast
    for(int i = 1; i < rng; i++)
    {
      int8_t t = m_fcData[i].temp;
      if(tmin > t) tmin = t;
      if(tmax < t) tmax = t;
    }

    if(tmin == tmax) tmax++;   // div by 0 check

    hvac.m_outMin = tmin;
    hvac.m_outMax = tmax;
  }

  displayOutTemp(); // update temp for HVAC

  if(nex.getPage()) // on different page
    return;

  if(bRef) // new forecast (erase old floating text)
  {
    nex.refreshItem("t19");
    nex.refreshItem("t20");
    nex.refreshItem("s0");
    delay(5);
  }

    // Update min/max
    int8_t tmin = m_fcData[0].temp;
    int8_t tmax = m_fcData[0].temp;

    if(tmin == 0) // initial value
      tmin = m_fcData[1].temp;

    int rng = fcCnt;
    if(rng > ee.fcDisplay) rng = ee.fcDisplay;

    // Get min/max of current forecast
    for(int i = 1; i < rng; i++)
    {
      int8_t t = m_fcData[i].temp;
      if(tmin > t) tmin = t;
      if(tmax < t) tmax = t;
    }

    if(tmin == tmax) tmax++;   // div by 0 check
   
  int16_t y = Fc_Top+1;
  int16_t incy = (Fc_Height-4) / 3;
  int16_t dec = (tmax - tmin)/3;
  int16_t t = tmax;
  int16_t x;

  // temp scale
  for(i = 0; i <= 3; i++)
  {
    nex.text(3, y-6, 0, rgb16(0, 31, 31), String(t)); // font height/2=6?
    y += incy;
    t -= dec;
  }

  int hrs = (m_fcData[fcCnt-1].tm - m_fcData[1].tm) / 3600; // normally 180ish hours
  int day_x = 0;

  if((tmax-tmin) == 0 || hrs <= 0) // divide by 0
    return;

  int y2 = Fc_Top+Fc_Height - 1 - (m_fcData[1].temp - tmin) * (Fc_Height-2) / (tmax-tmin);
  int x2 = Fc_Left;
  int hOld = 0;
  int day = weekday()-1;              // current day

  for(i = 1; i < fcCnt; i++) // should be 41 data points
  {
    int y1 = Fc_Top+Fc_Height - 1 - (m_fcData[i].temp - tmin) * (Fc_Height-2) / (tmax-tmin);
    int h = m_fcData[i].tm;
    if(h < m_fcData[i-1].tm) h = m_fcData[i-1].tm; // Todo: temp fix (end of month?)
    h = (h - m_fcData[1].tm) / 3600;
    int x1 = Fc_Left + h * (Fc_Width-1) / hrs;

    if(x2 < Fc_Left) x2 = Fc_Left;  // first point may be history
    if(x1 < Fc_Left) x1 = x2;  // todo: fix this
    nex.line(x2, y2, x1, y1, rgb16(31, 0, 0) ); // red

    h = (m_fcData[i].tm / 3600) % 24; // current hour
    if(hOld > h) // new day (draw line)
    {
      nex.line(x1, Fc_Top+1, x1, Fc_Top+Fc_Height-2, rgb16(20, 41, 20) ); // (light gray)
      if(x1 - 14 > Fc_Left) // fix 1st day too far left
      {
        nex.text(day_x = x1 - 27, Fc_Top+Fc_Height+1, 1, rgb16(0, 63, 31), _days_short[day]); // cyan
      }
      if(++day > 6) day = 0;
    }
    if( hOld < 12 && h >= 12) // noon (dark line)
    {
      nex.line(x1, Fc_Top, x1, Fc_Top+Fc_Height, rgb16(12, 25, 12) ); // gray
    }
    hOld = h;
    delay(5); // small glitch in drawing
    x2 = x1;
    y2 = y1;
  }
  day_x += 28;
  if(day_x < Fc_Left+Fc_Width - (8*3) )  // last partial day
    nex.text(day_x, Fc_Top+Fc_Height+1, 1, rgb16(0, 63, 31), _days_short[day]); // cyan
}

// get value at current minute between hours
int Display::tween(int8_t t1, int8_t t2, int m, int r)
{
  if(r == 0) r = 1; // div by zero check
  float t = (float)(t2 - t1) * (m * 100 / r) / 100;
  return (int)((t + (float)t1) * 10);
}

void Display::displayOutTemp()
{
  if(m_fcData[1].tm == 0) // not read yet or time not set
    return;

  int iH = 0;
  int m = minute();
  uint32_t tmNow = now() - ((ee.tz+hvac.m_DST)*3600);
  if( tmNow >= m_fcData[1].tm)
  {
    for(iH = 1; tmNow > m_fcData[iH].tm && m_fcData[iH].tm && iH < FC_CNT - 1; iH++);
    if(iH) iH--; // set iH to current 3 hour frame
    m = (tmNow - m_fcData[iH].tm) / 60;  // offset = minutes past forecast
  }

  if(iH > 3 && m_bUpdateFcstDone) // if data more than 3*2 hours old, refresh
  {
    String s = String("iH=") + String(iH);
    WsSend((char*)s.c_str(), "alert");
  }

  int r = (m_fcData[iH+1].tm - m_fcData[iH].tm) / 60; // usually 3 hour range (180 m)
  int outTempReal = tween(m_fcData[iH].temp, m_fcData[iH+1].temp, m, r);
  int outTempDelayed = outTempReal;
  if(iH) // assume range = 3 hours for a -3 hour delay
  {
     r = (m_fcData[iH].tm - m_fcData[iH-1].tm) / 60;
     outTempDelayed = tween(m_fcData[iH-1].temp, m_fcData[iH].temp, m, r);
  }

  if(nex.getPage() == Page_Thermostat)
    nex.itemFp(1, outTempReal);

  // Summer/winter curve.  Summer is delayed 3 hours
  hvac.updateOutdoorTemp((ee.Mode == Mode_Heat) ? outTempReal : outTempDelayed);
}

void Display::Note(char *cNote)
{
  screen(true);
  nex.itemText(12, cNote);
  WsSend(cNote, "alert");
}

// update the notification text box
void Display::updateNotification(bool bRef)
{
  static uint8_t note_last = Note_Init; // Needs a clear after startup
  if(!bRef && note_last == hvac.m_notif) // nothing changed
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
      s = "Replace Filter";
      break;
    case Note_Forecast:
      s = "Forecast Error"; // max chars 14 with this font
      break;
    case Note_Network:
      s = "Network Error";
      break;
    case Note_RemoteOff:
      s = "Remote Off";
      break;
    case Note_RemoteOn:
      s = "Remote On";
      break;
  }
  nex.itemText(12, s);
  if(s != "" && bRef == false) // refresh shouldn't be resent
  {
    s = "{\"text\":\"" + s + "\"}";
    WsSend((char*)s.c_str(), "alert");
  }
}

// true: set screen backlight to bright plus switch to thermostat
// false: cycle to next screensaver
bool Display::screen(bool bOn)
{
  if(wifi.isCfg() )
    return false;
  static bool bOldOn = true;

  if(bOldOn && nex.getPage()) // not in sync
    bOldOn = false;

  if(bOn) // input or other reason
  {
    nex.brightness(NEX_BRIGHT);
  }
  if(bOn == false && nex.getPage() == Page_Graph) // last sequence was graph
    bOn = true;

  m_backlightTimer = NEX_TIMEOUT; // update the auto backlight timer

  if(bOn)
  {
    if( bOn == bOldOn )
      return false; // no change occurred
    nex.setPage("Thermostat");
    delay(25); // 20 works most of the time
    refreshAll();
  }
  else switch(nex.getPage())
  {
    case Page_Clock: // already clock
      randomSeed(analogRead(0)+micros());
      nex.setPage("blank"); // lines
      break;
    case Page_Blank: // lines
      nex.setPage("graph"); // chart thing
      fillGraph();
      break;
    default:  // probably thermostat
      nex.setPage("clock"); // clock
      delay(20);
      updateClock();
      nex.brightness(NEX_DIM);
      break;
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
  if(nex.getPage() != Page_Clock)
    return;

  nex.refreshItem("cl"); // erases lines
  delay(8); // 8 works, 5 does not
  const float x = 159; // center
  const float y = 120;
  float x2,y2,x3,y3;

  cspoint(x2, y2, x, y, minute() * 6, 80);
  cspoint(x3, y3, x, y, (minute()+5) * 6, 10);
  nex.line(x3, y3, x2, y2, rgb16(0, 0, 31) ); // (blue) minute
  cspoint(x3, y3, x, y, (minute()-5) * 6, 10);
  nex.line(x3, y3, x2, y2, rgb16(0, 0, 31) ); // (blue) minute

  float a = (hour() + (minute() * 0.00833) ) * 30;
  cspoint(x2, y2, x, y, a, 64);
  a = (hour() + (minute() * 0.00833)+2 ) * 30;
  cspoint(x3, y3, x, y, a, 10);
  nex.line(x3, y3, x2, y2, rgb16(0, 63, 31) ); // (cyan) hour
  a = (hour() + (minute() * 0.00833)-2 ) * 30;
  cspoint(x3, y3, x, y, a, 10);
  nex.line(x3, y3, x2, y2, rgb16(0, 63, 31) ); // (cyan) hour

  cspoint(x2, y2, x, y, second() * 6, 91);
  cspoint(x3, y3, x, y, (second()+30) * 6, 24);
  nex.line(x3, y3, x2, y2, rgb16(31, 0, 0) ); // (red) second
}

void Display::cspoint(float &x2, float &y2, float x, float y, float angle, float size)
{
  float ang =  M_PI * (180-angle) / 180;
  x2 = x + size * sin(ang);
  y2 = y + size * cos(ang);  
}

void Display::updateModes() // update any displayed settings
{
  const char *sFan[] = {"Auto", "On"};
  const char *sModes[] = {"Off", "Cool", "Heat", "Auto"};
  const char *sHeatModes[] = {"HP", "NG", "Auto"};
  static bool bFan = true; // set these to something other than default to trigger them all
  static int8_t FanMode = 4;
  static uint8_t nMode = 10;
  static uint8_t heatMode = 10;

  if(nex.getPage())
  {
    nMode = heatMode = 10;
    return;
  }

  if( (FanMode != hvac.getFan() || bFan != hvac.getFanRunning()) && nex.getPage() == Page_Thermostat)
  {
    int idx = 10; // not running
    FanMode = hvac.getFan();
    if( bFan = hvac.getFanRunning() )
    {
      idx = 11; // running
      if(FanMode == FM_On)
        idx = 12; // on and running
    }
    nex.itemPic(5, idx);
  }

  if(nMode != hvac.getSetMode())
  {
    nMode = hvac.getSetMode();
    nex.itemPic(7, nMode + 13);
  }

  if(heatMode != hvac.getHeatMode())
  {
    heatMode = hvac.getHeatMode();
    nex.itemPic(8, heatMode + 17);
  }
}

void Display::updateAdjMode(bool bRef)  // current adjust indicator of the 4 temp settings
{
  static uint8_t am = 0;
  static bool bl = false;
  // p0-p3
  if(nex.getPage() || (bRef == false && am == m_adjustMode && bl == hvac.m_bLink) )
    return;

  nex.visible("p" + String(am), 0); // turn off both hi/lo of last
  nex.visible("p" + String(am^1), 0);
  am = m_adjustMode;
  nex.visible("p" + String(am), 1);
  if(hvac.m_bLink)
    nex.visible("p" + String(am^1), 1); // turn on the opposite hi/lo
  bl = hvac.m_bLink;
}

void Display::updateRSSI()
{
  static uint8_t seccnt = 2;
  static int16_t rssiT;
#define RSSI_CNT 8
  static int16_t rssi[RSSI_CNT];
  static uint8_t rssiIdx = 0;

  if(nex.getPage()) // must be page 0
  {
    rssiT = 0; // cause a refresh later
    seccnt = 1;
    return;
  }
  if(--seccnt)
    return;
  seccnt = 3;     // every 3 seconds

  rssi[rssiIdx] = WiFi.RSSI();
  if(++rssiIdx >= RSSI_CNT) rssiIdx = 0;

  int16_t rssiAvg = 0;
  for(int i = 0; i < RSSI_CNT; i++)
    rssiAvg += rssi[i];

  rssiAvg /= RSSI_CNT;
  if(rssiAvg == rssiT)
    return;

  nex.itemText(22, String(rssiT = rssiAvg) + "dB");

  int sigStrength = 127 + rssiT;
  int wh = 24; // width and height
  int x = 142; // X/Y position
  int y = 172;
  int sect = 127 / 5; //
  int dist = wh  / 5; // distance between blocks

  y += wh;

  for (int i = 1; i < 6; i++)
  {
    nex.fill( x + i*dist, y - i*dist, dist-2, i*dist, (sigStrength > i * sect) ? rgb16(0, 63,31) : rgb16(5, 10, 5) );
  }
}

void Display::updateRunIndicator(bool bForce) // run and fan running
{
  static bool bOn = false; // blinker
  static bool bCurrent = false; // run indicator
  static bool bPic = false; // red/blue
  static bool bHumid = false; // next to rH

  if(bForce)
  {
    bOn = false;
    bCurrent = false;
    bPic = false;
    bHumid = false;
  }

  if(hvac.getState()) // running
  {
    if(bPic != (hvac.getState() > State_Cool) ? true:false)
    {
      bPic = (hvac.getState() > State_Cool) ? true:false;
      nex.itemPic(4, bPic ? 3:1); // red or blue indicator
    }
    if(hvac.m_bRemoteStream)
      bOn = !bOn; // blink indicator if remote temp
    else bOn = true; // just on
  }
  else bOn = false;

  if(bCurrent != bOn && nex.getPage() == Page_Thermostat)
    nex.visible("p4", (bCurrent = bOn) ? 1:0); // blinking run indicator

  if(bHumid != hvac.getHumidifierRunning() && nex.getPage() == Page_Thermostat)
    nex.visible("p6", (bHumid = hvac.getHumidifierRunning()) ? 1:0); // blinking run indicator
}

// Lines demo
void Display::Lines()
{
  if(nex.getPage() != Page_Blank)
    return;

#define LINES 25
  static Line line[LINES], delta;
  uint16_t color;
  static uint8_t r=0, g=0, b=0;
  static int8_t cnt = 0;
  static bool bInit = false;

  if(!bInit)
  {
    memset(&line, 10, sizeof(line));
    memset(&delta, 1, sizeof(delta));
    bInit = true;
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

  color = rgb(r,g,b);
  
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
  if( hvac.m_inTemp == 0 || hvac.m_targetTemp == 0)
    return;
  m_temp_counter = 5*60;         // update every 5 minutes
  gPoint *p = &m_points[m_pointsIdx];
  p->time = now() - ((ee.tz+hvac.m_DST)*3600);

  p->temp = hvac.m_inTemp; // 66~90 scale to 0~220
  if(hvac.m_modeShadow == Mode_Heat)
  {
    p->h = hvac.m_targetTemp;
    p->l = hvac.m_targetTemp - ee.cycleThresh[0];
  }
  else // heat
  {
    p->h = hvac.m_targetTemp + ee.cycleThresh[1];
    p->l = hvac.m_targetTemp;
  }
  p->ot = hvac.m_outTemp;
//  p->ltemp = hvac.m_localTemp;
  p->bits.b.rh = hvac.m_rh;
  p->bits.b.fan = hvac.getFanRunning();
  p->bits.b.state = hvac.getState(); 
  p->bits.b.res = 0; // just clear the extra
  if(++m_pointsIdx >= GPTS)
    m_pointsIdx = 0;
}

// Draw the last 25 hours (todo: add run times)
void Display::fillGraph()
{
  uint16_t textcolor = rgb16(0, 63, 31);
  nex.text(292, 219, 2, textcolor, String(66));
  nex.line( 10, 164+8, 310, 164+8, rgb16(10, 20, 10) );
  nex.text(292, 164, 2, textcolor, String(72));
  nex.line( 10, 112+8, 310, 112+8, rgb16(10, 20, 10) );
  nex.text(292, 112, 2, textcolor, String(78));
  nex.line( 10,  58+8, 310,  58+8, rgb16(10, 20, 10) );
  nex.text(292, 58, 2, textcolor, String(84));
  nex.text(292,  8, 2, textcolor, String(90));

  int x = 310 - (minute() / 5); // center over even hour, 5 mins per pixel
  int h = hourFormat12();

  while(x > 10)
  {
    nex.line(x, 10, x, 230, rgb16(10, 20, 10) );
    delay(1);
    nex.text(x-4, 0, 1, 0x7FF, String(h)); // draw hour above chart
    x -= 12 * 6; // left 6 hours
    h -= 6;
    if( h <= 0) h += 12;
  }
  delay(2);
  drawPoints(0, rgb16( 22, 40, 10) ); // target (draw behind the other stuff)
  drawPoints(1, rgb16( 22, 40, 10) ); // target threshold
  drawPointsRh( rgb16(  0, 53,  0) ); // rh green
//  if(hvac.isRemote())
//  {
//    drawPoints(2, rgb16( 31, 0,  15) ); // remote temp
//  }
  drawPointsTemp(); // off/cool/heat colors
}

void Display::drawPoints(int w, uint16_t color)
{
  int i = m_pointsIdx - 1;
  if(i < 0) i = GPTS-1;
  const int yOff = 240-10;
  int y, y2;

  switch(w)
  {
    case 0: y2 = m_points[i].h; break;
    case 1: y2 = m_points[i].l; break;
  }
  if(y2 == -1) return; // not enough data

  const int base = 660; // 66.0 base
  y2 = (constrain(y2, 660, 900) - base) * 101 / 110;

  for(int x = 309, x2 = 310; x >= 10; x--)
  {
    if(--i < 0)
      i = GPTS-1;

    switch(w)
    {
      case 0: y = m_points[i].h; break;
      case 1: y = m_points[i].l; break;
    }

    if(y == -1) return;
    y = (constrain(y, 660, 900) - base) * 101 / 110; // 660~900 scale to 0~220

    if(y != y2)
    {
      nex.line(x, yOff - y, x2, yOff - y2, color);
      delay(2);
      y2 = y;
      x2 = x;
    }
  }
}

void Display::drawPointsRh(uint16_t color)
{
  int i = m_pointsIdx - 1;
  if(i < 0) i = GPTS-1;
  const int yOff = 240-10;
  int y, y2 = m_points[i].bits.b.rh;
  if(y2 == -1) return; // not enough data

  y2 = y2 * 55 / 250; // 0~100 to 0~240

  for(int x = 309, x2 = 310; x >= 10; x--)
  {
    if(--i < 0)
      i = GPTS-1;

    y = m_points[i].bits.b.rh;
    if(y == -1) return;
    y = y * 55 / 250;

    if(y != y2)
    {
      nex.line(x, yOff - y, x2, yOff - y2, color);
      delay(1);
      y2 = y;
      x2 = x;
    }
  }
}

void Display::drawPointsTemp()
{
  const int yOff = 240-10;
  int i = m_pointsIdx-1;
  if(i < 0) i = GPTS-1;
  uint8_t y, y2 = m_points[i].temp;
  if(y2 == -1) return;
  int x2 = 310;

  const int base = 660; // 66.0 base
  y2 = (constrain(y2, 660, 900) - base) * 101 / 110;

  for(int x = 309; x >= 10; x--)
  {
    if(--i < 0)
      i = GPTS-1;
    y = m_points[i].temp;
    if(y == -1) break; // invalid data
    y = (constrain(y, 660, 900) - base) * 101 / 110;
    if(y != y2)
    {
      nex.line(x2, yOff - y2, x, yOff - y, stateColor(m_points[i].bits) );
      delay(2);
      y2 = y;
      x2 = x;
    }
  }
}

uint16_t Display::stateColor(gflags v) // return a color based on run state
{
  uint16_t color;

  if(v.b.fan) // fan
    color = rgb16(0, 50, 0); // green

  switch(v.b.state)
  {
    case State_Off: // off
      color = rgb16(20, 40, 20); // gray
      break;
    case State_Cool: // cool
      color = rgb16(0, 0, 31); // blue
      break;
    case State_HP:
    case State_NG:
      color = rgb16(31, 0, 0); // red
      break;
  }
  return color;
}

bool Display::getGrapthPoints(gPoint *pts, int n)
{
  if(n < 0 || n > GPTS-1) // convert 0-(GPTS-1) to reverse index circular buffer
    return false;
  int idx = m_pointsIdx - 1 - n; // 0 = last entry
  if(idx < 0) idx += GPTS;
  if(m_points[idx].temp == -1) // invalid data
    return false;
  memcpy(pts, &m_points[idx], sizeof(gPoint));
}

int Display::minPointVal(int n)
{
  int minv = 10000;

  for(int i = 0; i < GPTS; i++)
  {
    int val;
    switch(n)
    {
      default: val = m_points[i].temp; break;
      case 1: val = m_points[i].l; break;
      case 2: val = m_points[i].h; break;
      case 3: val = m_points[i].bits.b.rh; break;
      case 4: val = m_points[i].ot; break;
    }
    if(val == -1) break;
    if(minv > val) 
      minv = val;
  }
  return minv;
}
