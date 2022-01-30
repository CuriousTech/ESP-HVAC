/**
 Modified by CuriousTech for stability Orgininal: https://github.com/hibikiledo/AM2320

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Copyright 2016 Ratthanan Nalintasnai
**/

#include "AM2320.h"

#include <Wire.h>

AM2320::AM2320() {
    // do nothing
}

void  AM2320::begin() {
    Wire.begin();
}

void AM2320::begin(int sda, int scl) {
    Wire.begin(sda, scl);
	_scl = scl;
	_sda = sda;
}

void AM2320::getbuf(uint8_t *p)
{
	memcpy(p, _buf, 8);
}

bool AM2320::measure(float& temp, float& rh)
{
  Wire.setClock(100000);
	code = 0;
    if ( ! _read_registers(0x00, 4))
	{
        return false;
	}

    uint16_t receivedCrc = *(uint16_t*)(_buf+6);   // little endien
	crc = receivedCrc;
    if (receivedCrc != crc16(_buf, 6))
    {
	  code = 2;
	  return false;
	}
    int16_t r = (int16_t)((_buf[2] << 8) | _buf[3]); // big endien
    rh = (float)r / 10;
    int16_t t = (int16_t)((_buf[4] << 8) | _buf[5]);
    temp = (float)t / 10;
	return true;
}

bool AM2320::_read_registers(int startAddress, int numByte) {
//	Wire.setClock(100000);
    Wire.beginTransmission(AM2320_ADDR);
    Wire.endTransmission();
    delay(10);                    // heat time >800us
    Wire.beginTransmission(AM2320_ADDR);
    Wire.write(0x03);           // function code: 0x03 - read register data
    Wire.write(startAddress);   // begin address
    Wire.write(numByte);        // number of bytes to read

    // send and check result if not success
    if (Wire.endTransmission(true) != 0) {
		code = 1;
	    Wire.begin(_sda, _scl);
        return false;           // sensor not ready
    }
    delay(2);                    // as specified in datasheet
    Wire.requestFrom(AM2320_ADDR, numByte + 4); // request bytes from sensor

    for ( int i = 0; i < numByte + 4; i++)    // read
        _buf[i] = Wire.read();

    return true;
}

uint16_t AM2320::crc16(byte *byte, int numByte) {
    uint16_t crc = 0xFFFF;          // 16-bit crc register

    while (numByte > 0) {               // loop until process all bytes
        crc ^= *byte;                   // exclusive-or crc with first byte

        for (int i = 0; i < 8; i++) {       // perform 8 shifts
            uint16_t lsb = crc & 0x01;  // extract LSB from crc
            crc >>= 1;                      // shift be one position to the right

            if (lsb == 0) {                 // LSB is 0
                continue;                   // repete the process
            }
            else {                          // LSB is 1
                crc ^= 0xA001;              // exclusive-or with 1010 0000 0000 0001
            }
        }

        numByte--;          // decrement number of byte left to be processed
        byte++;             // move to next byte
    }

    return crc;
}
