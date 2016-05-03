/*
  LibHumidity - A Humidity Library for Arduino.

  Supported Sensor modules:
    SHT21-Breakout Module - https://moderndevice.com/products/sht21-humidity-sensor

  Created by Christopher Ladden at Modern Device on December 2009.

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


#ifndef LIB_HUMIDITY_H
#define LIB_HUMIDITY_H

#include <inttypes.h>

typedef enum {
    eSHT21Address = 0x40,
} HUM_SENSOR_T;

typedef enum {
    eTempHoldCmd       = 0xE3,
    eRHumidityHoldCmd  = 0xE5,
    eTempNoHoldCmd     = 0xF3,
    eRHumidityNoHoldCmd = 0xF5,
} HUM_MEASUREMENT_CMD_T;

class LibHumidity
{
  public:
    LibHumidity(uint8_t sda, uint8_t sdc);
    void init(void);
    float GetHumidity(void);
    float GetTemperatureC(void);
    float GetTemperatureF(void);
    void startRead(uint8_t reg);
    uint16_t readMem(void);
    void service(void);
  private:
    float calculateHumidity(uint16_t analogHumValue, uint16_t analogTempValue);
    float calculateTemperatureC(uint16_t analogTempValue);
    float calculateTemperatureF(uint16_t analogTempValue);
    uint8_t m_sda;
    uint8_t m_sdc;
    uint16_t m_temp;
    uint16_t m_rh;
    unsigned long m_mil;
};

#endif
