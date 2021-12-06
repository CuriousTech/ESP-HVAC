/*
  HVAC.cpp - Arduino library for HVAC control.
  Copyright 2014 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.

  This library is distributed in the hope that it will be useful,  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
*/

#include <math.h>
#include "HVAC.h"
#include <TimeLib.h>
#include "eeMem.h"
#include "jsonstring.h"
#ifdef USE_AUDIO
#include "music.h"
extern Music mus;
#endif

#ifdef REMOTE
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include "WebHandler.h"

extern void WscSend(String s); // remote WebSocket
bool bValidData;
#endif

extern void WsSend(String s);

HVAC::HVAC()
{
#ifndef REMOTE
  pinMode(P_FAN, OUTPUT);
  pinMode(P_COOL, OUTPUT);
  pinMode(P_REV, OUTPUT);
  pinMode(P_HUMID, OUTPUT);
  pinMode(P_HEAT, OUTPUT);

  digitalWrite(P_FAN, FAN_OFF);
  digitalWrite(P_HEAT, HEAT_OFF);
  digitalWrite(P_REV, REV_OFF); // LOW = HEAT, HIGH = COOL
  digitalWrite(P_COOL, COOL_OFF);
  digitalWrite(P_HUMID, HUMID_OFF); // LOW = ON
#endif
}

void HVAC::init()
{
  m_setMode = ee.b.Mode;
  if(ee.b.Mode) m_modeShadow = ee.b.Mode;
  m_idleTimer = ee.idleMin - 60; // about 1 minute
  m_setHeat = ee.b.heatMode;
  m_filterMinutes = ee.filterMinutes; // save a few EEPROM writes

#ifndef REMOTE
  m_Sensor[0].IP= 192 | 168<<8 | 1<<24; // Setup sensor 0 as internal sensor
  m_Sensor[0].ID = 'NTNI';
  m_Sensor[0].flags = SNS_EN;
#endif
}

// Failsafe: shut everything off
void HVAC::disable()
{
#ifndef REMOTE
  digitalWrite(P_HEAT, HEAT_OFF);
  digitalWrite(P_COOL, COOL_OFF);
  digitalWrite(P_HUMID, HUMID_OFF);
  fanSwitch(false);
  m_bHumidRunning = false;
  m_bRunning = false;
  m_bEnabled = false;
#endif
}

bool HVAC::getFanRunning()
{
  return (m_bRunning || m_furnaceFan || m_bFanRunning);
}

bool HVAC::getHumidifierRunning()
{
  return m_bHumidRunning;
}

uint8_t HVAC::getMode()
{
  return ee.b.Mode;
}

int8_t HVAC::getAutoMode()
{
  return m_AutoMode;
}

int8_t HVAC::getSetMode()
{
  return m_setMode;
}

void HVAC::enableRemote()
{
#ifdef REMOTE // remote sensor priority toggle
  m_bRemoteStream = !m_bRemoteStream;
  sendCmd("rmtflg", m_bRemoteStream ? SNS_PRI : (SNS_NEG|SNS_PRI)); // set or unset priority
#else
  if(m_bRemoteStream) // remote sensor stop
  {
    m_bRemoteDisconnect = true;
    m_bRemoteStream = false;
    m_notif = Note_None;
  }
#endif
}

void HVAC::updateVar(int iName, int iValue)// host values (sent to remote)
{
#ifdef REMOTE

  switch(iName)
  {
    case 0: // r
      m_bRunning = iValue;
      break;
    case 1: // fr
      m_bFanRunning = iValue;
      break;
    case 2: // s
      break;
    case 3: // it
      m_inTemp = iValue;
      break;
    case 4: // rh
      m_rh = iValue;
      break;
    case 5: // tt
      m_targetTemp = iValue;
      break;
    case 6: // fm
      ee.filterMinutes = iValue;
      break;
    case 7: // outTemp
      break;
    case 8: // outmin
      break;
    case 9: // outmax
      break;
    case 10: // ct
      m_cycleTimer = iValue;
      break;
    case 11: // ft
      m_fanOnTimer = iValue;
      break;
    case 12: // rt
      m_runTotal = iValue;
      break;
    case 13: // h
      m_bHumidRunning = iValue;
      break;
    case 14: // lt (localTemp on host)
      break;
    case 15: // lh (local Rh)
      break;
    case 16: // rmt
      m_bRemoteStream = false; // command to kill remote temp send
      break;
  }
#endif
}

// Remote: send a command as JSON: cmd {key:password, command:value}
void HVAC::sendCmd(const char *szName, int value)
{
#ifdef REMOTE
  jsonString js("cmd");
  js.Var("key", ee.password);
  js.Var((char *)szName, value);
  WscSend(js.Close());
#endif
}

void HVAC::enable()
{
#ifndef REMOTE
  m_bEnabled = true;
  m_bRecheck = true;
#endif
}

int8_t HVAC::getFan()
{
  return m_FanMode;
}

// User:Set fan mode
void HVAC::setFan(int8_t m)
{
#ifdef REMOTE
  if(m == m_FanMode || !bValidData)  // requested fan operating mode change
    return;

  sendCmd("fanmode", m);
  m_FanMode = m;
#else
  if(m == m_FanMode)     // requested fan operating mode change
    return;

  m_FanMode = m;
  if(!m_bRunning)
    fanSwitch(m == FM_On ? true:false); // manual fan on/off if not running
#endif
}

// returns filter over 200 hours
bool HVAC::checkFilter(void)
{
  return (m_filterMinutes >= 60*200);
}

// Update outdoor temp
void HVAC::updateOutdoorTemp(int16_t outTemp)
{
  m_outTemp = outTemp;
}

int16_t HVAC::getSetTemp(int mode, int hl)
{
  switch(mode)
  {
    case Mode_Cool:
      return ee.coolTemp[hl];
    case Mode_Heat:
      return ee.heatTemp[hl];
    case Mode_Auto:
      return (m_AutoMode == Mode_Cool) ? ee.coolTemp[hl] : ee.heatTemp[hl];
  }
  return 0;
}

// Accumulate fan running times
void HVAC::filterInc()
{
#ifndef REMOTE
  static uint32_t nSecs = 0;

  nSecs ++;  // add last run time to total counter
  if(nSecs >= 60)    // increment filter minutes
  {
    m_filterMinutes++;
    nSecs -= 60;     // and subtract a minute
  }
#endif
}

// Service: called once per second
void HVAC::service()
{
#ifdef REMOTE
  tempCheck();

  if(!bValidData) // hasn't gotten data from host yet
    return;
  static uint16_t old[4];
  if(m_remoteTimer) // let user change values for some time before sending
  {
    if(--m_remoteTimer == 0)
    {
      if(old[0] != ee.coolTemp[0])  sendCmd("cooltempl", old[0] = ee.coolTemp[0]); 
      if(old[1] != ee.coolTemp[1])  sendCmd("cooltemph", old[1] = ee.coolTemp[1]);
      if(old[2] != ee.heatTemp[0])  sendCmd("heattempl", old[2] = ee.heatTemp[0]);
      if(old[3] != ee.heatTemp[1])  sendCmd("heattemph", old[3] = ee.heatTemp[1]);

      if(ee.b.heatMode != m_setHeat)  sendCmd("heatmode", ee.b.heatMode = m_setHeat);
      if(ee.b.Mode != m_setMode)      sendCmd("mode", ee.b.Mode = m_setMode);
    }
  }
#else
  if(m_bFanRunning || m_bRunning || m_furnaceFan)  // furance runs fan seperately
  {
    filterInc();
    m_fanOnTimer++;     // running time counter

    if(m_furnaceFan)      // fake fan status for furnace fan
      m_furnaceFan--;
    m_fanIdleTimer = 0;   // fan auto run timer reset
  }

  if(ee.b.Mode && !m_bRunning && !m_bFanRunning)
  {
    m_fanIdleTimer++;               // fan not running timer
    if(ee.fanIdleMax && ee.fanAutoRun && m_fanIdleTimer >= ee.fanIdleMax * 60)
    {
      fanSwitch(true);
      m_fanIdleTimer = 0;
      m_fanAutoRunTimer = ee.fanAutoRun * 60;
    }
  }

  if(m_fanAutoRunTimer)
  {
    if(--m_fanAutoRunTimer == 0)
      fanSwitch(false);
  }

  if(m_bHumidRunning)
    m_humidTimer++;

  if(m_fanPostTimer)                // Fan continuation delay
  {
    if(--m_fanPostTimer == 0)
      if(!m_bRunning && m_FanMode != FM_On) // Ensure system isn't running and fanMode is auto
        fanSwitch(false);
  }

  if(m_overrideTimer)       // User temp override timer
  {
    if(--m_overrideTimer == 0)
    {
      m_ovrTemp = 0;
      calcTargetTemp(m_modeShadow);  // recalc normal set temp
      m_bAway = false;
    }
  }

  for(int8_t i = 0; i < SNS_CNT; i++) // sensor priority timer countdown
  {
    if(m_Sensor[i].timer)
    {
      if(--m_Sensor[i].timer == 0)
      {
        if(m_Sensor[i].flags & SNS_TOPRI)
          m_Sensor[i].flags &= ~SNS_PRI;
        else
          m_Sensor[i].flags &= ~SNS_EN;
      }
    }
  }

  if(m_bRunning)
  {
    m_runTotal++;
    if(++m_cycleTimer < 20)           // Block changes for at least 20 seconds after a start
      return;
    if(m_cycleTimer >= ee.cycleMax)   // running too long (todo: skip for eHeat?)
    {
      m_bStop = true;
      m_notif = Note_CycleLimit; // cycle limit hit
    }
  }
  else
  {
    m_idleTimer++;                     // Time since stopped
  }

  if(m_setMode != ee.b.Mode || m_setHeat != ee.b.heatMode)    // requested HVAC mode change
  {
    if(m_bRunning)                   // cycleTimer is already > 20s here
      m_bStop = true;
    else if(m_idleTimer >= 5)        // User may be cycling through modes (give 5s)
    {
      ee.b.heatMode = m_setHeat;
      ee.b.Mode = m_setMode;
      if(ee.b.Mode != Mode_Off)
        m_modeShadow = ee.b.Mode;
      calcTargetTemp(m_modeShadow);
    }
  }

  int8_t hm = (ee.b.heatMode == Heat_Auto) ? m_AutoHeat : ee.b.heatMode; // true heat mode
  int8_t mode = (ee.b.Mode == Mode_Auto) ? m_AutoMode : ee.b.Mode;      // true heat/cool mode

  if(m_bStart && !m_bRunning)             // Start signal occurred
  {
    m_bStart = false;
    m_fanAutoRunTimer = 0; // disable fan autorun

    switch(mode)
    {
      case Mode_Cool:
        fanSwitch(true);
        if(digitalRead(P_REV) != REV_ON)
        {
          digitalWrite(P_REV, REV_ON);  // set heatpump to cool (if heats, reverse this)
          delay(2000);                //   if no heatpump, remove
        }
        digitalWrite(P_COOL, COOL_ON);
        m_bRunning = true;
        break;
    case Mode_Heat:
        if(hm == Heat_NG)  // gas
        {
          if(digitalRead(P_COOL) == COOL_ON)
            WsSend("print;Error: NG heat start conflict");
          else
            digitalWrite(P_HEAT, HEAT_ON);
        }
        else
        {
          fanSwitch(true);
          if( digitalRead(P_HEAT) == HEAT_OFF )
          {
            if(digitalRead(P_REV) != REV_OFF)  // set heatpump to heat (if cools, reverse this)
            {
              digitalWrite(P_REV, REV_OFF);
              delay(1000);
            }
            digitalWrite(P_COOL, COOL_ON);
          }
        }
        m_bRunning = true;
        break;
    }
    if(ee.b.humidMode == HM_Run)
      humidSwitch(true);
  }

  if(m_bStop && m_bRunning)             // Stop signal occurred
  {
    m_bStop = false;
    digitalWrite(P_COOL, COOL_OFF);
    digitalWrite(P_HEAT, HEAT_OFF);

    costAdd(m_cycleTimer, mode, hm);
    m_cycleTimer = 0;

    if(ee.b.humidMode == HM_Run)      // shut off after heat/cool phase
      humidSwitch(false);

    if(m_bFanRunning && m_FanMode != FM_On ) // Note: furnace manages fan
    {
      if(ee.fanPostDelay[digitalRead(P_REV)])         // leave fan running to circulate air longer
        m_fanPostTimer = ee.fanPostDelay[digitalRead(P_REV)]; // P_REV == true if heating
      else
        fanSwitch(false);
    }
  
    if(mode == Mode_Heat && hm)   // count run time as fan time in winter
    {                             // furnace post fan is 120 seconds
      m_furnaceFan = ee.furnacePost;
    }

    m_bRunning = false;
    m_idleTimer = 0;
  }

  tempCheck();
#endif // !REMOTE
}

bool HVAC::tempChange()
{
#ifdef REMOTE
  static uint16_t nTemp = 0;

  if(nTemp == m_localTemp)
    return false;

  nTemp = m_localTemp;
  return true;
#else
  static uint16_t nTemp = 0;
  static uint16_t nTarget = 0;
  bool bRet = false;

  if(nTemp != m_inTemp || nTarget != m_targetTemp)
  {
    nTemp = m_inTemp;
    nTarget = m_targetTemp;
    bRet = true;
  }
  return bRet;
#endif
}

// Control switching of system by temp
void HVAC::tempCheck()
{
#ifndef REMOTE
  if(m_bEnabled == false)    // hasn't been set yet
    return;

  int8_t mode = (ee.b.Mode == Mode_Auto) ? m_AutoMode : ee.b.Mode;

  int16_t tempL = 0;
  int16_t tempH = 0;
  int8_t sensCnt = 0;
  int16_t sensTemp = 0;
  int16_t sensRh = 0;
  bool  remSens = false;

  for(int8_t i = 0; i < SNS_CNT; i++)
  {
    if(m_Sensor[i].IP)
    {
      if(now() - m_Sensor[i].tm > 70) // disregard expired sensor data
      {
        if( (m_Sensor[i].flags & SNS_WARN) == 0)
        {
          m_Sensor[i].flags |= SNS_WARN;
          String s = "print;";
          s += (char*)&m_Sensor[i].ID;
          s += " sensor data expired";
          WsSend(s);
        }
        // Just ingore for the moment
        if(now() - m_Sensor[i].tm > 5*60) // 5 minutes
        {
          if(i) m_Sensor[i].IP = 0; // kill it
          remSens = true;
        }
      }
      else if( (m_Sensor[i].flags & (SNS_PRI | SNS_EN)) == (SNS_PRI | SNS_EN) )
      {
         m_Sensor[i].flags &= ~SNS_WARN;
         sensTemp = m_Sensor[i].temp;
         sensRh = m_Sensor[i].rh;
         sensCnt = 1;
         break;
      }
      else if(m_Sensor[i].flags & SNS_EN)
      {
         m_Sensor[i].flags &= ~SNS_WARN;
         sensTemp += m_Sensor[i].temp;
         sensRh += m_Sensor[i].rh;
         sensCnt++;
      }
    }
  }
  if(sensCnt > 1)
  {
    sensTemp /= sensCnt; // average
    sensRh /= sensCnt;
  }
  else if(sensCnt == 0) // make sure internal sensor and others aren't disabled by webpage
  {
    sensTemp = m_Sensor[0].temp;
    sensRh = m_Sensor[0].rh;
    m_Sensor[0].flags = SNS_EN;
  }

  tempL = tempH = sensTemp;
  m_inTemp = sensTemp;
  m_rh = sensRh;

  if(m_bRunning)
  {
    if(m_cycleTimer < ee.cycleMin)
      return;

    if(second() == 0 || m_bRecheck) // readjust while running
    {
      m_bRecheck = false;
      preCalcCycle(tempL, tempH);
    }

    switch(mode)
    {
      case Mode_Cool:
        if( tempL <= m_targetTemp - ee.cycleThresh[0]) // has cooled to desired temp - threshold
          m_bStop = true;
        break;
      case Mode_Heat:
        if(tempH >= m_targetTemp + ee.cycleThresh[1]) // has heated to desired temp + threshold
          m_bStop = true;
        break;
    }
  }
  else  // not running
  {
    if(m_fanPreTimer) // fan will circulate for the set time before going to actual heat/cool
    {
      bool bHit = false;
      if(mode == Mode_Cool)
      {
        if( tempL <= m_targetTemp - ee.cycleThresh[0]) // has cooled to desired temp - threshold
          bHit = true;
      }
      if(bHit) // fan hit threshold
      {
        if(m_FanMode != FM_On)
          fanSwitch(false);
        m_fanPreElap = 0;
        m_fanPreTimer = 0;
      }
      else if(--m_fanPreTimer == 0) // timed out, didn't hit threshold (Mode_Cool)
      {
        m_bStart = true;  // start the cycle
        return;
      }
    }

    if(m_idleTimer < ee.idleMin || m_fanPreTimer)
      return;

    if(m_fanPreElap < 60*30) // how long since pre-cycle fan has run (if it does)
      m_fanPreElap++;

    if(second() == 0 || m_bRecheck)
    {
      m_bRecheck = false;
      if(m_bStart = preCalcCycle(tempL, tempH))
      {
        if( m_bFanRunning == false)
        {
          uint16_t t = ee.fanPreTime[mode == Mode_Heat];
          if(t && m_fanPreElap > ee.idleMin) // try to use fan to adjust temp first
          {
            m_fanPreTimer = t;
            fanSwitch(true);
            m_bStart = false;
          }
        } // else start immediately if fan running
      }
    }
  }
  if(remSens)
    shiftSensors();
#endif
}

uint8_t HVAC::getState()
{
  if( m_bRunning == false) return 0;

  // Check if NG furnace is running, which controls the fan automatically
  uint8_t state = (ee.b.Mode == Mode_Auto) ? m_AutoMode : ee.b.Mode; // convert auto to just cool / heat

  if(state == Mode_Heat && ( ee.b.heatMode == Heat_NG || (ee.b.heatMode == Heat_Auto && m_AutoHeat == Heat_NG) ) )  // convert any NG mode to 3
    state = 3; // so logs will only be 1, 2 or 3.

  return state;
}

bool HVAC::stateChange()
{
  static bool bFan = false;
  static uint8_t lastMode = 0;
  static uint8_t nState = 0;

#ifdef REMOTE
  if(getMode() != lastMode || getState() != nState || bFan != getFanRunning())   // erase prev highlight
#else
  if(getMode() != lastMode || getState() != nState || bFan != getFanRunning() || m_bRemoteDisconnect)
#endif
  {
    lastMode = getMode();
    nState = getState();
    bFan = getFanRunning();
    return true;
  }
  return false;
}

uint8_t HVAC::getHeatMode()
{
#ifdef REMOTE
  return m_setHeat; // for faster visual update
#else
  return ee.b.heatMode;
#endif
}

void HVAC::setHeatMode(int mode)
{
#ifdef REMOTE
  m_setHeat = mode % 3;
  m_remoteTimer = 2;
#else
  m_setHeat = mode % 3;
#endif
}

// User:Set a new control mode
void HVAC::setMode(int mode)
{
  m_setMode = mode % 5;

#ifdef REMOTE
  m_remoteTimer = 2;
#else
  if(!m_bRunning)
  {
    if(m_setMode == Mode_Off && m_FanMode != FM_On)
    {
      fanSwitch(false); // fan may be on
      m_fanPreElap = 0;
      m_fanPreTimer = 0;
    }
    if(m_idleTimer < ee.idleMin - 60)
      m_idleTimer = ee.idleMin - 60;        // shorten the idle time
    if(m_idleTimer >= ee.idleMin)
      m_idleTimer = ee.idleMin - 10;        // but at least 10 seconds so mode can be chosen
  }
#endif
}

#ifndef REMOTE

// Switch the fan on/off
void HVAC::fanSwitch(bool bOn)
{
  if(bOn == m_bFanRunning)
    return;

  digitalWrite(P_FAN, bOn ? FAN_ON:FAN_OFF);
  m_bFanRunning = bOn;
  if(bOn)
  {
    if(ee.b.humidMode == HM_Fan) // run humidifier when fan is on
        humidSwitch(true);
  }
  else
  {
    m_iSecs[2] += m_fanOnTimer;
    costAdd(m_fanOnTimer, Mode_Fan, 0);
    m_fanOnTimer = 0;       // reset fan on timer
    humidSwitch(false);
  }
}

void HVAC::humidSwitch(bool bOn)
{
  if(m_bHumidRunning == bOn) return;
  digitalWrite(P_HUMID, bOn ? HUMID_ON:HUMID_OFF); // turn humidifier on
  m_bHumidRunning = bOn;
  if(bOn)
    m_humidTimer++;
  else
  {
    costAdd(m_humidTimer, Mode_Humid, 0);
    m_humidTimer = 0;
  }
}

void HVAC::costAdd(int secs, int mode, int hm)
{
  switch(mode)
  {
    case Mode_Cool:
      m_iSecs[0] += secs;
      break;
    case Mode_Heat:
      switch(hm)
      {
        case Heat_HP:
          m_iSecs[0] += secs;
          break;
        case Heat_NG:
          m_iSecs[1] += secs;
          break;
      }
      break;
    case Mode_Fan:
      break;
    case Mode_Humid:
      break;
  }
  dayTotals(day() - 1);
  monthTotal(month() - 1, day());
}

bool HVAC::preCalcCycle(int16_t tempL, int16_t tempH)
{
  bool bRet = false;

  // Standard triggers for now
  switch(ee.b.Mode)
  {
    case Mode_Off:
      calcTargetTemp(m_modeShadow); // use last heat/cool mode to calc target
      break;
    case Mode_Cool:
      calcTargetTemp(Mode_Cool);
      bRet = (tempH >= m_targetTemp);    // has reached threshold above desired temp
      break;
    case Mode_Heat:
      calcTargetTemp(Mode_Heat);
      bRet = (tempL <= m_targetTemp);
      if(ee.b.heatMode == Heat_Auto)
      {
        if(m_outTemp < ee.eHeatThresh * 10)  // Use gas when efficiency too low for pump
          m_AutoHeat = Heat_NG;
        else
          m_AutoHeat = Heat_HP;
      }
      break;
    case Mode_Auto:
      if(tempH >= ee.coolTemp[0])
      {
        calcTargetTemp(Mode_Cool);
        m_AutoMode = Mode_Cool;
        bRet = (tempH >= m_targetTemp);    // has reached threshold above desired temp
      }
      else if(tempL <= ee.heatTemp[1])
      {
        m_AutoMode = Mode_Heat;
        calcTargetTemp(Mode_Heat);
        if(ee.b.heatMode == Heat_Auto)
        {
          if(m_outTemp < ee.eHeatThresh * 10)  // Use gas when efficiency too low for pump
            m_AutoHeat = Heat_NG;
          else
            m_AutoHeat = Heat_HP;
        }
        bRet = (tempL <= m_targetTemp);
      }
      break;
  }
  return bRet;
}

void HVAC::calcTargetTemp(int mode)
{
  if(!m_bRunning)
  {
    if(digitalRead(P_REV) == REV_OFF && (mode == Mode_Cool) )  // set heatpump to cool if cooling
      digitalWrite(P_REV, REV_ON);
    else if(digitalRead(P_REV) == REV_ON && (mode == Mode_Heat) )  // set heatpump to heat if heating
      digitalWrite(P_REV, REV_OFF);
  }
  int16_t L = m_outMin * 10;
  int16_t H = m_outMax * 10;

  if( H-L == 0) // divide by 0
    return;

  switch(mode)
  {
    case Mode_Off:
    case Mode_Cool:
      m_targetTemp = (m_outTemp-L) * (ee.coolTemp[1]-ee.coolTemp[0]) / (H-L) + ee.coolTemp[0];
      m_targetTemp = constrain(m_targetTemp, ee.coolTemp[0], ee.coolTemp[1]); // just for safety
      if(m_targetTemp < m_outTemp - ee.diffLimit) // increase to differential limit
        m_targetTemp = m_outTemp - ee.diffLimit;
      break;
    case Mode_Heat:
      m_targetTemp = (m_outTemp-L) * (ee.heatTemp[1]-ee.heatTemp[0]) / (H-L) + ee.heatTemp[0];
      m_targetTemp = constrain(m_targetTemp, ee.heatTemp[0], ee.heatTemp[1]); // just for safety
      break;
  }

  m_targetTemp += m_ovrTemp; // override/away is normally 0, unless set remotely with a timeout

  switch(mode)
  {
    case Mode_Off:
    case Mode_Cool:
      m_targetTemp = constrain(m_targetTemp, (ee.b.bCelcius ? 180:650), (ee.b.bCelcius ? 370:980) ); // more safety (after override/away of up to +/-15)
      break;
    case Mode_Heat:
      m_targetTemp = constrain(m_targetTemp, (ee.b.bCelcius ? 150:590), (ee.b.bCelcius ? 300:860) );
      break;
  }
}

// Current control settings modified since last call
String HVAC::settingsJsonMod()
{
  static eeSet eeOld;
  bool bSend = false;
  static int8_t AutoMode, FanMode, RemoteFlags, ovrTemp;

  if(AutoMode != m_AutoMode || FanMode != m_FanMode || ovrTemp != m_ovrTemp)
  {
    AutoMode = m_AutoMode; FanMode = m_FanMode; ovrTemp = m_ovrTemp;
    bSend = true;
  }

  if( memcmp(&eeOld, &ee, sizeof(eeSet)) )
  {
    memcpy(&eeOld, &ee, sizeof(eeSet));
    bSend = true;
  }
  return bSend ? settingsJson() : "";
}
#endif

#ifdef REMOTE
#define min(a,b) ((a) < (b) ? (a) : (b)) // Why???
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

// User:Set new control temp
void HVAC::setTemp(int mode, int16_t Temp, int hl)
{
  if(mode == Mode_Auto)
    mode = m_AutoMode;
  int8_t save;

#ifdef REMOTE
  m_remoteTimer = 2; // 2 second hold before transmit
#endif
  switch(mode)
  {
    case Mode_Off:        // keep a value at least
#ifndef REMOTE
      calcTargetTemp(m_modeShadow);
#endif
      break;
 
    case Mode_Cool:
      if(Temp < (ee.b.bCelcius ? 180:650) || Temp > (ee.b.bCelcius ? 350:950) )    // ensure sane values
        break;
      ee.coolTemp[hl] = Temp;
      if(hl)
      {
        ee.coolTemp[0] = min(ee.coolTemp[1], ee.coolTemp[0]);     // don't allow h/l to invert
      }
      else
      {
        ee.coolTemp[1] = max(ee.coolTemp[0], ee.coolTemp[1]);
      }
      save = ee.heatTemp[1] - ee.heatTemp[0];
      ee.heatTemp[1] = min(ee.coolTemp[0] - (ee.b.bCelcius ? 11:20), ee.heatTemp[1]); // Keep 2.0 degree differential for Auto mode
      ee.heatTemp[0] = ee.heatTemp[1] - save;                      // shift heat low by original diff

#ifndef REMOTE
      if(ee.b.Mode == Mode_Cool)
        calcTargetTemp(ee.b.Mode);
#endif
      break;
    case Mode_Heat:
      if(Temp < (ee.b.bCelcius ? 170:630) || Temp > (ee.b.bCelcius ? 360:860) )    // ensure sane values
        break;
      ee.heatTemp[hl] = Temp;
      if(hl)
      {
        ee.heatTemp[0] = min(ee.heatTemp[1], ee.heatTemp[0]);
      }
      else
      {
        ee.heatTemp[1] = max(ee.heatTemp[0], ee.heatTemp[1]);
      }
      save = ee.coolTemp[1] - ee.coolTemp[0];
      ee.coolTemp[0] = max(ee.heatTemp[1] - (ee.b.bCelcius ? 11:20), ee.coolTemp[0]);
      ee.coolTemp[1] = ee.coolTemp[0] + save;
#ifndef REMOTE
      if(ee.b.Mode == Mode_Heat)
        calcTargetTemp(ee.b.Mode);
#endif
      break;
  }
}

// Update when DHT22/SHT21 changes
void HVAC::updateIndoorTemp(int16_t Temp, int16_t rh)
{
#ifdef REMOTE
  m_localTemp = Temp + ee.adj;
  m_localRh = rh;

  if( m_bRemoteStream )
  {
    m_inTemp = Temp + ee.adj;
    m_rh = rh;
  }

  static int16_t oldTemp;
  static int16_t oldRh;
  static uint32_t secs;

  if(m_localTemp != oldTemp || m_localRh != oldRh || now() - secs > 30)
  {
    oldTemp = m_localTemp;
    oldRh = m_localRh;
    secs = now();
    sendCmd("rmtname", '1TMR'); // RMT1
    sendCmd("rmttemp", m_localTemp);
    sendCmd("rmtrh", m_localRh);
  }
#else
  m_Sensor[0].temp = Temp + ee.adj;
  m_Sensor[0].rh = rh;
  m_Sensor[0].tm = now();

  // Auto1 == auto humidifier when running, Auto2 = even when not running (turns fan on)
  if(ee.b.humidMode >= HM_Auto1)
  {
    if(m_bHumidRunning)
    {
      if(m_rh >= ee.rhLevel[1]) // reached high
      {
        humidSwitch(false);
        if(m_bRunning == false && m_FanMode != FM_On)  // if not cooling/heating we can turn the fan off
        {
          fanSwitch(false);
          if(m_idleTimer > ee.idleMin)
            m_idleTimer = 0; // reset main idle timer if it's high enough (not well thought out)
        }
      }
    }
    else // not running
    {
      if(m_rh < ee.rhLevel[0]) // heating and cooling both reduce humidity
      {
        if(ee.b.humidMode == HM_Auto1 && m_bRunning == false); // do nothing
        else
        {
          humidSwitch(true);
          fanSwitch(true); // will use fan if not running
        }
      }
      else if(m_bRunning == false && m_rh > ee.rhLevel[1] + 5 &&  ee.b.humidMode == HM_Auto2 && ee.b.Mode == Mode_Cool)
      {  // humidity over desired level, use compressor to reduce (will run at least for cycleMin)
          if(m_idleTimer > ee.idleMin)
            m_bStart = true;
      }
    }
  }
#endif
}

void HVAC::resetFilter()
{
  m_filterMinutes = 0;
  if(m_notif == Note_Filter)
    m_notif = Note_None;
#ifdef REMOTE
  sendCmd("resetfilter", 0);
#endif
}

void HVAC::resetTotal()
{
  m_runTotal = 0;

#ifdef REMOTE
  sendCmd("resettotal", 0);
#endif
}

// Current control settings
String HVAC::settingsJson()
{
  jsonString js("settings");
  js.Var("m", ee.b.Mode);
#ifndef REMOTE
  js.Var("am", m_AutoMode);
  js.Var("hm", ee.b.heatMode);
  js.Var("fm", m_FanMode);
  js.Var("ot", m_ovrTemp);
  js.Var("ht", ee.eHeatThresh);
  js.Var("c0", ee.coolTemp[0]);
  js.Var("c1", ee.coolTemp[1]);
  js.Var("h0", ee.heatTemp[0]);
  js.Var("h1", ee.heatTemp[1]);
  js.Var("im", ee.idleMin);
  js.Var("cn", ee.cycleMin);
  js.Var("cx", ee.cycleMax);
  js.Var("ct", ee.cycleThresh[m_modeShadow == Mode_Heat]);
  js.Var("fd", ee.fanPostDelay[digitalRead(P_REV)]);
  js.Var("ov", ee.overrideTime);
  js.Var("rhm", ee.b.humidMode);
  js.Var("rh0", ee.rhLevel[0]);
  js.Var("rh1", ee.rhLevel[1]);
  js.Var("fp",  ee.fanPreTime[m_modeShadow == Mode_Heat]);
  js.Var("fct", 0);
  js.Var("at",  ee.awayTime);
  js.Var("ad",  ee.awayDelta[m_modeShadow == Mode_Heat]);
  js.Var("fcr", ee.fcRange);
  js.Var("fcd", ee.fcDisplay);
  js.Var("cw",  ee.compressorWatts);
  js.Var("fw",  ee.fanWatts);
  js.Var("frnw", ee.furnaceWatts);
  js.Var("hfw", ee.humidWatts);
  js.Var("ffp", ee.furnacePost);
  js.Var("fco", ee.fcOffset[m_modeShadow == Mode_Heat]);
  js.Var("fim", ee.fanIdleMax);
  js.Var("far", ee.fanAutoRun);
  js.Var("sm", ee.b.nSchedMode);
  js.Var("tu", ee.b.bCelcius);
  js.Var("lock", ee.b.bLock);
#endif
  js.Var("ppk", ee.ppkwh);
  js.Var("ccf", ee.ccf);
  js.Var("cfm", ee.cfm);
  js.Var("dl",  ee.diffLimit);
  return js.Close();
}

// Constant changing values
String HVAC::getPushData()
{
  jsonString js("state");
  js.Var("t", (long)now() - ((ee.tz+m_DST) * 3600));
  js.Var("r", m_bRunning);
  js.Var("fr", getFanRunning() );
  js.Var("it", m_inTemp);
  js.Var("ct", m_cycleTimer );
#ifdef REMOTE
  js.Var("tempi", m_localTemp );
  js.Var("rhi", m_localRh );
#else
  js.Var("s" , getState() );
  js.Var("rh", m_rh);
  js.Var("tt", m_targetTemp);
  js.Var("fm", m_filterMinutes);
  js.Var("ot", m_outTemp);
  js.Var("ol", m_outMin);
  js.Var("oh", m_outMax);
  js.Var("ft", m_fanOnTimer);
  js.Var("rt", m_runTotal);
  js.Var("h",  m_bHumidRunning);
  js.Var("aw", m_bAway);

  js.Array("snd", m_Sensor);
#endif
  return js.Close();
}

#ifndef REMOTE
const char *cmdList[] = { "cmd",
  "key",
  "data",
  "sum",
  "bin",

  "fanmode", // 0
  "mode",
  "heatmode",
  "resettotal",
  "resetfilter",
  "fanpostdelay",
  "cyclemin",
  "cyclemax",
  "idlemin",
  "cyclethresh",
  "cooltempl", // 10
  "cooltemph",
  "heattempl",
  "heattemph",
  "eheatthresh",
  "override",
  "overridetime",
  "humidmode",
  "humidl",
  "humidh",
  "adj",      // 20
  "fanpretime",
  "fim",
  "fco",
  "awaytime",
  "awaydelta",
  "away",
  "ppk",
  "ccf",
  "cfm",
  "lock",       // 30
  "far",
  "fcrange",
  "fcdisp",
  "save",
  "tz",
  "cw",
  "fw",
  "frnw",
  "hfw",
  "ffp",    // 40
  "dl",
  "play",
  "tu",
  "rmtid",
  "rmttemp",
  "rmtrh",
  "rmtflg",
  "rmtname",
  "rmtto",
  "sm", // 50
  NULL
};

int HVAC::CmdIdx(String s )
{
  int iCmd;
  // skip the top 4 (event, key, data)
  for(iCmd = 5; cmdList[iCmd]; iCmd++)
  {
    if( s.equalsIgnoreCase( String(cmdList[iCmd]) ) )
      break;
  }
  return iCmd - 5;
}

#endif

// WebSocket or GET/POST set params as "fanmode=1" or "fanmode":1
void HVAC::setVar(String sCmd, int val, char *psValue, IPAddress ip)
{
#ifndef REMOTE
  static uint8_t snsIdx; // current sensor in use

  int c = CmdIdx( sCmd );
  if(ee.b.bLock && c!= 51)
    return;
  int i;

  switch( c )
  {
    case 0:     // fanmode
      setFan( val );
      break;
    case 1:     // mode
      setMode( val );
      break;
    case 2:     // heatmode
      setHeatMode( val );
      break;
    case 3:     // resettotal
      resetTotal();
      break;
    case 4:
      resetFilter();
      break;
    case 5:     // fanpostdelay
      ee.fanPostDelay[digitalRead(P_REV)] = constrain(val, 0, 60*5); // Limit 0 to 5 minutes
      break;
    case 6:     // cyclemin
      ee.cycleMin = constrain(val, 60, 60*20); // Limit 1 to 20 minutes
      break;
    case 7:     // cyclemax
      ee.cycleMax = constrain(val, 60*2, 60*60); // Limit 2 to 60 minutes
      break;
    case 8:     // idlemin
      ee.idleMin = constrain(val, 60, 60*30); // Limit 1 to 30 minutes
      break;
    case 9:    // cyclethresh
      ee.cycleThresh[ee.b.Mode == Mode_Heat] = constrain(val, (ee.b.bCelcius ? 2:5), (ee.b.bCelcius ? 28:50) ); // Limit 0.5 to 5.0 degrees
      break;
    case 10:    // cooltempl
      setTemp(Mode_Cool, val, 0);
      m_bRecheck = true; // faster update
      break;
    case 11:    // cooltemph
      setTemp(Mode_Cool, val, 1);
      m_bRecheck = true;
      break;
    case 12:    // heattempl
      setTemp(Mode_Heat, val, 0);
      m_bRecheck = true;
      break;
    case 13:    // heattemph
      setTemp(Mode_Heat, val, 1);
      m_bRecheck = true;
      break;
    case 14:    // eheatthresh
      ee.eHeatThresh = constrain(val, (ee.b.bCelcius ? 2:5), (ee.b.bCelcius ? 28:50) ); // Limit 5 to 50 degrees F
      break;
    case 15:    // override
      if(val == 0)    // cancel
      {
        m_ovrTemp = 0;
        m_overrideTimer = 0;
      }
      else
      {
        m_ovrTemp = constrain(val, (ee.b.bCelcius ? -50:-99),(ee.b.bCelcius ? 50:99) ); // Limit to +/-9.9 degrees F
        m_overrideTimer = ee.overrideTime;
      }
      m_bRecheck = true;
      break;
    case 16:    // overridetime
      ee.overrideTime = constrain(val, 60*1, 60*60*6); // Limit 1 min to 6 hours
      break;
    case 17: // humidmode
      ee.b.humidMode = constrain(val, HM_Off, HM_Auto2);
      break;
    case 18: // humidl
      ee.rhLevel[0] = constrain(val, 300, 900); // no idea really
      break;
    case 19: // humidh
      ee.rhLevel[1] = constrain(val, 300, 900);
      break;
    case 20: // adj
      ee.adj = constrain(val, (ee.b.bCelcius ? -44:-80), (ee.b.bCelcius ? 5:10) ); // calibrate can only be -8.0 to +1.0
      break;
    case 21:     // fanPretime
      ee.fanPreTime[ee.b.Mode == Mode_Heat] = constrain(val, 0, 60*8); // Limit 0 to 8 minutes
      break;
    case 22: // fim
      ee.fanIdleMax = val;
      break;
    case 23: // fco
      ee.fcOffset[ee.b.Mode == Mode_Heat] = constrain(val, -1080, 1079); // 18 hours max
      break;
    case 24: // awaytime
      ee.awayTime = val; // no limit
      break;
    case 25: // awaydelta
      if(ee.b.Mode == Mode_Heat)
        ee.awayDelta[1] = constrain(val, (ee.b.bCelcius ? -83:-150), 0); // Limit to -15 degrees (heat away) target is constrained in calcTargetTemp
      else
        ee.awayDelta[0] = constrain(val, 0, (ee.b.bCelcius ? 83:150) ); // Limit +15 degrees (cool away)
      break;
    case 26: // away (uses the override feature)
      if(val) // away
      {
        m_overrideTimer = ee.awayTime * 60; // convert minutes to seconds
        m_ovrTemp = ee.awayDelta[ee.b.Mode == Mode_Heat];
        m_bAway = true;
      }
      else // back
      {
        m_ovrTemp = 0;
        m_overrideTimer = 0;
        m_bAway = false;
      }
      break;
    case 27:
      ee.ppkwh = val;
      break;
    case 28:
      ee.ccf = val;
      break;
    case 29:
      ee.cfm = val; // CFM / 1000
      break;
    case 30:
      ee.b.bLock = (val) ? 1:0;
      break;
    case 31:
      ee.fanAutoRun = val;
      break;
    case 32: // fcrange
      ee.fcRange = constrain(val, 1, 46);
      break;
    case 33: // fcdisp
      ee.fcDisplay = constrain(val, 1, 46);
      break;
    case 34: // force save
      eemem.update();
      break;
    case 35: // TZ
      ee.tz = constrain(val, -12, 12);
      break;
    case 36: // cw
      ee.compressorWatts = val;
      break;
    case 37: // fw
      ee.fanWatts = val;
      break;
    case 38: // frnw
      ee.furnaceWatts = val;
      break;
    case 39:
      ee.humidWatts = val;
      break;
    case 40:
      ee.furnacePost = val;
      break;
    case 41: // dl
      ee.diffLimit = constrain(val, (ee.b.bCelcius ? 83:150), (ee.b.bCelcius ? 194:350) );
      break;
    case 42: // play
#ifdef USE_AUDIO
      mus.play(val);
#endif
      break;
    case 43: // tu
      ee.b.bCelcius = val ? true:false;
      m_bRecheck = true;
      break;
    case 44: // rmtid (used by web page)
      {
        int i;

        for(i = 0; i < SNS_CNT; i++) // find ID
        {
          if((m_Sensor[i].IP >> 24) == val)
            break;
        }
        if(i < SNS_CNT)  snsIdx = i;
      }
      break;
    case 45: // rmttemp
      snsIdx = getSensorID(ip);
      if((m_Sensor[snsIdx].flags & SNS_C) && ee.b.bCelcius == false) // convert remote units to local units
        val = val * 90 / 50 + 320;
      else if((m_Sensor[snsIdx].flags & SNS_F) && ee.b.bCelcius)
        val = (val - 320) * 50 / 90;
      if(val < (ee.b.bCelcius ? 180:650) || val > (ee.b.bCelcius ? 370:990) )
      {
        String s = "print;";
        s += (char *)&m_Sensor[i].ID;
        s += " sensor range error ";
        s += val;
        WsSend(s);
        m_Sensor[i].flags &= ~(SNS_EN | SNS_PRI); // deactivate sensor        
      }
      else
        m_Sensor[snsIdx].temp = val;
      m_Sensor[snsIdx].tm = now();
      break;
    case 46: // rmtrh
      snsIdx = getSensorID(ip);
      m_Sensor[snsIdx].rh = val;
      break;
    case 47: // rmtflg (uses last referenced rmtid)
      if(val & SNS_NEG)
        m_Sensor[snsIdx].flags &= ~(val & 0x7F);
      else
        m_Sensor[snsIdx].flags |= (val & 0x7F);

      if(m_Sensor[snsIdx].flags & SNS_PRI) // remove priority from other sensors
      {
        for(i = 0; i < SNS_CNT; i++)
          if(i != snsIdx)
            m_Sensor[i].flags &= ~SNS_PRI;
      }
      break;
    case 48: // rmtname
      snsIdx = getSensorID(ip);
      m_Sensor[snsIdx].ID = val;
      if(val == '1TMR' && snsIdx != 1) //swap sensors so RMT1 is top
      {
        swapSensors(1, snsIdx);
        snsIdx = 0;
      }
      break;
    case 49: // rmtto
      snsIdx = getSensorID(ip);
      m_Sensor[snsIdx].timer = val;
      break;
    case 50: // sm
      ee.b.nSchedMode = constrain(val, 0, 2);
      break;
  }
#endif
}

#ifndef REMOTE
void HVAC::swapSensors(int n1, int n2)
{
  Sensor tmp;
  memcpy(&tmp, &m_Sensor[n1], sizeof(Sensor));
  memcpy(&m_Sensor[n1], &m_Sensor[n2], sizeof(Sensor));
  memcpy(&m_Sensor[n2], &tmp, sizeof(Sensor));
}

void HVAC::shiftSensors()
{
  for(int i = 1; i < SNS_CNT - 1; i++)
    if(m_Sensor[i].IP ==  0 && m_Sensor[i+1].IP )
      swapSensors(i, i+1);
}

int HVAC::getSensorID(uint32_t id)
{
  int i;

  for(i = 1; i < SNS_CNT; i++) // find ID
  {
    if(m_Sensor[i].IP == id)
      break;
  }
  if(i == SNS_CNT) // not found
    for(i = 1; i < SNS_CNT; i++)
    {
      if(m_Sensor[i].IP == 0)
        break;
    }
  if(i < SNS_CNT)
  {
    m_Sensor[i].IP = id;
    return i;
  }
  return 1;
}
#endif

void HVAC::dayTotals(int d)
{
#ifndef REMOTE
  ee.iSecsDay[d][0] += m_iSecs[0];
  ee.iSecsDay[d][1] += m_iSecs[1];
  ee.iSecsDay[d][2] += m_iSecs[2];
  m_iSecs[0] = 0;
  m_iSecs[1] = 0;
  m_iSecs[2] = 0;

  jsonString js("update");
  js.Var("type", "day");
  js.Var("e", day() - 1);
  js.Var("d0", ee.iSecsDay[d][0]);
  js.Var("d1", ee.iSecsDay[d][1]);
  js.Var("d2", ee.iSecsDay[d][2]);
  WsSend( js.Close() );
#endif
}

void HVAC::monthTotal(int m, int dys)
{
#ifndef REMOTE
  static const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31};

  uint32_t sec[3] = {0}; // This doesn't clear if not implied
  if(dys == -1) // use days of month
    dys = monthDays[m];
  for(int i = 0; i < dys; i++) // Todo: leap year
  {
    sec[0] += ee.iSecsDay[i][0];
    sec[1] += ee.iSecsDay[i][1];
    sec[2] += ee.iSecsDay[i][2];
  }
  ee.iSecsMon[m][0] = sec[0];
  ee.iSecsMon[m][1] = sec[1];
  ee.iSecsMon[m][2] = sec[2];
#endif
}

void HVAC::setSettings(int iName, int iValue)// remote settings
{
#ifdef REMOTE
  switch(iName)
  {
    case 0:
      m_setMode = ee.b.Mode = iValue;
      if(ee.b.Mode) m_modeShadow = ee.b.Mode;
      bValidData = true;
      break;
    case 1:
      m_AutoMode = iValue;
      break;
    case 2:
      m_setHeat = ee.b.heatMode = iValue;
      break;
    case 3:
      m_FanMode = iValue;
      break;
    case 4:
      m_ovrTemp = iValue;
      break;
    case 5:
      ee.eHeatThresh = iValue;
      break;
    case 6:
      ee.coolTemp[0] = iValue;
      break;
    case 7:
      ee.coolTemp[1] = iValue;
      break;
    case 8:
      ee.heatTemp[0] = iValue;
      break;
    case 9:
      ee.heatTemp[1] = iValue;
      break;
    case 10:
      ee.idleMin = iValue;
      break;
    case 11:
      ee.cycleMin = iValue;
      break;
    case 12:
      ee.cycleMax = iValue;
      break;
    case 13:
      ee.cycleThresh[ee.b.Mode == Mode_Heat] = iValue;
      break;
    case 14:
      break;
    case 15:
      ee.overrideTime = iValue;
      break;
    case 16:
      ee.b.humidMode = iValue;
      break;
    case 17:
      ee.rhLevel[0] = iValue;
      break;
    case 18:
      ee.rhLevel[1] = iValue;
      break;
    case 19: // tu
      ee.b.bCelcius = iValue;
      break;
  }
#endif
}
