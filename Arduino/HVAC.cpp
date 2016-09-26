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

#define FF_DELAY 120            // internal furnace fan post-run delay

HVAC::HVAC()
{
  memset(m_fcData, -1, sizeof(m_fcData)); // invalidate forecast
  m_outTemp = 0;
  m_inTemp = 0;
  m_rh = 0;
  m_bFanRunning = false;
  m_bHumidRunning = false;
  m_FanMode = FM_Auto;    // Auto=0, On=1, S=2
  m_AutoMode = 0;         // cool, heat
  m_setMode = 0;          // new mode request
  m_setHeat = 0;          // new heat mode request
  m_AutoHeat = 0;         // auto heat mode choice
  m_bRunning = false;     // is operating
  m_bStart = false;       // signal to start
  m_bStop = false;        // signal to stop
  m_bRecheck = false;
  m_bEnabled = false;
  m_runTotal = 0;         // time HVAC has been running total since reset
  m_fanOnTimer = 0;       // time fan is running
  m_cycleTimer = 0;       // time HVAC has been running
  m_fanPostTimer = 0;     // timer for delay
  m_overrideTimer = 0;    // countdown for override in seconds
  m_ovrTemp = 0;          // override delta of target
  m_furnaceFan = 0;       // fake fan timer
  m_notif = Note_None;    // Empty
  m_idleTimer = 60*3;     // start with a high idle, in case of power outage
  m_bRemoteStream = false;
  m_bRemoteDisconnect = false;
  m_bLocalTempDisplay = false;
  m_RemoteFlags = RF_RL|RF_RH;
  m_bAway = false;
  m_fanPreElap = 60*10;

  pinMode(P_FAN, OUTPUT);
  pinMode(P_COOL, OUTPUT);
  pinMode(P_REV, OUTPUT);
  pinMode(P_HUMID, OUTPUT);
  pinMode(P_HEAT, OUTPUT);

  digitalWrite(P_HEAT, LOW);
  digitalWrite(P_REV, LOW); // LOW = HEAT, HIGH = COOL
  digitalWrite(P_COOL, LOW);
  digitalWrite(P_HUMID, HIGH); // LOW = ON
  digitalWrite(P_FAN, LOW);
}

void HVAC::init()
{
  m_setMode = ee.Mode;
  m_idleTimer = ee.idleMin - 60; // about 1 minute
  m_setHeat = ee.heatMode;
}

// Switch the fan on/off
void HVAC::fanSwitch(bool bOn)
{
  if(bOn == m_bFanRunning)
    return;

  digitalWrite(P_FAN, bOn ? HIGH:LOW);
  m_bFanRunning = bOn;
  if(bOn)
  {
    m_fanOnTimer = 0;       // reset fan on timer
    if(ee.humidMode == HM_Fan) // run humidifier when fan is on
        humidSwitch(true);
  }
  else
  {
    humidSwitch(false);
  }
}

void HVAC::humidSwitch(bool bOn)
{
  digitalWrite(P_HUMID, bOn ? LOW:HIGH); // turn humidifier on
  m_bHumidRunning = bOn;
}

// Accumulate fan running times
void HVAC::filterInc()
{
  static uint16_t nSecs = 0;
  
  nSecs ++;  // add last run time to total counter
  if(nSecs >= 60)    // increment filter minutes
  {
    ee.filterMinutes++;
    nSecs -= 60;     // and subtract a minute
  }
}

// Failsafe: shut everything off
void HVAC::disable()
{
  digitalWrite(P_HEAT, LOW);
  digitalWrite(P_COOL, LOW);
  digitalWrite(P_HUMID, HIGH);
  fanSwitch(false);
  m_bHumidRunning = false;
  m_bRunning = false;
  m_bEnabled = false;
}

// Service: called once per second
void HVAC::service()
{
  if(m_bFanRunning || m_bRunning || m_furnaceFan)  // furance runs fan seperately
  {
    filterInc();
    if(m_fanOnTimer < 0xFFFF)
      m_fanOnTimer++;               // running time counter

    if(m_furnaceFan)                // fake fan status for furnace fan
      m_furnaceFan--;
  }

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
      calcTargetTemp(ee.Mode);  // recalc normal set temp
      m_bAway = false;
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

  if(m_setMode != ee.Mode || m_setHeat != ee.heatMode)    // requested HVAC mode change
  {
    if(m_bRunning)                     // cycleTimer is already > 20s here
      m_bStop = true;
    else if(m_idleTimer >= 5)
    {
      ee.heatMode = m_setHeat;
      ee.Mode = m_setMode;           // User may be cycling through modes (give 5s)
      calcTargetTemp(ee.Mode);
    }
  }

  int8_t hm = (ee.heatMode == Heat_Auto) ? m_AutoHeat : ee.heatMode; // true heat mode
  int8_t mode = (ee.Mode == Mode_Auto) ? m_AutoMode : ee.Mode;      // tue heat/cool mode

  if(m_bStart && !m_bRunning)             // Start signal occurred
  {
    m_bStart = false;

    switch(mode)
    {
      case Mode_Cool:
        fanSwitch(true);
        if(digitalRead(P_REV) != HIGH)
        {
          digitalWrite(P_REV, HIGH);  // set heatpump to cool (if heats, reverse this)
          delay(3000);               //    if no heatpump, remove
        }
        digitalWrite(P_COOL, HIGH);
        break;
    case Mode_Heat:
        if(hm)  // gas
        {
          digitalWrite(P_HEAT, HIGH);
        }
        else
        {
          fanSwitch(true);
          if(digitalRead(P_REV) != LOW)  // set heatpump to heat (if heats, reverse this)
          {
            digitalWrite(P_REV, LOW);
            delay(3000);
          }
          digitalWrite(P_COOL, HIGH);
        }
        break;
    }
    m_bRunning = true;
    if(ee.humidMode == HM_Run)
      humidSwitch(true);
    m_cycleTimer = 0;
  }

  if(m_bStop && m_bRunning)             // Stop signal occurred
  {
    m_bStop = false;
    digitalWrite(P_COOL, LOW);
    digitalWrite(P_HEAT, LOW);

    if(ee.humidMode == HM_Run)      // shut off after heat/cool phase
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
      m_furnaceFan = FF_DELAY;
    }

    m_bRunning = false;
    m_idleTimer = 0;
  }

  tempCheck();
}

bool HVAC::stateChange()
{
  static bool bFan = false;
  static uint8_t lastMode = 0;
  static uint8_t nState = 0;

  if(getMode() != lastMode || getState() != nState || bFan != getFanRunning() || m_bRemoteDisconnect)
  {
    lastMode = getMode();
    nState = getState();
    bFan = getFanRunning();
    return true;
  }
  return false;
}

bool HVAC::tempChange()
{
  static uint16_t nTemp = 0;
  static uint16_t nTarget = 0;
  
  if(nTemp == m_inTemp && nTarget == m_targetTemp)
    return false;

  nTemp = m_inTemp;
  nTarget = m_targetTemp;
  return true;
}

// Control switching of system by temp
void HVAC::tempCheck()
{
  if(m_inTemp == 0 || m_bEnabled == false)    // hasn't been set yet
    return;

  if(ee.Mode == Mode_Off)   // nothing to do
    return;

  int8_t mode = (ee.Mode == Mode_Auto) ? m_AutoMode : ee.Mode;

  int16_t tempL = m_inTemp;
  int16_t tempH = m_inTemp;

  if(m_bRemoteStream && m_RemoteFlags)
  {
    if(m_RemoteFlags & RF_ML)
      tempL = m_localTemp; // main low
    if((m_RemoteFlags & (RF_RL|RF_ML)) == (RF_RL|RF_ML))
      tempL = (m_inTemp + m_localTemp) / 2; // use both for low
    if(m_RemoteFlags & RF_MH)
      tempH = m_localTemp; // main high
    if((m_RemoteFlags & (RF_RH|RF_MH)) == (RF_RH|RF_MH))
      tempH = (m_inTemp + m_localTemp) / 2; // use both for high
  }

  if(m_bRunning)
  {
    if(m_cycleTimer < ee.cycleMin)
      return;

    if(second() == 0 || m_bRecheck) // readjust while running
    {
      m_bRecheck = false;
      preCalcCycle(ee.Mode);
    }

    switch(mode)
    {
      case Mode_Cool:
        if( tempL <= m_targetTemp - ee.cycleThresh ) // has cooled to desired temp - threshold
          m_bStop = true;
        break;
      case Mode_Heat:
        if(tempH > m_targetTemp + ee.cycleThresh) // has heated above desired temp + threshold
          m_bStop = true;
        break;
    }
  }
  else  // not running
  {
    if(m_fanPreTimer) // fan will circulate for the set time before going to actual heat/cool
    {
      bool bHit = false;
      switch(mode)
      {
        case Mode_Cool:
          if( tempL <= m_targetTemp - ee.cycleThresh ) // has cooled to desired temp - threshold
            bHit = true;
          break;
        case Mode_Heat:
          if(tempH >= m_targetTemp + ee.cycleThresh) // has heated to desired temp + threshold
            bHit = true;
          break;
      }
      if(bHit) // fan hit threshold
      {
        if(m_FanMode != FM_On)
          fanSwitch(false);
        m_fanPreElap = 0;
        m_fanPreTimer = 0;
      }
      else if(--m_fanPreTimer == 0) // timed out, didn't hit threshold
      {
        if(m_FanMode == FM_Cycle)
          fanSwitch(false);
        else
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
      if( m_bStart = preCalcCycle(ee.Mode) && m_bFanRunning == false)
      {
        uint16_t t = ee.fanPreTime[mode == Mode_Heat];
        if(t && m_fanPreElap > ee.idleMin) // try to use fan to adjust temp first
        {
          m_fanPreTimer = t;
          fanSwitch(true);
          m_bStart = false;
        }
      }
    }
    if( (m_FanMode == FM_Cycle) && m_bStart) // fan only cycle mode
    {
      m_bStart = false;
    }
  }
}

bool HVAC::preCalcCycle(int8_t mode)
{
  bool bRet = false;

  int16_t tempL = m_inTemp;
  int16_t tempH = m_inTemp;

  if(m_bRemoteStream && m_RemoteFlags)
  {
    if(m_RemoteFlags & RF_ML)
      tempL = m_localTemp; // main low
    if((m_RemoteFlags & (RF_RL|RF_ML)) == (RF_RL|RF_ML))
      tempL = (m_inTemp + m_localTemp) / 2; // use both for low
    if(m_RemoteFlags & RF_MH)
      tempH = m_localTemp; // main high
    if((m_RemoteFlags & (RF_RH|RF_MH)) == (RF_RH|RF_MH))
      tempH = (m_inTemp + m_localTemp) / 2; // use both for low
  }

  // Standard triggers for now
  switch(mode)
  {
    case Mode_Cool:
      calcTargetTemp(Mode_Cool);
      bRet = (tempH >= m_targetTemp);    // has reached threshold above desired tempu
      break;
    case Mode_Heat:
      calcTargetTemp(Mode_Heat);
      bRet = (tempL <= m_targetTemp);
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
//      Serial.println("Auto heat");
        m_AutoMode = Mode_Heat;
        calcTargetTemp(Mode_Heat);
        if(ee.heatMode == Heat_Auto)
        {
          if(m_outTemp < (ee.eHeatThresh * 10))  // Use gas when efficiency too low for pump
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

void HVAC::calcTargetTemp(int8_t mode)
{
  if(!m_bRunning)
  {
    if(digitalRead(P_REV) == LOW && (mode == Mode_Cool) )  // set heatpump to cool if cooling
      digitalWrite(P_REV, HIGH);
    else if(digitalRead(P_REV) == HIGH && (mode == Mode_Heat) )  // set heatpump to heat if heating
      digitalWrite(P_REV, LOW);
  }
  int16_t L = m_outMin * 10;
  int16_t H = m_outMax * 10;

  switch(mode)
  {
    case Mode_Cool:
      m_targetTemp  = (m_outTemp-L) * (ee.coolTemp[1]-ee.coolTemp[0]) / (H-L) + ee.coolTemp[0];
      m_targetTemp = constrain(m_targetTemp, ee.coolTemp[0], ee.coolTemp[1]); // just for safety
      break;
    case Mode_Heat:
      m_targetTemp  = (m_outTemp-L) * (ee.heatTemp[1]-ee.heatTemp[0]) / (H-L) + ee.heatTemp[0];
      m_targetTemp = constrain(m_targetTemp, ee.heatTemp[0], ee.heatTemp[1]); // just for safety
      break;
  }

  m_targetTemp += m_ovrTemp; // override/away is normally 0, unless set remotely with a timeout

  switch(mode)
  {
    case Mode_Cool:
      m_targetTemp = constrain(m_targetTemp, 650, 990); // more safety (after override/away of up to +/-15)
      break;
    case Mode_Heat:
      m_targetTemp = constrain(m_targetTemp, 590, 860);
      break;
  }

// Serial.print(" target=");
// Serial.println(m_targetTemp);
}

uint8_t HVAC::getState()
{
  if( m_bRunning == false) return 0;

  // Check if NG furnace is running, which controls the fan automatically
  uint8_t state = (ee.Mode == Mode_Auto) ? m_AutoMode : ee.Mode; // convert auto to just cool / heat

  if(state == Mode_Heat && ( ee.heatMode == Heat_NG || (ee.heatMode == Heat_Auto && m_AutoHeat == Heat_NG) ) )  // convert any NG mode to 3
    state = 3; // so logs will only be 1, 2 or 3.

  return state;
}

bool HVAC::getFanRunning()
{
  return (m_bRunning || m_furnaceFan || m_bFanRunning);
}

uint8_t HVAC::getMode()
{
  return ee.Mode;
}

void HVAC::setHeatMode(uint8_t mode)
{
  m_setHeat = mode % 3;
}

uint8_t HVAC::getHeatMode()
{
  return ee.heatMode;
}

int8_t HVAC::getAutoMode()
{
  return m_AutoMode;
}

bool HVAC::getHumidifierRunning()
{
  return m_bHumidRunning;
}

int8_t HVAC::getSetMode()
{
  return m_setMode;
}

// User:Set a new control mode
void HVAC::setMode(int8_t mode)
{
  m_setMode = mode & 3;
  if(!m_bRunning)
  {
    if(m_setMode == 0 && m_FanMode != FM_On)
      fanSwitch(0); // fan may be on
    if(m_idleTimer < ee.idleMin - 60)
      m_idleTimer = ee.idleMin - 60;        // shorten the idle time
    if(m_idleTimer >= ee.idleMin)
      m_idleTimer = ee.idleMin - 10;        // but at least 10 seconds so mode can be chosen
  }
}

void HVAC::enable()
{
  m_bEnabled = true;
  m_bRecheck = true;
}

int8_t HVAC::getFan()
{
  return m_FanMode;
}

// User:Set fan mode
void HVAC::setFan(int8_t m)
{
  if(m == m_FanMode)     // requested fan operating mode change
    return;

  m_FanMode = m;
  if(!m_bRunning)
    fanSwitch(m == FM_On ? true:false); // manual fan on/off if not running
}

int16_t HVAC::getSetTemp(int8_t mode, int8_t hl)
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

// User:Set new control temp
void HVAC::setTemp(int8_t mode, int16_t Temp, int8_t hl)
{
  if(mode == Mode_Auto)
  {
    mode = m_AutoMode;
  }

  int8_t save;

  switch(mode)
  {
    case Mode_Cool:
      if(Temp < 650 || Temp > 900)    // ensure sane values
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
      ee.heatTemp[1] = min(ee.coolTemp[0] - 20, ee.heatTemp[1]); // Keep 2.0 degree differential for Auto mode
      ee.heatTemp[0] = ee.heatTemp[1] - save;                      // shift heat low by original diff

      if(ee.Mode == Mode_Cool)
        calcTargetTemp(ee.Mode);

      break;
    case Mode_Heat:
      if(Temp < 630 || Temp > 860)    // ensure sane values
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
      ee.coolTemp[0] = max(ee.heatTemp[1] - 20, ee.coolTemp[0]);
      ee.coolTemp[1] = ee.coolTemp[0] + save;

      if(ee.Mode == Mode_Heat)
        calcTargetTemp(ee.Mode);

      break;
  }
}

bool HVAC::showLocalTemp()
{
  return m_bLocalTempDisplay; // should be displaying local temp
}

bool HVAC::isRemote()
{
  return false;
}

void HVAC::enableRemote()
{
  if(m_bRemoteStream) // if using external sensor, stop
  {
    m_bRemoteDisconnect = true;
    m_bRemoteStream = false;
    m_notif = Note_None;
    m_bLocalTempDisplay = true;
  }
}

// Update when DHT22/SHT21 changes
void HVAC::updateIndoorTemp(int16_t Temp, int16_t rh)
{
  m_localTemp = Temp + ee.adj; // Using remote vars for local here
  m_localRh = rh;

  if( m_bRemoteStream == false )
  {
    m_inTemp = Temp + ee.adj;
    m_rh = rh;
  }
  // Auto1 == auto humidifier when running, Auto2 = even when not running (turns fan on)
  if(ee.humidMode >= HM_Auto1)
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
        if(ee.humidMode == HM_Auto1 && m_bRunning == false); // do nothing
        else
        {
          humidSwitch(true);
          fanSwitch(true); // will use fan if not running
        }
      }
      else if(m_bRunning == false && m_rh > ee.rhLevel[1] + 5 &&  ee.humidMode == HM_Auto2 && ee.Mode == Mode_Cool)
      {  // humidity over desired level, use compressor to reduce (will run at least for cycleMin)
          if(m_idleTimer > ee.idleMin)
            m_bStart = true;
      }
    }
  }
}

// Update outdoor temp
void HVAC::updateOutdoorTemp(int16_t outTemp)
{
  m_outTemp = outTemp;
}

// Update min/max for next 180 hrs + 60 past
void HVAC::updatePeaks()
{
  int8_t tmin = m_fcData[0].t;
  int8_t tmax = m_fcData[0].t;

  if(tmin == -1) // initial value
    tmin = m_fcData[1].t;

  int fcCnt;
  for(fcCnt = 1; fcCnt < FC_CNT; fcCnt++) // get length (255 = unused)
    if(m_fcData[fcCnt].h == 255)
      break;

  // Get min/max of current forecast
  for(int i = 1; i < fcCnt; i++)
  {
    int8_t t = m_fcData[i].t;
    if(tmin > t) tmin = t;
    if(tmax < t) tmax = t;
  }

  if(tmin == tmax) tmax++;   // div by 0 check

  m_outMin = tmin;
  m_outMax = tmax;
}

void HVAC::resetFilter()
{
  ee.filterMinutes = 0;
  if(m_notif == Note_Filter)
    m_notif = Note_None;
}

// returns filter over 200 hours
bool HVAC::checkFilter(void)
{
  return (ee.filterMinutes >= 60*200);
}

void HVAC::resetTotal()
{
  m_runTotal = 0;
}

// Current control settings
String HVAC::settingsJson()
{
  String s = "{";
  s += "\"m\":";   s += ee.Mode;
  s += ",\"am\":";  s += m_AutoMode;
  s += ",\"hm\":";  s += ee.heatMode;
  s += ",\"fm\":";  s += m_FanMode;
  s += ",\"ot\":";  s += m_ovrTemp;
  s += ",\"ht\":";  s += ee.eHeatThresh;
  s += ",\"c0\":";  s += ee.coolTemp[0];
  s += ",\"c1\":";  s += ee.coolTemp[1];
  s += ",\"h0\":";  s += ee.heatTemp[0];
  s += ",\"h1\":";  s += ee.heatTemp[1];
  s += ",\"im\":";  s += ee.idleMin;
  s += ",\"cn\":";  s += ee.cycleMin;
  s += ",\"cx\":";  s += ee.cycleMax;
  s += ",\"ct\":";  s += ee.cycleThresh;
  s += ",\"fd\":";  s += ee.fanPostDelay[digitalRead(P_REV)];
  s += ",\"ov\":";  s += ee.overrideTime;
  s += ",\"rhm\":";  s += ee.humidMode;
  s += ",\"rh0\":";  s += ee.rhLevel[0];
  s += ",\"rh1\":";  s += ee.rhLevel[1];
  s += ",\"fp\":";   s += ee.fanPreTime[ee.Mode == Mode_Heat];
  s += ",\"fct\":";  s += ee.fanCycleTime;
  s += ",\"ar\":";  s += m_RemoteFlags;
  s += ",\"at\":";  s += ee.awayTime;
  s += ",\"ad\":";  s += ee.awayDelta[ee.Mode == Mode_Heat];
  s += "}";
  return s;
}

// Constant changing values
String HVAC::getPushData()
{
  String s = "{";
  s += "\"r\":" ;  s += m_bRunning;
  s += ",\"fr\":";  s += getFanRunning(),
  s += ",\"s\":" ;  s += getState();
  s += ",\"it\":";  s += m_inTemp;
  s += ",\"rh\":";  s += m_rh;
  s += ",\"lt\":";  s += m_localTemp; // always local
  s += ",\"lh\":";  s += m_localRh;
  s += ",\"tt\":";  s += m_targetTemp;
  s += ",\"fm\":";  s += ee.filterMinutes;
  s += ",\"ot\":";  s += m_outTemp;
  s += ",\"ol\":";  s += m_outMin;
  s += ",\"oh\":";  s += m_outMax;
  s += ",\"ct\":";  s += m_cycleTimer;
  s += ",\"ft\":";  s += m_fanOnTimer;
  s += ",\"rt\":";  s += m_runTotal;
  s += ",\"h\":";  s += m_bHumidRunning;
  s += ",\"aw\":";  s += m_bAway;
  if(m_bRemoteDisconnect)
  {
    s += ",\"rmt\":0";
    m_bRemoteDisconnect = false;
    m_bLocalTempDisplay = true;
  }
  s += "}";
  return s;
}

static const char *cSCmds[] =
{
  "fanmode",
  "mode",
  "heatmode",
  "resettotal",
  "resetfilter",
  "fanpostdelay",
  "cyclemin",
  "cyclemax",
  "idlemin",
  "cyclethresh",
  "cooltempl",
  "cooltemph",
  "heattempl",
  "heattemph",
  "eheatthresh",
  "override",
  "overridetime",
  "humidmode",
  "humidl",
  "humidh",
  "adj",
  "fanpretime",
  "fancycletime",
  "rmtflgs",
  "awaytime",
  "awaydelta",
  "away",
  NULL
};

int HVAC::CmdIdx(String s, const char **pCmds )
{
  int iCmd;

  for(iCmd = 0; pCmds[iCmd]; iCmd++)
  {
    if( s.equalsIgnoreCase( String(pCmds[iCmd]) ) )
      break;
  }
  return iCmd;
}

// POST set params as "fanmode=1"
void HVAC::setVar(String sCmd, int val)
{
  if(ee.bLock) return;

  switch( CmdIdx( sCmd, cSCmds ) )
  {
    case 0:     // fanmode
      if(val == 3) // "freshen"
      {
        if(m_bRunning || m_furnaceFan || m_bFanRunning) // don't run if system or fan is running
          break;
        m_fanPostTimer = ee.fanCycleTime; // use the post fan timer to shut off
        fanSwitch(true);
      }
      else setFan( val );
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
      ee.cycleThresh = constrain(val, 5, 50); // Limit 0.5 to 5.0 degrees
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
      ee.eHeatThresh = constrain(val, 5, 50); // Limit 5 to 50 degrees F
      break;
    case 15:    // override
      if(val == 0)    // cancel
      {
        m_ovrTemp = 0;
        m_overrideTimer = 0;
      }
      else
      {
        m_ovrTemp = constrain(val, -90, 90); // Limit to -9.0 to +9.0 degrees F
        m_overrideTimer = ee.overrideTime;
      }
      m_bRecheck = true;
      break;
    case 16:    // overridetime
      ee.overrideTime = constrain(val, 60*1, 60*60*6); // Limit 1 min to 6 hours
      break;
    case 17: // humidmode
      ee.humidMode = constrain(val, HM_Off, HM_Auto2);
      break;
    case 18: // humidl
      ee.rhLevel[0] = constrain(val, 300, 900); // no idea really
      break;
    case 19: // humidh
      ee.rhLevel[1] = constrain(val, 300, 900);
      break;
    case 20: // adj
      ee.adj = constrain(val, -30, 30); // calibrate can only be +/-3.0
      break;
    case 21:     // fanPretime
      ee.fanPreTime[ee.Mode == Mode_Heat] = constrain(val, 0, 60*5); // Limit 0 to 5 minutes
      break;
    case 22: // fancycletime
      ee.fanCycleTime = val;
      break;
    case 23: // rmtflgs  0xC=(RF_RL|RF_RH) = use remote, 0x3=(RF_ML|RF_MH)= use main, 0xF = use both averaged
      if(val & (RF_RL|RF_ML) == 0) val |= RF_RL;
      if(val & (RF_RH|RF_MH) == 0) val |= RF_RH;
      m_RemoteFlags = val;
      break;
    case 24: // awaytime
      ee.awayTime = val; // no limit
      break;
    case 25: // awaydelta
      if(ee.Mode == Mode_Heat)
        ee.awayDelta[1] = constrain(val, -150, 0); // Limit to -15 degrees (heat away) target is constrained in calcTargetTemp
      else
        ee.awayDelta[0] = constrain(val, 0, 150); // Limit +15 degrees (cool away)
      break;
    case 26: // away (uses the override feature)
      if(val) // away
      {
        m_overrideTimer = ee.awayTime * 60; // convert minutes to seconds
        m_ovrTemp = ee.awayDelta[ee.Mode == Mode_Heat];
        m_bAway = true;
      }
      else // back
      {
        m_ovrTemp = 0;
        m_overrideTimer = 0;
        m_bAway = false;
      }
      break;
  }
}

void HVAC::updateVar(int iName, int iValue)// host values
{
}

void HVAC::setSettings(int iName, int iValue)// remote settings
{
}
