#ifndef EEMEM_H
#define EEMEM_H

#include <Arduino.h>

struct eeSet // EEPROM backed data
{
  uint16_t size;          // if size changes, use defauls
  uint16_t sum;           // if sum is diiferent from memory struct, write
  char     szSSID[32];
  char     szSSIDPassword[64];
  int8_t   tz;            // Timezone offset
  uint8_t  useTime;
  int8_t   tempCal;
  bool     bEnableOLED;
  uint16_t rate;
  char     szControlPassword[32];
  uint8_t  hostIP[4];
  uint16_t hostPort;
  uint8_t  hvacIP[4];
  uint32_t time_off;
  uint32_t sleep;
  uint32_t priSecs;
  uint8_t  PriEn;
  bool    bPIR;
  char     res[26];
};

extern eeSet ee;

class eeMem
{
public:
  eeMem();
  void update(void);
private:
  uint16_t Fletcher16( uint8_t* data, int count);
};

extern eeMem eemem;

#endif // EEMEM_H
