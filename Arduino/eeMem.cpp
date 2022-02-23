#include "eeMem.h"
#include <EEPROM.h>

eeMem::eeMem()
{
}

bool eeMem::init()
{
  EEPROM.begin(EESIZE);

  uint8_t data[EESIZE];
  uint16_t *pwTemp = (uint16_t *)data;

#ifdef ESP32
  EEPROM.readBytes(0, &data, EESIZE);
#else
  int addr = 0;
  for(int i = 0; i < EESIZE; i++, addr++)
    data[i] = EEPROM.read( addr );
#endif
  if(pwTemp[0] != EESIZE)
    return true; // revert to defaults if struct size changes

  uint16_t sum = pwTemp[1];
  pwTemp[1] = 0;
  pwTemp[1] = Fletcher16(data, EESIZE );
  if(pwTemp[1] != sum)
    return true; // revert to defaults if sum fails
  memcpy(this + offsetof(eeMem, size), data, EESIZE );
  return true;
}

bool eeMem::update() // write the settings if changed
{
  check(); // make sure sum is correct
#ifdef ESP32
  EEPROM.writeBytes(0, this + offsetof(eeMem, size), EESIZE);
#else
  uint16_t addr = 0;
  uint8_t *pData = (uint8_t *)this + offsetof(eeMem, size);
  for(int i = 0; i < EESIZE; i++, addr++)
    EEPROM.write(addr, pData[i] );
#endif
  return EEPROM.commit();
}

bool eeMem::check()
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)this + offsetof(eeMem, size), EESIZE);
  return (old_sum == ee.sum) ? false:true;
}

uint16_t eeMem::getSum()
{
  return Fletcher16((uint8_t*)this + offsetof(eeMem, size), EESIZE);
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
