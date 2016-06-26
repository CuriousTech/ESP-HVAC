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
#include <JsonClient.h>
#include <Event.h>

extern eventHandler event;
extern const char *controlPassword;
extern uint8_t serverPort;

HVAC::HVAC()
{
  m_EE.size = sizeof(EEConfig);
//---------- EEPROM default values ----------
  m_EE.cycleMin = 60;         // 60 seconds minimum for a cycle
  m_EE.cycleMax = 60*15;      // 15 minutes maximun for a cycle
  m_EE.idleMin  = 60*5;       // 5 minutes minimum between cycles
  m_EE.cycleThresh =  17;      // 1.7 degree cycle range
  m_EE.coolTemp[1] = 820;     // 82.0 default temps
  m_EE.coolTemp[0] = 790;     // 79.0
  m_EE.heatTemp[1] = 740;     // 74.0
  m_EE.heatTemp[0] = 700;     // 70.0
  m_EE.eHeatThresh =  30;     // Setting this low (30 deg) for now
  m_EE.fanPostDelay[0] = 60;  // 1 minute after compressor stops (HP)
  m_EE.fanPostDelay[1] = 120; // 2 minutes after compressor stops (cool)
  m_EE.overrideTime = 60*10;  // 10 mins default for override
  m_remoteTimeout   = 0;   // remote transmit delay
  m_EE.humidMode = 0;
  m_EE.rhLevel[0] = 450;    // 45.0%
  m_EE.rhLevel[1] = 550;
  m_EE.tz = -5;
  m_EE.filterMinutes = 0;
  m_EE.adj = 0;
  m_EE.fanPreTime[0] = 0; // disable by default
  m_EE.fanPreTime[1] = 0;
  m_EE.fanCycleTime = 30*60; // 30 mins
  m_EE.awayDelta[0] = 40; // +4.0 cool
  m_EE.awayDelta[1] = -40; // heat
  m_EE.awayTime = 9*60; // 9 hours
  m_EE.hostIp = 192 | (168<<8) | (105<<24); // 192.168.0.105
  m_EE.hostPort = 85;
  strcpy(m_EE.zipCode, "41042");
  memset(m_EE.reserved, 0, sizeof(m_EE.reserved));
//----------------------------
  memset(m_fcData, -1, sizeof(m_fcData)); // invalidate forecast
  m_outTemp = 0;
  m_inTemp = 0;
  m_rh = 0;
  m_bFanRunning = false;
  m_bHumidRunning = false;
  m_outMax[0] = -50;      // set as invalid
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
  m_furnaceFan = 0;       // fake fan timer
  m_notif = Note_None;    // Empty
  m_idleTimer = 60*3;     // start with a high idle, in case of power outage
  m_bRemoteStream = false;
  m_bRemoteDisconnect = false;
  m_bLocalTempDisplay = true; // default to local/remote temp
  m_bAvgRemote = false;
  m_localTemp = 0;
  m_bAway = false;
  m_fanPreElap = 60*10;
}

void HVAC::init()
{
  m_setMode = m_EE.Mode;
  m_idleTimer = m_EE.idleMin - 60; // about 1 minute
  m_setHeat = m_EE.heatMode;
}

// Failsafe: shut everything off
void HVAC::disable()
{
}

// Service: called once per second
void HVAC::service()
{
  static uint8_t initRmt = 15; // delayed start
  static unsigned long hostIp = m_EE.hostIp + m_EE.hostPort;

  if(initRmt)
  {
    if(--initRmt == 0)
      connectRemote();
  }
  else if( hostIp != m_EE.hostIp + m_EE.hostPort) // host IP was reconfigured
  {
    hostIp == m_EE.hostIp;
    initRmt = 1; // cause a restart
  }
  
  tempCheck();

  static uint16_t old[4];
  if(m_remoteTimer) // let user change values for some time before sending
  {
    if(--m_remoteTimer == 0)
    {
      if(old[0] != m_EE.coolTemp[0])  sendCmd("cooltempl", old[0] = m_EE.coolTemp[0]); 
      if(old[1] != m_EE.coolTemp[1])  sendCmd("cooltemph", old[1] = m_EE.coolTemp[1]);
      if(old[2] != m_EE.heatTemp[0])  sendCmd("heattempl", old[2] = m_EE.heatTemp[0]);
      if(old[3] != m_EE.heatTemp[1])  sendCmd("heattemph", old[3] = m_EE.heatTemp[1]);

      if(m_EE.heatMode != m_setHeat)  sendCmd("heatmode", m_EE.heatMode = m_setHeat);
      if(m_EE.Mode != m_setMode)      sendCmd("mode", m_EE.Mode = m_setMode);
    }
  }
}

void HVAC::sendCmd(const char *szName, int value)
{
  String s = "{\"";
  s += szName;
  s += "\":";
  s += value;
  s += "}";

  event.push("cmd", s);
}

void sc_callback(uint16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
}

void HVAC::connectRemote() // request an event listener from main
{
  JsonClient cl(sc_callback);
  String path = "/remote?key=";
  path += controlPassword;
  path += "&path=%2Fevents%3Fi=30%26p=1&port=";  // the path needs to be URL encoded
  path += serverPort;
  m_bLocalTempDisplay = true;
  IPAddress ip(m_EE.hostIp);
  if(!cl.begin(ip.toString().c_str(), path.c_str(), m_EE.hostPort, false))
    event.print("Can't send event request");
}

void HVAC::enableRemote()
{
  m_bRemoteStream = !m_bRemoteStream;
  event.push(); // send rmt state + update temp/rh
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
  uint8_t state = (m_EE.Mode == Mode_Auto) ? m_AutoMode : m_EE.Mode; // convert auto to just cool / heat

  if(state == Mode_Heat && ( m_EE.heatMode == Heat_NG || (m_EE.heatMode == Heat_Auto && m_AutoHeat == Heat_NG) ) )  // convert any NG mode to 3
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
  return m_EE.Mode;
}

void HVAC::setHeatMode(uint8_t mode)
{
  m_setHeat = mode % 3;
  m_remoteTimer = 2;
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
  m_remoteTimer = 2;
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

//template <class T> const T& max (const T& a, const T& b) {
//  return (a<b)?b:a;     // or: return comp(a,b)?b:a; for version (2)
//}
//template <class T> const T& min (const T& a, const T& b) {
//  return (a>b)?b:a;     // or: return comp(a,b)?b:a; for version (2)
//}
// User:Set new control temp
void HVAC::setTemp(int8_t mode, int16_t Temp, int8_t hl)
{
  if(mode == Mode_Auto)
  {
    mode = m_AutoMode;
  }

  int8_t save;
  m_remoteTimer = 2; // 3 second hold before transmit

  switch(mode)
  {
    case Mode_Cool:
      if(Temp < 650 || Temp > 900)    // ensure sane values
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
  m_localTemp = Temp + m_EE.adj;
  m_localRh = rh;

  if( m_bRemoteStream )
  {
    m_inTemp = Temp + m_EE.adj;
    m_rh = rh;
  }
}

// Update outdoor temp
void HVAC::updateOutdoorTemp(int16_t outTemp)
{
  m_outTemp = outTemp;
}

// Update min/max for next 48 hrs + 60 past
void HVAC::updatePeaks()
{
  int8_t tmin = m_fcData[0].t;
  int8_t tmax = m_fcData[0].t;

  // Get min/max of current forecast
  for(int i = 1; i < 18; i++)
  {
    int8_t t = m_fcData[i].t;
    if(tmin > t) tmin = t;
    if(tmax < t) tmax = t;
  }

  if(tmin == tmax) tmax++;   // div by 0 check

  // add it to the history
  if(m_outMax[0] != -50)      // preserve peaks longer
  {
    for(int i = 0; i < PEAKS_CNT-1; i++) // FIFO
    {
      m_outMax[i+1] = m_outMax[i];
      m_outMin[i+1] = m_outMin[i];
    }
  }
  else                        // initial fill value
  {
    for(int i = 0; i < PEAKS_CNT; i++)
    {
      m_outMax[i] = tmax;
      m_outMin[i] = tmax;
    }
  }
  m_outMin[0] = tmin;
  m_outMax[0] = tmax;
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
      m_EE.filterMinutes = iValue;
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
  s += "\"tempi\":"; s += m_localTemp;
  s += ",\"rhi\":"; s += m_localRh;
  s += ",\"rmt\":"; s += m_bRemoteStream;
  s += "}";
  return s;
}
