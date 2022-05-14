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

#define EESIZE (offsetof(eeMem, end) - offsetof(eeMem, size) )

class eeMem
{
public:
  eeMem(){};
  void init(void);
  void update(void);
private:
  uint16_t Fletcher16( uint8_t* data, int count);
public:
  uint16_t  size = EESIZE;          // if size changes, use defauls
  uint16_t  sum = 0xAAAA;           // if sum is diiferent from memory struct, write
  char      szSSID[32] = "";
  char      szSSIDPassword[64] = "";
  int8_t    tz = -5;               // Timezone offset
  eflags    e = {0,1,1,1,0,0,0}; // PirEn, bPIR, bCall, bCF, bUseTime, bEnableOLED, res
  char      szName[32] = "Sensor1";
  uint32_t  sensorID = '1SNS';
  int8_t    tempCal = 0;
  uint16_t  sendRate = 15;
  uint16_t  logRate = 60;
  char      szControlPassword[32] = "password";
  uint8_t   hostIP[4] = {192,168,31,100};
  uint16_t  hostPort = 80;
  uint8_t   hvacIP[4] = {192,168,31,46};
  uint32_t  time_off = 0;
  uint32_t  sleep = 30;
  uint32_t  priSecs = 60*5;
  uint8_t   pirPin = 12;
  uint16_t  wAlertLevel[16] =  {320, 1000, 0, 900, 0, 1000, 0, 10, 0, 20, 0, 1000, 0, 1000, 0, 1000}; // alert levels L/H
  int8_t    rhCal = 0;
  uint8_t   weight = 1;
  uint8_t   res[30];
  uint8_t   end;
};

extern eeMem ee;

#endif // EEMEM_H
