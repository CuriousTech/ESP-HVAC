#include "eeMem.h"
#include <EEPROM.h>

eeSet ee = { sizeof(eeSet), 0xAAAA,
  "",  // saved SSID (place your SSID and password here)
  "", // router password
  {870, 900},   // 87.0, 90.0 default cool temps
  {730, 750},   // default heat temps
  {30, 17},     // cycleThresh (cool 3.0, heat 1.7)
  0,            // Mode
  30,           // heatThresh (under 30F is gas)
  60*2,         // 2 mins minimum for a cycle
  60*25,        // 25 minutes maximun for a cycle
  60*5,         // idleMin 5 minutes minimum between cycles
  0,            // filterMinutes
  {60, 120},    // fanPostDelay {cool, HP}
  {60, 60},     // fanPreTime {cool, HP}
  60*10,        // 10 mins default for override
  0,            // heatMode
  -5,           // timeZone
  0,            // temp reading offset adjust
  0,            // humidMode
  {450, 550},   // rhLevel 45.0%, 55%
  {40, -40},    // awayDelta cool, heat
  9*60,         // awayTime (minutes)
  30*60,        // fanCycleTime 30 mins
  192 | (168<<8) | (105<<24), // hostIp 192.168.0.105
  85,           // host port
  "41042",      // zipCode
  "password",  // password for controlling thermostat
  false,        // bLock
  false,        // bNotLocal
  1454,         // price per KWH in cents / 10000 (0.145)
   700,         // nat gas cost per cubic foot in cents / 100 (0.70)
  46,           // forecast range for in mapping to out mix/max (5, but 3 can be better)
  46,           // forecast range for display (5 of 7 day max)
  {0.0f},       // fCostE month costs
  {0.0f},       // fCostG
  {0},          // fCostDay day costs
  {0},          // ulRes
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
