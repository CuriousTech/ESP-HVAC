#ifndef EEMEM_H
#define EEMEM_H

#include <Arduino.h>

struct eflags
{
  uint8_t PriEn:2;
  uint8_t bPIR:1;
  uint8_t bCall:1;
  uint8_t bCF:1;
  uint8_t bUseTime:1;
  uint8_t bEnableOLED:1;
  uint8_t res:1;
};


struct eeSet // EEPROM backed data
{
  uint16_t  size;          // if size changes, use defauls
  uint16_t  sum;           // if sum is diiferent from memory struct, write
  char      szSSID[32];
  char      szSSIDPassword[64];
  int8_t    tz;            // Timezone offset
  eflags    e;
  char      szName[32];
  uint32_t  sensorID;
  int8_t    tempCal;
  uint16_t  sendRate;
  uint16_t  logRate;
  char      szControlPassword[32];
  uint8_t   hostIP[4];
  uint16_t  hostPort;
  uint8_t   hvacIP[4];
  uint32_t  time_off;
  uint32_t  sleep;
  uint32_t  priSecs;
  uint8_t   pirPin;
  uint16_t  wAlertLevel[16]; // L/H
  uint8_t   res[32];
};

extern eeSet ee;

class eeMem
{
public:
  eeMem(){};
  void init(void);
  void update(void);
private:
  uint16_t Fletcher16( uint8_t* data, int count);
};

extern eeMem eemem;

#endif // EEMEM_H
