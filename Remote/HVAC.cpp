/*
  HVAC.cpp - Arduino library for HVAC control (Remote unit).
  Copyright 2014 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.

  This library is distributed in the hope that it will be useful,  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
*/

#include <math.h>
#include "HVAC.h"
#include <TimeLib.h>
#include <JsonClient.h>

extern const char *hostIp;
extern const char *controlPassword;
extern uint8_t hostPort;

HVAC::HVAC()
{
  m_EE.size = sizeof(EEConfig);
//----------------------------
  m_EE.cycleMin = 60;         // 60 seconds minimum for a cycle
  m_EE.cycleMax = 60*15;      // 15 minutes maximun for a cycle
  m_EE.idleMin  = 60*5;       // 5 minutes minimum between cycles
  m_EE.cycleThresh =  17;     // 1.7 degree cycle range
  m_EE.coolTemp[1] = 820;     // 82.0 default temps
  m_EE.coolTemp[0] = 790;     // 79.0
  m_EE.heatTemp[1] = 740;     // 74.0
  m_EE.heatTemp[0] = 700;     // 70.0
  m_EE.eHeatThresh =  30;     // Setting this low (30 deg) for now
  m_EE.fanPostDelay[0] = 60;  // 1 minute after compressor stops (HP)
  m_EE.fanPostDelay[1] = 120; // 2 minutes after compressor stops (cool)
  m_EE.overrideTime = 60*10;  // 10 mins default for override
  m_remoteTimeout   = 60*5;   // 5 minutes default
  m_EE.humidMode = 0;
  m_EE.rhLevel[0] = 450;    // 45.0%
  m_EE.rhLevel[1] = 550;
  m_EE.humidMode = 0;
  m_EE.tz = -5;
  m_EE.filterMinutes = 0;
  m_EE.adj = 0;
  strcpy(m_EE.zipCode, "41042");
//----------------------------
  memset(m_fcData, -1, sizeof(m_fcData)); // invalidate forecast
  m_outTemp = 0;
  m_inTemp = 0;
  m_rh = 0;
  m_bFanRunning = false;
  m_outMax[0] = -50;      // set as invalid
  m_outMax[1] = -50;      // set as invalid
  m_bFanMode = false;     // Auto=false, On=true
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
  m_remoteTimer = 0;      // in seconds
  m_furnaceFan = 0;       // fake fan timer
  m_notif = Note_None;    // Empty
  m_idleTimer = 60*3;     // start with a high idle, in case of power outage
  m_bRemoteConnected = false;
}

void HVAC::init()
{
  m_setMode = m_EE.Mode;
  m_idleTimer = m_EE.idleMin / 2;
  m_setHeat = m_EE.heatMode;
}

// Failsafe: shut everything off
void HVAC::disable()
{
}

// Service: called once per second
void HVAC::service()
{
  tempCheck();
}

void sc_callback(uint16_t iEvent, uint16_t iName, uint16_t iValue, char *psValue)
{
}

void HVAC::sendCmd(char *szName, int value)
{
  char szPath[64];

  JsonClient cl(sc_callback);
  String path = "/s?";
  path += "key=";
  path += controlPassword;
  path += "&";
  path += szName;
  path += "=";
  path += value;
  path.toCharArray(szPath, 64);
  cl.begin(hostIp, szPath, hostPort, false);
}

void HVAC::enableRemote()
{
  char szPath[64];

  JsonClient cl(sc_callback);
  String path = "/remotes?";
  path += "key=";
  path += controlPassword;
  if(m_bRemoteConnected)
  {
    path += "&end=1";
    m_bRemoteConnected = false;
  }
  else
  {
    path += "&path=\"/events?i=30&p=1\"";
    m_bRemoteConnected = true;
  }
  path.toCharArray(szPath, 64);
  cl.begin(hostIp, szPath, hostPort, false);
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
  static uint16_t nTarget = 0;

  nTemp = m_inTemp;
  nTarget = m_targetTemp;
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
  uint8_t state = (m_EE.Mode == Mode_Auto) ? m_AutoMode : m_EE.Mode; // convert auto to just cool / heat

  if(state == Mode_Heat && ( m_EE.heatMode == Heat_NG || (m_EE.heatMode == Heat_Auto && m_AutoHeat == Heat_NG) ) )  // convert any NG mode to 3
    state = 3; // so logs will only be 1, 2 or 3.

  return state;
}

bool HVAC::getFanRunning()
{
  return (m_bRunning || m_furnaceFan || m_bFanRunning);
}

uint8_t HVAC::getMode()
{
  return m_EE.Mode;
}

void HVAC::setHeatMode(uint8_t mode)
{
  m_setHeat = mode % 3;
  sendCmd("heatmode", m_setHeat);
  m_EE.heatMode = m_setHeat;
}

uint8_t HVAC::getHeatMode()
{
  return m_EE.heatMode;
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
void HVAC::setMode(int8_t mode)
{
  m_setMode = mode & 3;
  sendCmd("mode", m_setMode);
  m_EE.Mode = m_setMode;
}

void HVAC::enable()
{
}

bool HVAC::getFan()
{
  return m_bFanMode;
}

// User:Set fan mode
void HVAC::setFan(bool bon)
{
  if(bon == m_bFanMode)     // requested fan operating mode change
    return;

  sendCmd("fanmode", bon);
  m_bFanMode = bon;
}

// Accumulate fan running times
void HVAC::filterInc()
{
}

int16_t HVAC::getSetTemp(int8_t mode, int8_t hl)
{
  switch(mode)
  {
    case Mode_Cool:
      return m_EE.coolTemp[hl];
    case Mode_Heat:
      return m_EE.heatTemp[hl];
    case Mode_Auto:
      return (m_AutoMode == Mode_Cool) ? m_EE.coolTemp[hl] : m_EE.heatTemp[hl];
  }
  return 0;
}

template <class T> const T& max (const T& a, const T& b) {
  return (a<b)?b:a;     // or: return comp(a,b)?b:a; for version (2)
}
template <class T> const T& min (const T& a, const T& b) {
  return (a>b)?b:a;     // or: return comp(a,b)?b:a; for version (2)
}
// User:Set new control temp
void HVAC::setTemp(int8_t mode, int16_t Temp, int8_t hl)
{
  if(mode == Mode_Auto)
  {
    mode = m_AutoMode;
  }

  int8_t save;
  uint16_t old[4];
  old[0] = m_EE.coolTemp[0];
  old[1] = m_EE.coolTemp[1];
  old[2] = m_EE.heatTemp[0];
  old[3] = m_EE.heatTemp[1];

  switch(mode)
  {
    case Mode_Cool:
      if(Temp < 650 || Temp > 880)    // ensure sane values
        break;
      m_EE.coolTemp[hl] = Temp;
      if(hl)
      {
        m_EE.coolTemp[0] = min((int)m_EE.coolTemp[1], (int)m_EE.coolTemp[0]);     // don't allow h/l to invert
      }
      else
      {
        m_EE.coolTemp[1] = max((int)m_EE.coolTemp[0], (int)m_EE.coolTemp[1]);
      }
      save = m_EE.heatTemp[1] - m_EE.heatTemp[0];
      m_EE.heatTemp[1] = min((int)m_EE.coolTemp[0] - 20, (int)m_EE.heatTemp[1]); // Keep 2.0 degree differential for Auto mode
      m_EE.heatTemp[0] = m_EE.heatTemp[1] - save;                      // shift heat low by original diff

      break;
    case Mode_Heat:
      if(Temp < 630 || Temp > 860)    // ensure sane values
        break;
      m_EE.heatTemp[hl] = Temp;
      if(hl)
      {
        m_EE.heatTemp[0] = min(m_EE.heatTemp[1], m_EE.heatTemp[0]);
      }
      else
      {
        m_EE.heatTemp[1] = max(m_EE.heatTemp[0], m_EE.heatTemp[1]);
      }
      save = m_EE.coolTemp[1] - m_EE.coolTemp[0];
      m_EE.coolTemp[0] = max(m_EE.heatTemp[1] - 20, (int)m_EE.coolTemp[0]);
      m_EE.coolTemp[1] = m_EE.coolTemp[0] + save;
      break;
  }

  if(old[0] != m_EE.coolTemp[0])    sendCmd("cooll", m_EE.coolTemp[0]);
  if(old[1] != m_EE.coolTemp[1])    sendCmd("coolh", m_EE.coolTemp[1]);
  if(old[2] != m_EE.heatTemp[0])    sendCmd("heatl", m_EE.heatTemp[0]);
  if(old[3] != m_EE.heatTemp[1])    sendCmd("heath", m_EE.heatTemp[1]);
}

bool HVAC::isRemoteTemp()
{
  return m_bRemoteConnected ? true:false;
}

// Update when DHT22/SHT21 changes
void HVAC::updateIndoorTemp(int16_t Temp, int16_t rh)
{
  if( m_bRemoteConnected == false )
    return;
  m_inTemp = Temp + m_EE.adj;
  m_rh = rh;
}

// Update outdoor temp
void HVAC::updateOutdoorTemp(int16_t outTemp)
{
  m_outTemp = outTemp;
}

// Update min/max for next 24 hrs
void HVAC::updatePeaks(int8_t mn, int8_t mx)
{
  if(m_outMax[0] != -50)      // preserve peaks longer
  {
    m_outMax[0] = m_outMax[1];
    m_outMin[0] = m_outMin[1];
  }
  else                        // initial value
  {
    m_outMin[0] = mn;
    m_outMax[0] = mx;
  }
  m_outMin[1] = mn;
  m_outMax[1] = mx;
}

void HVAC::resetFilter()
{
  m_EE.filterMinutes = 0;
  sendCmd("resetfilter", 0);
  if(m_notif == Note_Filter)
    m_notif = Note_None;
}

// returns filter over 200 hours
bool HVAC::checkFilter(void)
{
  return (m_EE.filterMinutes >= 60*200);
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
    case 0:
      m_bRunning = iValue;
      break;
    case 1:
      m_bFanRunning = iValue;
      break;
    case 2:
//      m_state = iValue;
      break;
    case 3:
      if( m_bRemoteConnected == false )
        m_inTemp = iValue;
      break;
    case 4:
      if( m_bRemoteConnected == false )
        m_rh = iValue;
      break;
    case 5:
      m_targetTemp = iValue;
      break;
    case 6:
      m_EE.filterMinutes = iValue;
      break;
    case 7: // outTemp
      break;
    case 8: // outmin
      break;
    case 9: // outmax
      break;
    case 10:
      m_cycleTimer = iValue;
      break;
    case 11:
      m_fanOnTimer = iValue;
      break;
    case 12:
      m_runTotal = iValue;
      break;
  }
}

void HVAC::setSettings(int iName, int iValue)// remote settings
{
  switch(iName)
  {
    case 0:
      m_setMode = m_EE.Mode = iValue;
      break;
    case 1:
      m_AutoMode = iValue;
      break;
    case 2:
      m_setHeat = m_EE.heatMode = iValue;
      break;
    case 3:
      m_bFanMode = iValue;
      break;
    case 4:
      m_ovrTemp = iValue;
      break;
    case 5:
      m_EE.eHeatThresh = iValue;
      break;
    case 6:
      m_EE.coolTemp[0] = iValue;
      break;
    case 7:
      m_EE.coolTemp[1] = iValue;
      break;
    case 8:
      m_EE.heatTemp[0] = iValue;
      break;
    case 9:
      m_EE.heatTemp[1] = iValue;
      break;
    case 10:
      m_EE.idleMin = iValue;
      break;
    case 11:
      m_EE.cycleMin = iValue;
      break;
    case 12:
      m_EE.cycleMax = iValue;
      break;
    case 13:
      m_EE.cycleThresh = iValue;
      break;
    case 14:
      break;
    case 15:
      m_EE.overrideTime = iValue;
      break;
    case 16:
      m_EE.humidMode = iValue;
      break;
    case 17:
      m_EE.rhLevel[0] = iValue;
      break;
    case 18:
      m_EE.rhLevel[1] = iValue;
      break;
  }
}

// Remote sensor values
String HVAC::getPushData()
{
  String s = "{";
  s += ",\"tempi\":";  s += m_inTemp;
  s += ",\"rhi\":";  s += m_rh;
  s += "}";
  return s;
}
