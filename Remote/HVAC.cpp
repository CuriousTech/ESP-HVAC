/*
  HVAC.cpp - Arduino library for HVAC control.
  Copyright 2014 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.

  This library is distributed in the hope that it will be useful,  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
*/

#include <math.h>
#include "HVAC.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include <TimeLib.h>
#include "WebHandler.h"
#include "eeMem.h"

extern const char *controlPassword;
extern uint8_t serverPort;

HVAC::HVAC()
{
}

void HVAC::init()
{
  m_setMode = ee.Mode;
  m_idleTimer = ee.idleMin - 60; // about 1 minute
  m_setHeat = ee.heatMode;
}

// Failsafe: shut everything off
void HVAC::disable()
{
}

// Service: called once per second
void HVAC::service()
{
  tempCheck();

  static uint16_t old[4];
  if(m_remoteTimer) // let user change values for some time before sending
  {
    if(--m_remoteTimer == 0)
    {
      if(old[0] != ee.coolTemp[0])  sendCmd("cooltempl", old[0] = ee.coolTemp[0]); 
      if(old[1] != ee.coolTemp[1])  sendCmd("cooltemph", old[1] = ee.coolTemp[1]);
      if(old[2] != ee.heatTemp[0])  sendCmd("heattempl", old[2] = ee.heatTemp[0]);
      if(old[3] != ee.heatTemp[1])  sendCmd("heattemph", old[3] = ee.heatTemp[1]);

      if(ee.heatMode != m_setHeat)  sendCmd("heatmode", ee.heatMode = m_setHeat);
      if(ee.Mode != m_setMode)      sendCmd("mode", ee.Mode = m_setMode);
    }
  }
}

// send a command as JSON: cmd {key:password, command:value}
void HVAC::sendCmd(const char *szName, int value)
{
  String s = "{\"key\":\"";
  s += ee.password;
  s += "\",\"";
  s += szName;
  s += "\":";
  s += value;
  s += "}";

  WsSend((char*)s.c_str(), "cmd");
}

void HVAC::enableRemote()
{
  m_bRemoteStream = !m_bRemoteStream;
  sendCmd("rmt", m_bRemoteStream);
}

bool HVAC::stateChange()
{
  static bool bFan = false;
  static uint8_t lastMode = 0;
  static uint8_t nState = 0;

  if(getMode() != lastMode || getState() != nState || bFan != getFanRunning())   // erase prev highlight
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

  if(nTemp == m_localTemp)
    return false;

  nTemp = m_localTemp;
  return true;
}

// Control switching of system by temp
void HVAC::tempCheck()
{
  if(m_inTemp == 0)    // hasn't been set yet
    return;
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

bool HVAC::getHumidifierRunning()
{
  return m_bHumidRunning;
}

uint8_t HVAC::getMode()
{
  return ee.Mode;
}

void HVAC::setHeatMode(int mode)
{
  m_setHeat = mode % 3;
  m_remoteTimer = 2;
}

uint8_t HVAC::getHeatMode()
{
  return m_setHeat; // for faster visual update
}

int8_t HVAC::getAutoMode()
{
  return m_AutoMode;
}

int8_t HVAC::getSetMode()
{
  return m_setMode;
}

// User:Set a new control mode
void HVAC::setMode(int mode)
{
  m_setMode = mode & 3;
  m_remoteTimer = 2;
}

void HVAC::enable()
{
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

  sendCmd("fanmode", m);
  m_FanMode = m;
}

// Accumulate fan running times
void HVAC::filterInc()
{
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

// User:Set new control temp
void HVAC::setTemp(int mode, int16_t Temp, int hl)
{
  if(mode == Mode_Auto)
  {
    mode = m_AutoMode;
  }

  int8_t save;
  m_remoteTimer = 2; // 2 second hold before transmit

  switch(mode)
  {
    case Mode_Cool:
      if(Temp < 650 || Temp > 900)    // ensure sane values
        break;
      ee.coolTemp[hl] = Temp;
      if(hl)
      {
        ee.coolTemp[0] = min((int)ee.coolTemp[1], (int)ee.coolTemp[0]);     // don't allow h/l to invert
      }
      else
      {
        ee.coolTemp[1] = max((int)ee.coolTemp[0], (int)ee.coolTemp[1]);
      }
      save = ee.heatTemp[1] - ee.heatTemp[0];
      ee.heatTemp[1] = min((int)ee.coolTemp[0] - 20, (int)ee.heatTemp[1]); // Keep 2.0 degree differential for Auto mode
      ee.heatTemp[0] = ee.heatTemp[1] - save;                      // shift heat low by original diff

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
      ee.coolTemp[0] = max(ee.heatTemp[1] - 20, (int)ee.coolTemp[0]);
      ee.coolTemp[1] = ee.coolTemp[0] + save;
      break;
  }
}

bool HVAC::showLocalTemp()
{
  return m_bLocalTempDisplay;
}

bool HVAC::isRemote()
{
  return true;
}

// Update when DHT22/SHT21 changes
void HVAC::updateIndoorTemp(int16_t Temp, int16_t rh)
{
  m_localTemp = Temp + ee.adj;
  m_localRh = rh;

  if( m_bRemoteStream )
  {
    m_inTemp = Temp + ee.adj;
    m_rh = rh;
  }
}

// Update outdoor temp
void HVAC::updateOutdoorTemp(int16_t outTemp)
{
  m_outTemp = outTemp;
}

void HVAC::resetFilter()
{
  ee.filterMinutes = 0;
  sendCmd("resetfilter", 0);
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
  sendCmd("resettotal", 0);
}

void HVAC::setVar(String sCmd, int val) // remote settings
{
}

void HVAC::updateVar(int iName, int iValue)// host values
{
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
      break;
    case 4: // rh
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
      m_inTemp = iValue;
      break;
    case 15: // lh
      m_rh = iValue;
      break;
    case 16: // rmt
      m_bRemoteStream = false; // command to kill remote temp send
      break;
  }
}

void HVAC::setSettings(int iName, int iValue)// remote settings
{
  switch(iName)
  {
    case 0:
      m_setMode = ee.Mode = iValue;
      break;
    case 1:
      m_AutoMode = iValue;
      break;
    case 2:
      m_setHeat = ee.heatMode = iValue;
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
      ee.cycleThresh[ee.Mode == Mode_Heat] = iValue;
      break;
    case 14:
      break;
    case 15:
      ee.overrideTime = iValue;
      break;
    case 16:
      ee.humidMode = iValue;
      break;
    case 17:
      ee.rhLevel[0] = iValue;
      break;
    case 18:
      ee.rhLevel[1] = iValue;
      break;
  }
}

// Current control settings
String HVAC::settingsJson()
{
  String s = "{";
  s += "\"rmt\":"; s += m_bRemoteStream;
  s += "}";
  return s;
}

// Remote sensor values
String HVAC::getPushData()
{
  String s = "{";
  s += "\"t\":";  s += now() - ((ee.tz+m_DST) * 3600);
  s += ",\"tempi\":"; s += m_localTemp;
  s += ",\"rhi\":"; s += m_localRh;
  s += ",\"rmt\":"; s += m_bRemoteStream;
  s += "}";
  return s;
}
