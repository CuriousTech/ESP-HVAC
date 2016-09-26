// EEPROM wrapper with defaults

#include "eeMem.h"
#include <EEPROM.h>

eeSet ee = { sizeof(eeSet), 0xAAAA,
  "",  // saved SSID (place your SSID and password here)
  "", // router password
  {790, 820},   // 79.0, 82.0 default cool temps
  {700, 740},   // default heat temps
  17,           // cycleThresh 1.7
  0,            // Mode
  30,           // heatThresh
  60,           // 60 seconds minimum for a cycle
  60*15,        // 15 minutes maximun for a cycle
  60*5,         // idleMin 5 minutes minimum between cycles
  0,            // filterMinutes
  {60, 120},    // fanPostDelay
  {0, 0},       // fanPreTime
  60*10,        // 10 mins default for override
  0,            // heatMode
  -5,           // timeZone
  0,            // temp reading offset adjust
  0,            // humidMode
  {450, 550},   // rhLevel 45.0%, 55%
  {40, -40},    // awayDelta cool, heat
  9*60,         // awayTime
  30*60,        // fanCycleTime 30 mins
  192 | (168<<8) | (105<<24), // hostIp 192.168.0.105
  85,           // host port
  "41042",      // zipCode
  "password",   // password for controlling thermostat
  false,        // bLock
  false,
  {0}
};

eeMem::eeMem()
{
  EEPROM.begin(512);

  uint8_t data[sizeof(eeSet)];
  uint16_t *pwTemp = (uint16_t *)data;

  int addr = 0;
  for(int i = 0; i < sizeof(eeSet); i++, addr++)
  {
    data[i] = EEPROM.read( addr );
  }

  if(pwTemp[0] != sizeof(eeSet)) return; // revert to defaults if struct size changes
  uint16_t sum = pwTemp[1];
  pwTemp[1] = 0;
  pwTemp[1] = Fletcher16(data, sizeof(eeSet) );
  if(pwTemp[1] != sum) return; // revert to defaults if sum fails
  memcpy(&ee, data, sizeof(eeSet) );
}

void eeMem::update() // write the settings if changed
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)&ee, sizeof(eeSet));

  if(old_sum == ee.sum)
    return; // Nothing has changed?

  uint16_t addr = 0;
  uint8_t *pData = (uint8_t *)&ee;
  for(int i = 0; i < sizeof(eeSet); i++, addr++)
  {
    EEPROM.write(addr, pData[i] );
  }
  EEPROM.commit();
}

uint16_t eeMem::Fletcher16( uint8_t* data, int count)
{
   uint16_t sum1 = 0;
   uint16_t sum2 = 0;

   for( int index = 0; index < count; ++index )
   {
      sum1 = (sum1 + data[index]) % 255;
      sum2 = (sum2 + sum1) % 255;
   }

   return (sum2 << 8) | sum1;
}
