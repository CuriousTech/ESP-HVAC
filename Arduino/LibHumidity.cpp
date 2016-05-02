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

/******************************************************************************
 * Constructors
 ******************************************************************************/

/**********************************************************
 * Initialize the sensor based on the specified type.
 **********************************************************/
LibHumidity::LibHumidity(uint8_t sda, uint8_t sdc) {
  l_sda = sda;
  l_sdc = sdc;
}

void LibHumidity::init() {
  Wire.begin(l_sda, l_sdc);
  Wire.setClock(400000);
}

/******************************************************************************
 * Global Functions
 ******************************************************************************/

/**********************************************************
 * GetHumidity
 *  Gets the current humidity from the sensor.
 *
 * @return float - The relative humidity in %RH
 **********************************************************/
float LibHumidity::GetHumidity(void) {

  return calculateHumidity(readMem(), temp);
}

/**********************************************************
 * GetTemperatureC
 *  Gets the current temperature from the sensor.
 *
 * @return float - The temperature in Deg C
 **********************************************************/
float LibHumidity::GetTemperatureC(void) {
  return calculateTemperatureC(readMem());
}

/**********************************************************
 * GetTemperatureF
 *  Gets the current temperature from the sensor.
 *
 * @return float - The temperature in Deg F
 **********************************************************/
float LibHumidity::GetTemperatureF(void) {
  return calculateTemperatureF(readMem());
}

/**********************************************************
 * SetReadDelay
 *  Set the I2C Read delay from the sensor.
 *
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

void LibHumidity::startRead(uint8_t command) {
  Wire.beginTransmission(eSHT21Address);   //begin
  Wire.write(command);                      //send the pointer location
  Wire.endTransmission();                  //end
}

uint16_t LibHumidity::readMem(void) {
  Wire.requestFrom(eSHT21Address, 3);
  unsigned long m = millis();
  while(millis() - m < 1000 && Wire.available() < 3) {
      ; //wait
  }

  return ( Wire.read() << 8 ) | ( Wire.read() & 0xFFFC );
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

