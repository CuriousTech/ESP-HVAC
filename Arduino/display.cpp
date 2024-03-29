#include "HVAC.h"
#include "display.h"
#include "Nextion.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include "jsonstring.h"
#include <TimeLib.h>
#include "forecast.h"
#ifdef ESP8266
#include <ESP8266mDNS.h> // for WiFi.RSSI()
#endif
#include "eeMem.h"
#ifdef USE_AUDIO
#include "music.h"
Music mus;
#endif

Nextion nex;
extern HVAC hvac;
extern Forecast FC;
extern void WsSend(String s);

void Display::init()
{
  nex.FFF(); // Just to end any debug strings in the Nextion
  nex.reset();
  screen( true ); // brighten the screen if it just reset
  refreshAll();
  nex.itemPic(9, ee.b.bLock ? 20:21);
  updateNotification(true);
  FC.init( (ee.tz+hvac.m_DST)*3600 );
#ifdef USE_AUDIO
  mus.init();
#endif
}

// called each second
void Display::oneSec()
{
  if(WiFi.status() != WL_CONNECTED)
    return;
  updateClock();
  updateRunIndicator(false); // running stuff
  displayTime();    // time update every second
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

  if(nex.getPage() == Page_Thermostat && FC.m_bFcstUpdated)
  {
    FC.m_bFcstUpdated = false;
    drawForecast(true);
  }
}

void Display::buttonRepeat()
{
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
  static char cBuf[64];
  int len = nex.service(cBuf); // returns just the button value or 0 if no data
  uint8_t btn;
  String s;
  static uint8_t textIdx = 0;
#ifdef USE_AUDIO
  mus.service();
#endif
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
      if( m_backlightTimer == 0)
      {
        nex.brightness(NEX_BRIGHT); // backlight was off, ignore this input
        m_backlightTimer = NEX_TIMEOUT;
        return;
      }

#ifdef USE_AUDIO
      if(cBuf[3]) // press, not release
        mus.add(6000, 20);
#endif
 
      switch(cBuf[1]) // page
      {
        case Page_Thermostat:
          m_backlightTimer = NEX_TIMEOUT;
          switch(btn)
          {
            case 6 ... 9: // cool hi, lo, heat hi, lo
              hvac.m_bLink = (m_adjustMode == btn-6);
              m_adjustMode = btn-6;
              break;
            case 15 ... 18: // cool hi, lo, heat hi, lo
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
              if(ee.b.bLock) break;
              hvac.setFan( (hvac.getFan() == FM_On) ? FM_Auto : FM_On ); // Todo: Add 3rd icon
              updateModes(); // faster feedback
              break;
            case 23: // Mode
              if(ee.b.bLock) break;
              hvac.setMode( (hvac.getSetMode() + 1) & 3 );
              updateModes(); // faster feedback
              break;
            case 24: // Heat
              if(ee.b.bLock) break;
              hvac.setHeatMode( (hvac.getHeatMode() + 1) % 3 );
              updateModes(); // faster feedback
              break;
            case 10: // notification clear
              if(ee.b.bLock) break;
              hvac.m_notif = Note_None;
#ifdef USE_AUDIO
              mus.add(6000, 20);
              mus.add(7000, 20);
#endif
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
              if(ee.b.bLock) break;
              textIdx = 0;
              nex.setPage("keyboard"); // go to keyboard
              nex.itemText(1, "Enter Zipcode");
              break;
            case 13: // temp scale
              if(ee.b.bLock) break;
              textIdx = 1;
              nex.setPage("keyboard"); // go to keyboard
              nex.itemText(1, "Enter Password");
              break;
            case 5:  // target temp
            case 19:
              if(ee.b.bLock) break;
              hvac.enableRemote();
              break;
            case 1: // out
              break;
            case 3: // in
            case 4: // rh
              updateTemps();
              break;
            case 21: // humidifier indicator
              break;
            case 25: // lock
//#define PWLOCK  // uncomment for password entry unlock
#ifdef PWLOCK
              if(ee.b.bLock)
              {
                textIdx = 2;
                nex.itemText(0, ""); // clear last text
                nex.setPage("keyboard"); // go to keyboard
                nex.itemText(1, "Enter Password");
              }
              else
                ee.b.bLock = 1;
#else
              ee.b.bLock = !ee.b.bLock;
#endif
              nex.itemPic(9, ee.b.bLock ? 20:21);
              break;
          }
          break;
        case Page_SSID: // Selection page t1=ID 2 ~ t16=ID 17
          WiFi.SSID(cBuf[2]-2).toCharArray(ee.szSSID, sizeof(ee.szSSID) );
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
        case 0: // city ID edit
          if(strlen(cBuf + 1) < 5)
            break;
          strncpy(ee.cityID, cBuf + 1, sizeof(ee.cityID));
          break;
        case 1: // password edit
          if(strlen(cBuf + 1) < 5)
            return;
          strncpy(ee.password, cBuf + 1, sizeof(ee.password) );
          break;
        case 2: // password unlock
          if(!strcmp(ee.password, cBuf + 1) )
            ee.b.bLock = false;
          nex.itemText(0, ""); // clear password
          nex.itemPic(9, ee.b.bLock ? 20:21);
          break;
        case 3: // AP password
          nex.setPage("Thermostat");
          strncpy(ee.szSSIDPassword, cBuf + 1, sizeof(ee.szSSIDPassword) );
          ee.update();
          delay(500);
#ifdef ESP32
          ESP.restart();
#else
          ESP.reset();
#endif
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

  if(bRmt != hvac.m_bRemoteStream)
  {
    bRmt = hvac.m_bRemoteStream;
    nex.itemColor("f2", bRmt ? rgb16(31, 0, 15) : rgb16(0, 63, 31));
    nex.itemColor("f3", bRmt ? rgb16(31, 0, 15) : rgb16(0, 63, 31));
  }

  if(last[0] != hvac.m_inTemp)     nex.itemFp(2, last[0] = hvac.m_inTemp);
  if(last[1] != hvac.m_rh)         nex.itemFp(3, last[1] = hvac.m_rh);
  if(last[2] != hvac.m_targetTemp) nex.itemFp(4, last[2] = hvac.m_targetTemp);
  if(last[3] != ee.coolTemp[1])    nex.itemFp(5, last[3] = ee.coolTemp[1]);
  if(last[4] != ee.coolTemp[0])    nex.itemFp(6, last[4] = ee.coolTemp[0]);
  if(last[5] != ee.heatTemp[1])    nex.itemFp(7, last[5] = ee.heatTemp[1]);
  if(last[6] != ee.heatTemp[0])    nex.itemFp(8, last[6] = ee.heatTemp[0]);
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

bool Display::drawForecast(bool bRef)
{
  if(FC.m_fc.Date == 0) // no data yet
  {
    if(FC.m_bUpdateFcstIdle)
      FC.m_bUpdateFcst = true;
    return false;
  }

  int8_t fcOff;
  int8_t fcDispOff = 0;
  int8_t fcCnt;
  uint32_t tm;

  if(!FC.getCurrentIndex(fcOff, fcCnt, tm))
    return false;

  if(nex.getPage()) // on different page
    return true;

  if(bRef) // new forecast (erase old floating text)
  {
    nex.refreshItem("t19");
    nex.refreshItem("t20");
    nex.refreshItem("s0");
    delay(5);
  }

  tmElements_t tmE;
  breakTime(FC.m_fc.Date + ((ee.tz+hvac.m_DST)*3600) + (fcOff * FC.m_fc.Freq), tmE); // get current hour

  if(fcOff >= (tmE.Hour / 3) )
    fcDispOff = fcOff - (tmE.Hour / 3); // shift back to start of day
  else
    fcDispOff = fcOff; // else just shift the first day

  breakTime(FC.m_fc.Date + ((ee.tz+hvac.m_DST)*3600) + (fcDispOff * FC.m_fc.Freq), tmE);  // get current hour after adjusting for display offset

  int8_t hrng = fcCnt - fcDispOff;
  if(hrng > ee.fcDisplay)
    hrng = ee.fcDisplay; // shorten to user display range

  // Update min/max
  int16_t tmin;
  int16_t tmax;
  FC.getMinMax(tmin, tmax, fcDispOff, hrng);

  int16_t y = Fc_Top+1;
  int16_t incy = (Fc_Height-4) / 3;
  int16_t dec = (tmax - tmin)/3;
  int16_t t = tmax;
  int16_t x;

  // temp scale
  for(int i = 0; i <= 3; i++)
  {
    nex.text(3, y-6, 0, rgb16(0, 31, 31), String(t/10)); // font height/2=6?
    y += incy;
    t -= dec;
  }

  int hrs = hrng * FC.m_fc.Freq / 3600; // normally 180ish hours
  int day_x = 0;
  if((tmax-tmin) == 0 || hrs <= 0) // divide by 0
    return true;

  int y2 = Fc_Top+Fc_Height - 1 - (FC.m_fc.Data[fcOff].temp - tmin) * (Fc_Height-2) / (tmax-tmin);
  int x2 = Fc_Left;
  int hOld = 0;
  uint8_t wkday = tmE.Wday - 1;              // current DOW

  int h = 0;

  for(int i = fcOff; i < fcOff+hrng && FC.m_fc.Data[i].temp != -1000; i++) // should be 41 data points (close to 300ms)
  {
    int y1 = Fc_Top+Fc_Height - 1 - (FC.m_fc.Data[i].temp - tmin) * (Fc_Height-2) / (tmax-tmin);
    int x1 = Fc_Left + h * (Fc_Width-1) / hrs;

    nex.line(x2, y2, x1, y1, rgb16(31, 0, 0) ); // red

    int h24 = h % 24;
    if(hOld > h24 ) // new day (draw line)
    {
      nex.line(x1, Fc_Top+1, x1, Fc_Top+Fc_Height-2, rgb16(20, 41, 20) ); // (light gray)
      if(x1 - 14 > Fc_Left) // fix 1st day too far left
      {
        nex.text(day_x = x1 - 27, Fc_Top+Fc_Height+1, 1, rgb16(0, 63, 31), _days_short[wkday]); // cyan
      }
      if(++wkday > 6) wkday = 0;
    }
    if( hOld < 12 && h24 >= 12) // noon (dark line)
    {
      nex.line(x1, Fc_Top, x1, Fc_Top+Fc_Height, rgb16(12, 25, 12) ); // gray
    }
    hOld = h24;
    delay(7); // avoid buffer overrun
#ifdef USE_AUDIO
    mus.service();
#endif
    x2 = x1;
    y2 = y1;
    h += FC.m_fc.Freq / 3600;
  }
  day_x += 28;
  if(day_x < Fc_Left+Fc_Width - (8*3) )  // last partial day
    nex.text(day_x, Fc_Top+Fc_Height+1, 1, rgb16(0, 63, 31), _days_short[wkday]); // cyan
  return true;
}

void Display::displayOutTemp()
{
  if(FC.m_fc.Date == 0) // not read yet or time not set
    return;

  FC.getMinMax(hvac.m_outMin, hvac.m_outMax, 0, ee.fcRange);

  int outTempShift;
  int outTempReal = FC.getCurrentTemp(outTempShift, ee.fcOffset[hvac.m_modeShadow == Mode_Heat] );

  if(nex.getPage() == Page_Thermostat)
    nex.itemFp(1, outTempReal);

  // Summer/winter curve.  Summer is delayed 3 hours
  hvac.updateOutdoorTemp( outTempShift );
}

void Display::Note(char *cNote)
{
  screen(true);
  nex.itemText(12, cNote);
  jsonString js("alert");
  js.Var("text", cNote);
  WsSend(js.Close());
}

// update the notification text box
void Display::updateNotification(bool bRef)
{
  static uint8_t note_last = Note_None; // Needs a clear after startup
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
    case Note_Connecting:
      s = "Connecting";
      break;
    case Note_Connected:
      s = "Connected";
      break;
    case Note_RemoteOff:
      s = "Remote Off";
      break;
    case Note_RemoteOn:
      s = "Remote On";
      break;
    case Note_EspTouch:
      s = "Use EspTouch App";
      break;
    case Note_Found:
      s = "HVAC Found";
      break;
  }
  nex.itemText(12, s);
  if(s != "")
  {
    if(bRef == false) // refresh shouldn't be resent
    {
      jsonString js("alert");
      js.Var("text", s);
      WsSend(js.Close());
    }
#ifdef USE_AUDIO
    if(bRef == false || hvac.m_notif >= Note_Network) // once / repeats if important
    {
      mus.add(3500, 180);
      mus.add(2500, 100); // notification sound
    }
#endif
  }
}

// true: set screen backlight to bright plus switch to thermostat
// false: cycle to next screensaver
bool Display::screen(bool bOn)
{
  if(WiFi.status() != WL_CONNECTED )
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
  delay(27); // 25 or less has flicker with latest FW. 30 is good
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
    if(nMode == Mode_Fan)
      nex.itemPic(7, 10);
    else
      nex.itemPic(7, nMode + 13);

    hvac.m_bLink = true;
    m_adjustMode = (nMode == Mode_Heat) ? 2:0; // set adjust to selected heat/cool
    updateAdjMode(false);
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

  uint32_t pdate = now() - ((ee.tz+hvac.m_DST)*3600);
  if(m_lastPDate == 0)
    m_lastPDate = pdate;
  p->bits.tmdiff = pdate - m_lastPDate;
  m_lastPDate = pdate;
  p->t.inTemp = hvac.m_inTemp;
  if(hvac.m_modeShadow == Mode_Heat)
    p->t.target = hvac.m_targetTemp;
  else // cool
    p->t.target = hvac.m_targetTemp - ee.cycleThresh[0];
  p->t.outTemp = hvac.m_outTemp;
  p->bits.rh = hvac.m_rh;
  p->bits.fan = hvac.getFanRunning();
  p->bits.state = hvac.getState(); 
  p->sens0 = hvac.m_Sensor[0].temp - p->t.inTemp;
  p->sens1 = hvac.m_Sensor[1].temp - p->t.inTemp;
  p->sens2 = hvac.m_Sensor[2].temp - p->t.inTemp;
  p->sens3 = hvac.m_Sensor[3].temp - p->t.inTemp;
  p->bits.sens4 = hvac.m_Sensor[4].temp - p->t.inTemp;
  if(++m_pointsIdx >= GPTS)
    m_pointsIdx = 0;
  m_points[m_pointsIdx].t.u = 0; // mark as invalid data/end
}

// Draw the last 25 hours
void Display::fillGraph()
{
  m_tempHigh = (ee.b.bCelcius ? 370:990);
  m_tempLow = (ee.b.bCelcius ? 240:750);

  int tempMin = minPointVal(0);
  if(tempMin < (ee.b.bCelcius ? 240:750) )
  {
    m_tempHigh = (ee.b.bCelcius ? 320:890);
    m_tempLow = (ee.b.bCelcius ? 180:650);
  }
  int tmpInc = (m_tempHigh - m_tempLow) / 4;
  uint16_t textcolor = rgb16(0, 63, 31);
  int temp = m_tempLow;
  int y = 216;
  for(int i = 0; i < 5; i++)
  {
    nex.text(302, y, 2, textcolor, String(temp / 10) );
    if(i>0) nex.line( 10, y+8, 310, y+8, rgb16(10, 20, 10) );
    y -= 53;
    temp += tmpInc;
    delay(1);
  }

  int x = 310 - (minute() / 5); // center over even hour, 5 mins per pixel
  int h = hourFormat12();

  while(x > 12 * 6)
  {
    x -= 12 * 6; // left 6 hours
    h -= 6;
    if( h <= 0) h += 12;
    nex.line(x, 10, x, 230, rgb16(10, 20, 10) );
    delay(3);
    nex.text(x-4, 0, 1, 0x7FF, String(h)); // draw hour above chart
  }
#ifdef USE_AUDIO
    mus.service();
#endif
  yield();
  delay(3);
  drawPoints(0, rgb16( 22, 40, 10) ); // target (draw behind the other stuff)
#ifdef USE_AUDIO
    mus.service();
#endif
  delay(3);
  drawPoints(1, rgb16( 22, 40, 10) ); // target threshold
#ifdef USE_AUDIO
    mus.service();
#endif
  delay(3);
  drawPointsTemp(); // off/cool/heat colors
  delay(3);
#ifdef USE_AUDIO
    mus.service();
#endif
  drawPointsRh( rgb16(  0, 53,  0) ); // rh green
}

void Display::drawPoints(int w, uint16_t color)
{
  int i = m_pointsIdx - 1;
  if(i < 0) i = GPTS-1;
  const int yOff = 240-10;
  int y, y2;

  for(int x = 309, x2 = 310; x >= 10; x--)
  {
    if(m_points[i].t.u == 0)
      return;
    switch(w)
    {
      case 0: y = m_points[i].t.target + ee.cycleThresh[(hvac.m_modeShadow == Mode_Heat) ? 1:0]; break;
      case 1: y = m_points[i].t.target; break;
    }

    y = (constrain(y, m_tempLow, m_tempHigh) - m_tempLow) * 220 / (m_tempHigh-m_tempLow); // scale to 0~220

    if(y != y2)
    {
      if(x != 309) nex.line(x, yOff - y, x2, yOff - y2, color);
      y2 = y;
      x2 = x;
    }
    if(--i < 0)
      i = GPTS-1;
  }
}

void Display::drawPointsRh(uint16_t color)
{
  int i = m_pointsIdx - 1;
  if(i < 0) i = GPTS-1;
  const int yOff = 240-10;
  int y, y2 = m_points[i].bits.rh;

  if(m_points[i].t.u == 0)
    return; // not enough data

  y2 = y2 * 55 / 250; // 0~100 to 0~240

  for(int x = 309, x2 = 310; x >= 10; x--)
  {
    if(--i < 0)
      i = GPTS-1;

    y = m_points[i].bits.rh;
    if(m_points[i].t.u == 0)
      return;
    y = y * 55 / 250;

    if(y != y2)
    {
      nex.line(x, yOff - y, x2, yOff - y2, color);
      y2 = y;
      x2 = x;
    }
  }
}

void Display::drawPointsTemp()
{
  const int yOff = 240-10;
  int y, y2;
  int x2 = 310;
  int i = m_pointsIdx-1;

  if(i < 0) i = GPTS-1;

  for(int x = 309; x >= 10; x--)
  {
    if(m_points[i].t.u == 0)
      break; // end
    y = (constrain(m_points[i].t.inTemp, m_tempLow, m_tempHigh) - m_tempLow) * 220 / (m_tempHigh-m_tempLow);
    if(y != y2)
    {
      if(x != 309) nex.line(x2, yOff - y2, x, yOff - y, stateColor(m_points[i].bits) );
      y2 = y;
      x2 = x;
    }
    if(--i < 0)
      i = GPTS-1;
  }
}

uint16_t Display::stateColor(gflags v) // return a color based on run state
{
  uint16_t color;

  if(v.fan) // fan
    color = rgb16(0, 50, 0); // green

  switch(v.state)
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
  if(m_points[idx].t.u == 0) // invalid data
    return false;
  memcpy(pts, &m_points[idx], sizeof(gPoint));
  return true;
}

int Display::minPointVal(int n)
{
  int minv = 10000;
  int maxv = -1000;
  int i = m_pointsIdx - 1; // 0 = last entry
  if(i < 0) i = GPTS-1;

  while(i != m_pointsIdx)
  {
    int val;
    if(m_points[i].t.u == 0)
      break;
    switch(n)
    {
      default: val = m_points[i].t.inTemp; break;
      case 1: val = m_points[i].t.target; break;
      case 2: val = m_points[i].t.target + ee.cycleThresh[(hvac.m_modeShadow == Mode_Heat) ? 1:0]; break;
      case 3: val = m_points[i].bits.rh; break;
      case 4: val = m_points[i].t.outTemp; break;
    }
    if(minv > val) 
      minv = val;
    if(maxv < val) 
      maxv = val;
    if(--i < 0) i = GPTS-1;
  }
  m_tempMax = maxv;
  return minv;
}
