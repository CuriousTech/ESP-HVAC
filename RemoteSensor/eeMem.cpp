#include "eeMem.h"
#include <EEPROM.h>

void eeMem::init()
{
  EEPROM.begin(EESIZE);

  uint8_t data[EESIZE];
  uint16_t *pwTemp = (uint16_t *)data;

  int addr = 0;
  for(int i = 0; i < EESIZE; i++, addr++)
  {
    data[i] = EEPROM.read( addr );
  }

  if(pwTemp[0] != EESIZE) return; // revert to defaults if struct size changes
  uint16_t sum = pwTemp[1];
  pwTemp[1] = 0;
  pwTemp[1] = Fletcher16(data, EESIZE );
  if(pwTemp[1] != sum) return; // revert to defaults if sum fails
  memcpy(this + offsetof(eeMem, size), data, EESIZE );
}

void eeMem::update() // write the settings if changed
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)this + offsetof(eeMem, size), EESIZE);

  if(old_sum == ee.sum)
    return; // Nothing has changed?

  uint16_t addr = 0;
  uint8_t *pData = (uint8_t *)this + offsetof(eeMem, size);
  for(int i = 0; i < EESIZE; i++, addr++)
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
