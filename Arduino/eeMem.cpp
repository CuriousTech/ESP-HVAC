#include "eeMem.h"
#include <EEPROM.h>

eeSet ee = { sizeof(eeSet), 0xAAAA,
  "",  // saved SSID (place your SSID and password here)
  "", // router password
  {850, 860},   // 87.0, 90.0 default cool temps  F/C issue
  {740, 750},   // default heat temps             F/C issue
  {0,0,0,0,0,0,0,0}, // flags
  {28, 8},      // cycleThresh (cool 0.5, heat 0.8) F/C issue
  33,           // heatThresh (under 33F is gas)  F/C issue
  60*4,         // 5 mins minimum for a cycle
  60*30,        // 30 minutes maximun for a cycle
  60*8,         // idleMin 8 minutes minimum between cycles
  0,            // filterMinutes
  {60*2, 60*2}, // fanPostDelay {cool, heat}
  {60*1, 60*1}, // fanPreTime {cool, heat}
  60*10,        // 10 mins default for override
  -5,           // timeZone
  0,            // temp reading offset adjust
  {450, 750},   // rhLevel 45.0%, 75%
  {40, -40},    // awayDelta cool, heat 4.0      F/C issue
  60*8,         // awayTime (minutes)
  {192,168,31,100}, // hostIp 192.168.31.46
  80,           // host port
  "4291945",    // OWM city ID from http://bulk.openweathermap.org/sample/ city.list.json.gz
  "password",  // password for controlling thermostat
  23,           // forecast range for in mapping to out mix/max (in hours * 3)
  46,           // forecast range for display (5 or 7 day max)
  {{0,0,1794},{0,0,1794},{0,0,1196},{5460,0,9072},{14343,0,21945},{14635,0,21993},{16979,0,26391},{2783,0,5246},{2675,0,4414},{0,0,1794},{3183,0,5402},
   {6896,0,9241},{5280,0,7381},{6663,0,8945},{8941,0,11168},{10682,0,16647},{7228,0,12776},{3022,0,4879},{4659,1156,8608},{6037,0,8319},{4487,0,6289},
   {2327,0,4184},{3012,0,5050},{5853,0,7954},{14164,0,16753},{16901,0,19671},{10425,0,12589},{11859,0,14747},{10630,0,13156},{11409,0,13998},{0,0,1794}},

  {{16565,357781,769917},{16565,357781,769917},{0,170664,419895},{523,57837,146926},{27756,26256,113103},{153956,0,235082},
   {212466,0,304331},{0,0,4784},{0,0,0},{0,0,0},{113429,52189,295135},{0,251368,492489}},

  147,          // price per KWH in cents * 10000 (0.140)
  1190,         // nat gas cost per 1000 cubic feet in 10th of cents * 1000 ($1.21)
  820,          // cubic feet per minute * 1000 of furnace (0.92)
  2600,         // compressorWatts
  250,          // fanWatts
  220,          // furnaceWatts (1.84A inducer)
  150,          // humidWatts
  114,          // furnacePost (furnace internal fan timer)
  300,          // set to 30 deg differential cooling limit    F/C issue
  {-180,0},     // forecast offset in minutes (cool, heat)
  60*4,         // fan idle max
  5,            // fan auto run
  {0,0},        // sine offset
};

eeMem::eeMem()
{
}

bool eeMem::init()
{
  EEPROM.begin(sizeof(eeSet));

  uint8_t data[sizeof(eeSet)];
  uint16_t *pwTemp = (uint16_t *)data;

#ifdef ESP32
  EEPROM.readBytes(0, &data, sizeof(eeSet));
#else
  int addr = 0;
  for(int i = 0; i < sizeof(eeSet); i++, addr++)
    data[i] = EEPROM.read( addr );
#endif
  if(pwTemp[0] != sizeof(eeSet))
    return true; // revert to defaults if struct size changes

  uint16_t sum = pwTemp[1];
  pwTemp[1] = 0;
  pwTemp[1] = Fletcher16(data, sizeof(eeSet) );
  if(pwTemp[1] != sum)
    return true; // revert to defaults if sum fails
  memcpy(&ee, data, sizeof(eeSet) );
  return true;
}

bool eeMem::update() // write the settings if changed
{
  check(); // make sure sum is correct
#ifdef ESP32
  EEPROM.writeBytes(0, &ee, sizeof(eeSet));
#else
  uint16_t addr = 0;
  uint8_t *pData = (uint8_t *)&ee;
  for(int i = 0; i < sizeof(eeSet); i++, addr++)
    EEPROM.write(addr, pData[i] );
#endif
  return EEPROM.commit();
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
