// Class for AM2320 temp and humidity

#include "BasicSensor.h"
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html

#define ESP_LED  2  // low turns on ESP blue LED
#define CF_BTN   13  // C/F button

extern void WsSend(String s);

void BasicInterface::init(bool bCF)
{
  pinMode(ESP_LED, OUTPUT);
  digitalWrite(ESP_LED, LOW);
  m_am.begin(5, 4);
  m_bCF = bCF;
}

int BasicInterface::service(int8_t tcal, int8_t rhcal)
{
  static uint8_t lastSec;

  if(second() == lastSec)
    return 0;

  lastSec = second();

  if((lastSec % 5) == 0)
  {
    float ftemp, frh;
    if(m_am.measure(ftemp, frh))
    {
      m_status = 0;
      
      if(m_bCF)
        ftemp = ( 1.8 * ftemp + 32.0) * 10;
      else
        ftemp *= 10;
      ftemp += tcal;
      m_tempMedian[0].add( ftemp);
      m_tempMedian[0].getAverage(2, ftemp);
      m_tempMedian[1].add(frh * 10);
      m_tempMedian[1].getAverage(2, frh);
      if(m_values[DE_TEMP] != (uint16_t)ftemp || m_values[DE_RH] != (uint16_t)frh)
        m_bUpdated = true;
      m_values[DE_TEMP] = ftemp;
      m_values[DE_RH] = frh + rhcal;
    }
    else
    {
      m_status = 1;
    }
  }

  static bool bBtn = true;
  if( digitalRead(CF_BTN) != bBtn )
  {
    bBtn = digitalRead(CF_BTN);
    if( bBtn ) // release
      setCF( !m_bCF );
  }
  return m_status;
}

void BasicInterface::setCF(bool f)
{
  m_bCF = f;
}

int BasicInterface::status()
{
  return m_status;
}

void BasicInterface::setLED(uint8_t no, bool bOn)
{
  m_bLED[0] = bOn;
  digitalWrite(ESP_LED, !bOn);   // No external LED
}
