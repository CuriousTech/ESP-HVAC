#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif

// Uncomment to compile for remote
//#define REMOTE

//  HVAC Control
//
#ifndef HVAC_H
#define HVAC_H
//----------------
#ifdef ESP32
#define P_FAN   33 // G GPIO for SSRs
#define P_COOL  25 // Y
#define P_REV   26 // O
#define P_HEAT  14 // W
#define P_HUMID 27 // H
#define USE_AUDIO
#define SPEAKER 17
#define FAN_ON  LOW
#define FAN_OFF HIGH
#define COOL_ON LOW
#define COOL_OFF HIGH
#define HEAT_ON LOW
#define HEAT_OFF HIGH
#define REV_ON LOW
#define REV_OFF HIGH
#define HUMID_ON LOW
#define HUMID_OFF HIGH
#else
#define P_FAN   16 // G GPIO for SSRs
#define P_COOL  14 // Y
#define P_REV   12 // O
#define P_HEAT  15 // W
#define P_HUMID  0 // H
#define FAN_ON  HIGH
#define FAN_OFF LOW
#define COOL_ON HIGH
#define COOL_OFF LOW
#define HEAT_ON HIGH
#define HEAT_OFF LOW
#define REV_ON HIGH
#define REV_OFF LOW
#define HUMID_ON LOW
#define HUMID_OFF HIGH
#endif
//-----------------
#include <arduino.h>

enum Mode
{
  Mode_Off,
  Mode_Cool,
  Mode_Heat,
  Mode_Auto,
  Mode_Fan,
  Mode_Humid
};

enum FanMode
{
  FM_Auto,
  FM_On
};

enum Notif
{
  Note_None,
  Note_Connecting,
  Note_Connected,
  Note_RemoteOff,
  Note_RemoteOn,
  Note_CycleLimit,
  Note_Network, // Sound errors below this point
  Note_Forecast,
  Note_Filter,
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

enum ScheduleMode
{
  SM_Forecast,
  SM_Sine,
  SM_Flat,
  SM_Reserved
};


#define SNS_PRI   (1 << 0) // Single sensor overrides all others including internal
#define SNS_EN    (1 << 1) // Enabled = averaged between all enabled
#define SNS_C     (1 << 2) // Data from remote sensor is C or F
#define SNS_F     (1 << 3) // ""
#define SNS_TOPRI (1 << 4) // 1=timer is for priority, 0=for average
#define SNS_LO    (1 << 5) // lo/hi unused as of now
#define SNS_HI    (1 << 6)
#define SNS_WARN  (1 << 7) // internal flag for data timeout
#define SNS_NEG   (1 << 8)  // From remote or page, set this bit to disable a flag above

struct Sensor
{
  uint32_t tm;
  uint32_t timer; // seconds, priority timer
  uint32_t IP; //
  uint8_t flags;
  int16_t temp;
  uint16_t rh;
  uint32_t ID; // hex text?
  uint8_t pad; // NULL for ID
};

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
  void    setMode(int mode);   // request new mode; see enum Mode
  void    setHeatMode(int mode); // heat mode
  int8_t  getFan(void);           // fan mode
  bool    getHumidifierRunning(void);
  void    setFan(int8_t m);        // auto/on/s mode
  void    filterInc(void);
  bool    stateChange(void);      // change since last call = true
  int16_t getSetTemp(int mode, int hl); // get temp set for a mode (cool/heat, hi/lo)
  void    setTemp(int mode, int16_t Temp, int hl); // set temp for a mode
  void    enableRemote(void);
  void    updateIndoorTemp(int16_t Temp, int16_t rh);
  void    updateOutdoorTemp(int16_t outTemp);
  void    resetFilter(void);    // reset the filter hour count
  bool    checkFilter(void);
  void    resetTotal(void);
  bool    tempChange(void);
  void    setVar(String sCmd, int val, char *psValue, IPAddress ip); // remote settings
  void    updateVar(int iName, int iValue); // host values
  void    setSettings(int iName, int iValue);// remote settings
  String  settingsJson(void); // get all settings in json format
  String  settingsJsonMod(void);
  String  getPushData(void);  // get states/temps/data in json
  void    dayTotals(int d);
  void    monthTotal(int m, int dys);

  int16_t  m_outTemp;       // adjusted current temp *10
  int16_t  m_inTemp;        // current indoor temperature *10
  int16_t  m_rh;
  int16_t  m_localTemp;     // this device's temperature *10
  int16_t  m_localRh;
  uint16_t m_targetTemp;    // end temp for cycle
  uint16_t m_filterMinutes;
  uint8_t  m_notif;
  bool     m_bRemoteStream; // remote is streaming temp/rh
  bool     m_bRemoteDisconnect;
  int8_t   m_outMin, m_outMax;
  uint16_t m_iSecs[3];
  bool     m_bLink;         // link adjust mode
  uint8_t  m_DST;
  int8_t   m_modeShadow = Mode_Cool;  // shadow last valid mode
#define SNS_CNT 8
  Sensor   m_Sensor[SNS_CNT]; // remote and sensors

private:
  void  fanSwitch(bool bOn);
  void  humidSwitch(bool bOn);
  void  tempCheck(void);
  bool  preCalcCycle(int16_t tempL, int16_t tempH);
  void  calcTargetTemp(int mode);
  void  costAdd(int secs, int mode, int hm);
  int   CmdIdx(String s);
  void  sendCmd(const char *szName, int value);
  int   getSensorID(uint32_t val);
  void  swapSensors(int n1, int n2);
  void  shiftSensors(void);
  void  deactivateSensor(int idx);

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
  uint16_t m_fanPreElap = 60*10;
  uint16_t m_runTotal;      // time HVAC has been running total since reset
  uint32_t m_fanOnTimer;    // time fan is running
  uint16_t m_cycleTimer;    // time HVAC has been running
  uint16_t m_fanPostTimer;  // timer for delay
  uint16_t m_fanPreTimer;   // timer for fan pre-run
  uint16_t m_idleTimer = 3*60; // time not running
  uint32_t m_fanIdleTimer; // time fan not running
  uint16_t m_fanAutoRunTimer; // auto fan time
  int      m_overrideTimer; // countdown for override in seconds
  int8_t   m_ovrTemp;       // override delta of target
  uint16_t m_remoteTimeout; // timeout for remote sensor
  uint16_t m_remoteTimer;   // in seconds
  uint16_t m_humidTimer;    // timer for humidifier cost
  int8_t   m_furnaceFan;    // fake fan timer
};

#endif
