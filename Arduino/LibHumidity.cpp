/*
  LibHumidity - A Humidity Library for Arduino.

  Supported Sensor modules:
    SHT21-Breakout Module - https://moderndevice.com/products/sht21-humidity-sensor

  Created by Christopher Ladden at Modern Device on December 2009.
  modified by Paul Badger March 2010

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <inttypes.h>
#include <Wire.h>
#include "LibHumidity.h"
#include "Arduino.h"
#include "HVAC.h"

extern HVAC hvac;
/******************************************************************************
 * Constructors
 ******************************************************************************/

/**********************************************************
 * Initialize the sensor based on the specified type.
 **********************************************************/
LibHumidity::LibHumidity(uint8_t sda, uint8_t sdc) {
  m_sda = sda;
  m_sdc = sdc;
}

void LibHumidity::init() {
  Wire.begin(m_sda, m_sdc);
  Wire.setClock(400000);
  m_mil = millis();
}


/**********************************************************
 *  The SHT21 humidity sensor datasheet says:
 *  Parameter Resolution typ max Units
 *    14 bit      66        85      ms
 *    13 bit      33        43      ms
 *    12 Bit      17        22      ms
 *    11 bit       8        11      ms
 *    10 bit       4         6      ms
 *
 *      Measurement time
 *      (max values for -40°C
 *        125°C.)
 *      8 bit 1 3 ms
 *
 **********************************************************/

void LibHumidity::service()
{
  static uint8_t state = 0;

  if(millis() - m_mil < 100)
    return;

  switch(state)
  {
    case 0:
      Wire.beginTransmission(eSHT21Address);   //begin
      Wire.write(eTempNoHoldCmd);              //send the pointer location
      Wire.endTransmission();                  //end
      break;
    case 1:
      Wire.requestFrom(eSHT21Address, 3);
      break;
    case 2:
      if(Wire.available() < 3) return; // not ready
      m_temp = ( Wire.read() << 8 ) | ( Wire.read() & 0xFFFC );
      break;
    case 3:
      hvac.m_inTemp = (int)(calculateTemperatureF(m_temp) * 10);
      break;
    case 4:
      Wire.beginTransmission(eSHT21Address);   //begin
      Wire.write(eRHumidityNoHoldCmd);         //send the pointer location
      Wire.endTransmission();                  //end
      break;
    case 5:
      Wire.requestFrom(eSHT21Address, 3);
      break;
    case 6:
      if(Wire.available() < 3) return; // not ready
      m_rh = ( Wire.read() << 8 ) | ( Wire.read() & 0xFFFC );
      break;
    case 7:
      hvac.m_rh = (int)(calculateHumidity(m_rh, m_temp) * 10);
      break;
  }

  if(++state > 100) state = 0; // about every 10 seconds start over
  m_mil = millis();
}

/******************************************************************************
 * Private Functions
 ******************************************************************************/

float LibHumidity::calculateTemperatureC(uint16_t analogTempValue) {

  return (((175.72/65536.0) * (float)analogTempValue) - 46.85); //T= -46.85 + 175.72 * ST/2^16
}

float LibHumidity::calculateTemperatureF(uint16_t analogTempValue) {

  return (((175.72/65536.0) * (float)analogTempValue) - 46.85) * 9/5 + 32; //T= -46.85 + 175.72 * ST/2^16
}

float LibHumidity::calculateHumidity(uint16_t analogHumValue, uint16_t analogTempValue)
{
  float srh = analogHumValue;
  float humidityRH;                       // variable for result

  //-- calculate relative humidity [%RH] --
  humidityRH = -6.0 + 125.0/65536.0 * srh;       // RH= -6 + 125 * SRH/2^16
  return humidityRH;
}
