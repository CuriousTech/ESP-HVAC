#ifndef EEMEM_H
#define EEMEM_H

#include <Arduino.h>

struct Sched
{
  uint16_t setTemp;
  uint16_t timeSch;
  uint8_t thresh;
  uint8_t wday;
  char    name[16]; // names for small display
}; // 22

struct flags_t
{
  uint16_t Mode:3;
  uint16_t heatMode:2;
  uint16_t humidMode:3;
  uint16_t nSchedMode:2;
  uint16_t bCelcius:1;
  uint16_t bLock:1;
  uint16_t bNotLocalFcst:1;
  uint16_t res:3;
};

struct eeSet // EEPROM backed data
{
  uint16_t size;          // if size changes, use defaults
  uint16_t sum;           // if sum is different from memory struct, write
  char     szSSID[24];
  char     szSSIDPassword[24];
  uint16_t coolTemp[2]; // cool to temp *10 low/high
  uint16_t heatTemp[2]; // heat to temp *10 low/high
  flags_t  b;           // see flags_t
  int8_t   cycleThresh[2]; // temp range for cycle *10
  uint8_t  eHeatThresh; // degree threshold to switch to gas
  uint16_t cycleMin;    // min time to run
  uint16_t cycleMax;    // max time to run
  uint16_t idleMin;     // min time to not run
  uint16_t filterMinutes; // resettable minutes run timer (200 hours is standard change interval)
  uint16_t fanPostDelay[2]; // delay to run auto fan after [hp/cool] stops
  uint16_t fanPreTime[2]; // fan pre-run before [cool/heat]
  uint16_t overrideTime; // time used for an override
  int8_t   tz;          // current timezone and DST
  int8_t   adj;         // temp offset adjust
  uint16_t rhLevel[2];  // rh low/high
  int8_t   awayDelta[2]; // temp offset in away mode[cool][heat]
  uint16_t awayTime;    // time limit for away offset (in minutes)
  uint8_t  hostIp[4];
  uint16_t hostPort;
  char     zipCode[8];  // Your zipcode
  char     password[24];
  uint8_t  fcRange; // number in forecasts (3 hours)
  uint8_t  fcDisplay; // number in forecasts (3 hours)
  uint16_t iSecsDay[32][3];
  uint32_t iSecsMon[12][3]; // compressor,gas,fan
  uint16_t ppkwh;
  uint16_t ccf;
  uint16_t cfm;         // cubic feet per minute
  uint16_t compressorWatts;
  uint8_t  fanWatts;
  uint8_t  furnaceWatts; // inducer motor mostly
  uint8_t  humidWatts;
  uint16_t furnacePost;
  uint16_t diffLimit;  // in/out thermal differential limit
  int16_t  fcOffset[2]; // forecast offset adjust
  uint16_t fanIdleMax; // in minutes
  uint8_t  fanAutoRun;
  uint8_t  reserved[17];
}; // 512 bytes

extern eeSet ee;

class eeMem
{
public:
  eeMem();
  bool init(void);
  bool check(void);
  bool update(void);
private:
  uint16_t Fletcher16( uint8_t* data, int count);
};

extern eeMem eemem;

#endif // EEMEM_H
