#include <ESP8266WiFi.h>
//  HVAC Control
//
#ifndef HVAC_H
#define HVAC_H
//----------------
#define P_FAN   16 // GPIO for SSRs
#define P_COOL  14
#define P_REV   12
#define P_HEAT  15
#define P_HUMID  0
//-----------------
#include <arduino.h>

struct Forecast
{
  uint8_t h;   // hours ahead up to 255
  int8_t t;   // integer temperature value
};

enum Mode
{
  Mode_Off,
  Mode_Cool,
  Mode_Heat,
  Mode_Auto
};

enum FanMode
{
  FM_Auto,
  FM_On,
  FM_Cycle,
};

enum Notif
{
  Note_None,
  Note_CycleLimit,
  Note_Filter,
  Note_Network,
  Note_Forecast,
  Note_RemoteOff,
  Note_RemoteOn,
  Note_Init,
};

enum HeatMode
{
  Heat_HP,
  Heat_NG,
  Heat_Auto
};

enum State
{
  State_Off,
  State_Cool,
  State_HP,
  State_NG
};

enum HumidifierMode
{
  HM_Off,
  HM_Fan,
  HM_Run,
  HM_Auto1,
  HM_Auto2,
};

#define RF_ML (1 << 0)
#define RF_MH (1 << 1)
#define RF_RL (1 << 2)
#define RF_RH (1 << 3)

class HVAC
{
public:
  HVAC(void);
  void    init(void);             // after EEPROM read
  void    disable(void);          // Shut it off
  void    service(void);          // call once per second
  uint8_t getState(void);         // return current run state simplified (0=off, 1=cool, 2=hp, 4=NG)
  bool    getFanRunning(void);    // return fan running
  uint8_t getMode(void);          // actual mode
  uint8_t getHeatMode(void);      // heat mode
  int8_t  getAutoMode(void);      // get current auto heat/cool mode
  int8_t  getSetMode(void);       // get last requested mode
  void    setMode(int8_t mode);   // request new mode; see enum Mode
  void    setHeatMode(uint8_t mode); // heat mode
  int8_t  getFan(void);           // fan mode
  bool    getHumidifierRunning(void);
  void    setFan(int8_t m);        // auto/on/s mode
  void    filterInc(void);
  bool    stateChange(void);      // change since last call = true
  int16_t getSetTemp(int8_t mode, int8_t hl); // get temp set for a mode (cool/heat, hi/lo)
  void    setTemp(int8_t mode, int16_t Temp, int8_t hl); // set temp for a mode
  void    enableRemote(void);
  bool    showLocalTemp(void);
  bool    isRemote(void);          // just indicate remote unit or not
  void    updateIndoorTemp(int16_t Temp, int16_t rh);
  void    updateOutdoorTemp(int16_t outTemp);
  void    updatePeaks(void);
  void    resetFilter(void);    // reset the filter hour count
  bool    checkFilter(void);
  void    resetTotal(void);
  bool    tempChange(void);
  void    setVar(String sCmd, int val); // remote settings
  void    updateVar(int iName, int iValue); // host values
  void    setSettings(int iName, int iValue);// remote settings
  void    enable(void);
  String  settingsJson(void); // get all settings in json format
  String  getPushData(void);  // get states/temps/data in json
#define FC_CNT 41
  Forecast  m_fcData[FC_CNT];
  int16_t   m_outTemp;       // adjusted current temp *10
  int16_t   m_inTemp;        // current indoor temperature *10
  int16_t   m_rh;
  int16_t   m_localTemp;     // this device's temperature *10
  int16_t   m_localRh;
  uint16_t  m_targetTemp;    // end temp for cycle
  uint8_t   m_notif;
  bool      m_bRemoteStream; // remote is streaming temp/rh
  bool      m_bRemoteDisconnect;
  bool      m_bLocalTempDisplay;
  uint8_t   m_RemoteFlags;
  int8_t    m_outMin, m_outMax;
  uint16_t  m_fanPreElap;

private:
  void  fanSwitch(bool bOn);
  void  humidSwitch(bool bOn);
  void  tempCheck(void);
  bool  preCalcCycle(int8_t mode);
  void  calcTargetTemp(int8_t mode);
  int   CmdIdx(String s, const char **pCmds);
  void  sendCmd(const char *szName, int value);

  int8_t  m_FanMode;        // Auto=0, On=1, s=2
  bool    m_bFanRunning;    // when fan is running
  bool    m_bHumidRunning;
  int8_t  m_AutoMode;       // cool, heat
  int8_t  m_setMode;        // preemted mode request
  int8_t  m_setHeat;        // preemt heat mode request
  int8_t  m_AutoHeat;       // auto heat mode choice
  bool    m_bRunning;       // is operating
  bool    m_bStart;         // signal to start
  bool    m_bStop;          // signal to stop
  bool    m_bRecheck;       // recalculate target now
  bool    m_bEnabled;       // enables system
  bool    m_bAway;
  uint16_t m_runTotal;      // time HVAC has been running total since reset
  uint16_t m_fanOnTimer;    // time fan is running
  uint16_t m_cycleTimer;    // time HVAC has been running
  uint16_t m_fanPostTimer;  // timer for delay
  uint16_t m_fanPreTimer;   // timer for fan pre-run
  uint16_t m_idleTimer;     // time not running
  int      m_overrideTimer; // countdown for override in seconds
  int8_t   m_ovrTemp;       // override delta of target
  uint16_t m_remoteTimeout; // timeout for remote sensor
  uint16_t m_remoteTimer;   // in seconds
  int8_t   m_furnaceFan;    // fake fan timer
};

#endif
