#include "eeMem.h"
#include <EEPROM.h>

eeSet ee = { sizeof(eeSet), 0xAAAA,
  "",  // saved SSID (place your SSID and password here)
  "", // router password
  {850, 860},   // 87.0, 90.0 default cool temps
  {740, 750},   // default heat temps
  {28, 8},     // cycleThresh (cool 0.5, heat 0.8)
  0,            // Mode
  33,           // heatThresh (under 33F is gas)
  60*4,         // 5 mins minimum for a cycle
  60*30,        // 30 minutes maximun for a cycle
  60*8,         // idleMin 8 minutes minimum between cycles
  0,            // filterMinutes
  {60*2, 60*2}, // fanPostDelay {cool, heat}
  {60*1, 60*1}, // fanPreTime {cool, heat}
  60*10,        // 10 mins default for override
  0,            // heatMode
  -5,           // timeZone
  0,            // temp reading offset adjust
  0,            // humidMode
  {450, 550},   // rhLevel 45.0%, 55%
  {40, -40},    // awayDelta cool, heat 4.0
  60*8,         // awayTime (minutes)
  60*30,        // fanCycleTime 30 mins
  {192,168,31,100}, // hostIp 192.168.0.105
  80,           // host port
  "41042",      // zipCode
  "password",  // password for controlling thermostat
  false,        // bLock
  false,        // bNotLocal
  140,          // price per KWH in cents * 10000 (0.140)
  1210,         // nat gas cost per 1000 cubic feet in 10th of cents * 1000 ($1.21)
  23,           // forecast range for in mapping to out mix/max (in hours * 3)
  46,           // forecast range for display (5 of 7 day max)
//  { 10.55, 7.49, 6.07, 4.46,6.87,17.66,53.21,40.00,19.02,20.92,4.65,5.19},       // fCostE month costs
//  {82.13,53.05,37.82,23.45, 8.26, 0.40,0.0,0.0,0.0,7.54,39.38,42.55},       // fCostG
//  {0},          // fCostDay day costs
//  {{1.86,0.0},{1.44,0},{0.11,0},{1.35,0},{1.00,0},{0.59,0},{0.41,0},{0.13,0},{0.56,0},{0.40,0},{0.79,0},{1.73,0},{1.84,0},{2.06,0.00},{0.54,0.00},{0.00,0},{0.68,0},{1.38,0},{0.76,0},{0.33,0},
//  {1.12,0},{2.08,0.0},{1.11,0.0},{0.56,0.0},{0.19,0.0},{0.19,0.0},{0.0,0.0},{0.99,0.0},{0.0,0.0},{0.0,0.0},{0.0,0.0},{0.0,0.0}},
  {0},
  {0},
  920,          // cubic feet per minute * 1000 of furnace (0.92)
  2600,         // compressorWatts
  250,          // fanWatts
  100,          // furnaceWatts
  150,          // humidWatts
  120,          // furnacePost (furnace internal fan timer)
  {0,0,0,0},    // remoteIP
  0,            // remote port
  {0},          // remote path
  300,          // set to 30 deg differential cooling limit
  {-180,0},     // forecast offset in minutes (cool, heat)
};

eeMem::eeMem()
{
  EEPROM.begin(sizeof(eeSet));

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
  if(check() == false)
    return; // Nothing has changed?

  uint16_t addr = 0;
  uint8_t *pData = (uint8_t *)&ee;
  for(int i = 0; i < sizeof(eeSet); i++, addr++)
  {
    EEPROM.write(addr, pData[i] );
  }
  EEPROM.commit();
}

bool eeMem::check()
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)&ee, sizeof(eeSet));

  return (old_sum == ee.sum) ? false:true;
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
