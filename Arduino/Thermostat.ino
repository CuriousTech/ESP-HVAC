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

// Build with Arduino IDE 1.6.11 and esp8266 SDK 2.3.0

#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include "HVAC.h"
#include <XMLReader.h>
#include "Encoder.h"
#include "WebHandler.h"
#include "display.h"
#include <Wire.h>
#include "eeMem.h"
#include "RunningMedian.h"

//uncomment to swap Serial's pins to 15(TX) and 13(RX) that don't interfere with booting
//#define SER_SWAP https://github.com/esp8266/Arduino/blob/master/doc/reference.md

// Uncomment only one of these
#include <SHT21.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/SHT21
//#include <DHT.h>  // http://www.github.com/markruys/arduino-DHT
//#include <DallasTemperature.h> //DallasTemperature from library mamanger

//----- Pin Configuration - See HVAC.h for the rest -
#define ESP_LED   2  //Blue LED on ESP07 (on low) also SCL
#define SCL       2
#define SDA      13

#define ENC_A    5  // Encoder is on GPIO4 and 5
#define ENC_B    4
//------------------------

extern AsyncEventSource events; // event source (Server-Sent events)

Display display;
eeMem eemem;

HVAC hvac;

#ifdef SHT21_H
SHT21 sht(SDA, SCL, 5);
RunningMedian<int16_t,20> tempMedian; //median over 20 samples at 5s intervals
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
#endif

XML_tag_t Xtags[] =
{
  {"creation-date", NULL, NULL, 1},
  {"time-layout", "time-coordinate", "local", FC_CNT},
  {"temperature", "type", "hourly", FC_CNT},
  {NULL}
};

int xmlState;

void xml_callback(int8_t item, int8_t idx, char *p)
{
  int8_t newtz;
  int8_t h;
  int8_t d;
  static int8_t hO;
  static int8_t lastd;
  static tmElements_t t;

  switch(item)
  {
    case -1: // done
      xmlState = idx;
      break;
    case 0:
      if(atoi(p) == 0) // todo: fix
        break;
      t.Year = CalendarYrToTm(atoi(p));
      t.Month = atoi(p+5);
      t.Day = atoi(p+8);
      t.Hour = atoi(p+11);
      t.Minute = atoi(p+14);
      t.Second = atoi(p+17);
      break;
    case 1:            // valid time
      if(idx == 0)     // first item isn't really data
      {
        hO = 0;        // reset hour offset
        hvac.m_fcData[0].t = hvac.m_fcData[1].t; // keep a copy of first 3hour data
        hvac.m_fcData[0].h = hvac.m_fcData[1].h;
        break;
      }

      d = atoi(p + 8);  // 2014-mm-ddThh:00:00-tz:00
      h = atoi(p + 11);

      if(idx != 1 && d != lastd)
        hO += 24; // change to hours offset
      lastd = d;
      hvac.m_fcData[idx].h = h + hO;

      newtz = atoi(p + 20); // tz minutes = atoi(p+23) but uncommon
      if(p[19] == '-') // its negative
        newtz = -newtz;

      if(idx == 1)
      {
        time_t epoc = makeTime(t);
        ee.tz = newtz;
        epoc += ee.tz * 3600;
        setTime(epoc);
      }
      break;
    case 2:                  // temperature
      if(idx == 0) break;    // 1st value is not temp
      hvac.m_fcData[idx].t = atoi(p);
      break;
  }
}

XMLReader xml(xml_callback, Xtags);

void GetForecast()
{
  String path = "/xml/sample_products/browser_interface/ndfdXMLclient.php?zipCodeList=";
  path += ee.zipCode;
  path += "&Unit=e&temp=temp&Submit=Submit";

  if(!xml.begin("graphical.weather.gov", path))
    events.send("Forecast failed", "alert");
}

//-----

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

//  t += (r > 0) ? 1 : -1; // inc/dec by 1 only
  t += r; // inc/dec by any amount

  hvac.setTemp(m, t, hilo);

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
#ifdef SHT21_H
  sht.init();
#endif
#ifdef dht_h
  dht.setup(SDA, DHT::DHT22);
#endif
#ifdef DallasTemperature_h
  ds18.setResolution(ds18addr, ds18Resolution);
  ds18.setWaitForConversion(false); //this enables asyncronous calls
  ds18.requestTemperatures(); //fire off the first request
  ds18lastreq = millis();
  ds18delay = 750 / (1 << (12 - ds18Resolution)); //delay based on resolution
#endif
}

void loop()
{
  static uint8_t hour_save, min_save = 255, sec_save;
  static int8_t lastSec;
  static int8_t lastHour;

  while( EncoderCheck() );
  display.checkNextion();  // check for touch, etc.
  handleServer(); // handles mDNS, web
#ifdef SHT21_H
  if(sht.service())
  {
    tempMedian.add(sht.getTemperatureF() * 10);
    int16_t temp;
    if (tempMedian.getMedian(temp) == tempMedian.OK) {
      hvac.updateIndoorTemp( temp, sht.getRh() * 10 );
    }
  }
#endif
#ifdef DallasTemperature_h
  if(ds18lastreq > 0 && millis() - ds18lastreq >= ds18delay) { //new temp is ready
    tempMedian.add(ds18.getTempF(ds18addr));
    ds18lastreq = 0; //prevents this block from firing repeatedly
    float temp;
    if (tempMedian.getMedian(temp) == tempMedian.OK) {
      hvac.updateIndoorTemp( temp * 10, 500); //fake 50%
    }
  }

  if(millis() - ds18reqlastreq >= ds18reqdelay) {
    ds18.requestTemperatures(); 
    ds18lastreq = millis();
    ds18reqlastreq = ds18lastreq;
  }
#endif
  if(xmlState)
  {
      switch(xmlState)
      {
        case XML_COMPLETED:
        case XML_DONE:
          hvac.enable();
          events.send("Forecast success", "print");
          hvac.updatePeaks();
          display.screen(true);
          display.drawForecast(true);
          break;
        case XML_TIMEOUT:
          events.send("Forcast timeout", "print");
          hvac.disable();
          hvac.m_notif = Note_Forecast;
          break;
      }
      xmlState = 0;
  }
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
      int16_t temp = (dht.toFahrenheit(dht.getTemperature()) * 10);

      if(dht.getStatus() == DHT::ERROR_NONE)
      {
        tempMedian.add(temp);
        if (tempMedian.getMedian(temp) == tempMedian.OK) {
          hvac.updateIndoorTemp( temp, dht.getHumidity() * 10);
        }
      }
      read_delay = 5; // update every 5 seconds
    }
#endif

    if(min_save != minute()) // only do stuff once per minute
    {
      min_save = minute();
      if (hour_save != hour()) // update our IP and time daily (at 2AM for DST)
      {
        eemem.update(); // update EEPROM if needed while we're at it (give user time to make many adjustments)
      }

      if(--display.m_updateFcst <= 0 )  // usually every hour / 3 hours
      {
        display.m_updateFcst = 5;    // retry in 5 mins if anything fails
        GetForecast();
      }
    }

  }
  delay(8); // rotary encoder and lines() need 8ms minimum
}
