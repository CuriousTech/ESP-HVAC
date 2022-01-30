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

#ifndef AM2303_H
#define AM2303_H

#include <Arduino.h>

#define AM2320_ADDR 0x5C // address of AM2320

class AM2320 {
    public:
        AM2320();
        void begin();
        void begin(int sda, int scl);
		bool measure(float& temp, float& rh);
		void getbuf(uint8_t *p);
		int code;
		uint16_t crc;
		uint8_t _sda;
		uint8_t _scl;
    private:
        uint8_t _buf[8];
        bool _read_registers(int startAddress, int numByte);
		uint16_t crc16(byte *byte, int numByte);
};

#endif
