/**The MIT License (MIT)

Copyright (c) 2016 by Greg Cunningham, CuriousTech

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// Build with Arduino IDE 1.8.15 and esp8266 SDK 3.0.1  1M (64K SPIFFS)
#ifdef ESP32
#else
#include <ESP8266mDNS.h>
#endif
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include <UdpTime.h> // https://github.com/CuriousTech/ESP07_WiFiGarageDoor/tree/master/libraries/UdpTime
#include "HVAC.h"
#include "Encoder.h"
#include "WebHandler.h"
#include "display.h"
#include <Wire.h>
#include "eeMem.h"
#include "RunningMedian.h"

// ESP8266 uncomment to swap Serial's pins to 15(TX) and 13(RX) that don't interfere with booting
//#define SER_SWAP https://github.com/esp8266/Arduino/blob/master/doc/reference.md

// Uncomment only one of these
#include <SHT21.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/SHT21
//#include <DHT.h>  // http://www.github.com/markruys/arduino-DHT
//#include <DallasTemperature.h> //DallasTemperature from library mamanger
//#include <AM2320.h>

//----- Pin Configuration - See HVAC.h for the rest -
#ifdef ESP32
#define SDA      21
#define SCL      22
#define ENC_A    16
#define ENC_B    4
#else // ESP8266 pins
#define SDA       2
#define SCL      13
#define ENC_A    5  // Encoder is on GPIO4 and 5
#define ENC_B    4
#endif
//------------------------

//extern AsyncEventSource events; // event source (Server-Sent events)

Display display;
eeMem eemem;

HVAC hvac;

#ifdef SHT21_H
SHT21 sht(SDA, SCL, 4);
RunningMedian<int16_t,25> tempMedian; //median over 25 samples at 4s intervals
#endif
#ifdef dht_h
DHT dht;
RunningMedian<int16_t,20> tempMedian; //median over 20 samples at 5s intervals
#endif
#ifdef DallasTemperature_h
const int ds18Resolution = 12;
DeviceAddress ds18addr = { 0x28, 0xC1, 0x02, 0x64, 0x04, 0x00, 0x00, 0x35 };
unsigned int ds18delay;
unsigned long ds18lastreq = 1; //zero is special
const unsigned int ds18reqdelay = 5000; //request every 5 seconds
unsigned long ds18reqlastreq;
OneWire oneWire(2); //pin 2
DallasTemperature ds18(&oneWire);
RunningMedian<int16_t,25> tempMedian; //median over 25 samples at 2s intervals
#endif
#ifdef AM2303_H
AM2320 am;
RunningMedian<int16_t,25> tempMedian; //median over 25 samples at 4s intervals
#endif

UdpTime utime;

Encoder rot(ENC_B, ENC_A);

bool EncoderCheck()
{
  if(ee.bLock) return false;

  int r = rot.poll();

  if(r == 0)  // no change
    return false;

  display.screen(true); // ensure page is thermostat

  int8_t m = (display.m_adjustMode < 2) ? Mode_Cool : Mode_Heat; // lower 2 are cool
  int8_t hilo = (display.m_adjustMode ^ 1) & 1; // hi or low of set
  int16_t t = hvac.getSetTemp(m, hilo ); // 

  t += r; // inc/dec by any amount

  hvac.setTemp(m, t, hilo);

  if(hvac.m_bLink) // adjust both high and low
  {
    t = hvac.getSetTemp(m, hilo^1 ) + r; // adjust opposite hi/lo the same
    hvac.setTemp(m, t, hilo^1);
  }

  display.updateTemps();
  return true;
}

void setup()
{
  Serial.begin(115200);  // Nextion must be set with bauds=115200
#ifdef SER_SWAP
  Serial.swap(); //swap to gpio 15/13
#endif

  startServer();
  hvac.init();
  display.init();

  ee.bCelcius = false; // Force F for now

#ifdef SHT21_H
  sht.init();
#endif
#ifdef dht_h
  dht.setup(SCL, DHT::DHT22);
#endif
#ifdef DallasTemperature_h
  ds18.setResolution(ds18addr, ds18Resolution);
  ds18.setWaitForConversion(false); //this enables asyncronous calls
  ds18.requestTemperatures(); //fire off the first request
  ds18lastreq = millis();
  ds18delay = 750 / (1 << (12 - ds18Resolution)); //delay based on resolution
#endif
#ifdef AM2303_H
  am.begin(SDA, SCL);
#endif
  utime.start();
}

void loop()
{
  static uint8_t hour_save, min_save = 255, sec_save;
  static int8_t lastSec;
  static int8_t lastHour;
  static int8_t lastDay = -1;

  while( EncoderCheck() );
  display.checkNextion();  // check for touch, etc.
  handleServer(); // handles mDNS, web
  if(utime.check(ee.tz))
  {
    hvac.m_DST = utime.getDST();
    if(lastDay == -1)
      lastDay = day() - 1;
  }
#ifdef SHT21_H
  if(sht.service())
  {
    tempMedian.add((ee.bCelcius ? sht.getTemperatureC():sht.getTemperatureF()) * 10);
    float temp;
    if (tempMedian.getAverage(2, temp) == tempMedian.OK) {
      hvac.updateIndoorTemp( temp, sht.getRh() * 10 );
    }
  }
#endif
#ifdef DallasTemperature_h
  if(ds18lastreq > 0 && millis() - ds18lastreq >= ds18delay) { //new temp is ready
    tempMedian.add((ee.bCelcius ? ds18.getTempC(ds18addr):ds18.getTempF(ds18addr)) );
    ds18lastreq = 0; //prevents this block from firing repeatedly
    float temp;
    if (tempMedian.getAverage(temp) == tempMedian.OK) {
      hvac.updateIndoorTemp( temp * 10, 500); //fake 50%
    }
  }

  if(millis() - ds18reqlastreq >= ds18reqdelay) {
    ds18.requestTemperatures(); 
    ds18lastreq = millis();
    ds18reqlastreq = ds18lastreq;
  }
#endif
  if(sec_save != second()) // only do stuff once per second
  {
    sec_save = second();
    secondsServer(); // once per second stuff
    display.oneSec();
    hvac.service();   // all HVAC code

#ifdef dht_h
    static uint8_t read_delay = 2;
    if(--read_delay == 0)
    {
      float temp;
      if(ee.bCelcius)
        temp = dht.getTemperature() * 10;
      else
        temp = dht.toFahrenheit(dht.getTemperature()) * 10;

      if(dht.getStatus() == DHT::ERROR_NONE)
      {
        tempMedian.add(temp);
        if (tempMedian.getAverage(2, temp) == tempMedian.OK) {
          hvac.updateIndoorTemp( temp, dht.getHumidity() * 10);
        }
      }
      read_delay = 5; // update every 5 seconds
    }
#endif
#ifdef AM2303_H
    static uint8_t read_delay = 2;
    if(--read_delay == 0)
    {
      float temp;
      float rh;
      if(am.measure(temp, rh))
      {
        if(!ee.bCelcius)
          temp = temp * 9 / 5 + 32;
        tempMedian.add(temp * 10);
        if (tempMedian.getAverage(2, temp) == tempMedian.OK) {
          hvac.updateIndoorTemp( temp, rh * 10 );
        }
      }
      read_delay = 5; // update every 5 seconds
    }
#endif

    if(min_save != minute()) // only do stuff once per minute
    {
      min_save = minute();

      if(hour_save != hour()) // update our IP and time daily (at 2AM for DST)
      {
        hour_save = hour();
        if(hour_save == 2)
          utime.start(); // update time daily at DST change
        if(hour_save == 0 && year() > 2020)
        {
          if(lastDay != -1)
          {
            hvac.dayTotals(lastDay);
            hvac.monthTotal(month() - 1, day());
          }
          lastDay = day() - 1;
          ee.iSecsDay[lastDay][0] = 0; // reset
          ee.iSecsDay[lastDay][1] = 0;
          ee.iSecsDay[lastDay][2] = 0;
          if(lastDay == 0) // new month
          {
            int m = (month() + 10) % 12; // last month: Dec = 10, Jan = 11, Feb = 0
            hvac.monthTotal(m, -1);
          }
        }
        if(eemem.check())
        {
          if((hour_save & 1) == 0) // every other hour
          {
            ee.filterMinutes = hvac.m_filterMinutes;
            eemem.update(); // update EEPROM if needed while we're at it (give user time to make many adjustments)
          }
        }
      }
    }

  }
  delay(8); // rotary encoder and lines() need 8ms minimum
}
