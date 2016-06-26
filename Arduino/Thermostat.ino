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

// Build with Arduino IDE 1.6.9 and esp8266 SDK 2.2.0

#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESP8266WebServer.h>
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include "HVAC.h"
#include <Event.h>
#include <XMLReader.h>
#include "Encoder.h"
#include "WebHandler.h"
#include "display.h"
#include <Wire.h>

// Uncomment only one of these
#include <SHT21.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/SHT21
//#include <DHT.h>  // http://www.github.com/markruys/arduino-DHT

//----- Pin Configuration - See HVAC.h for the rest -
#define ESP_LED   2  //Blue LED on ESP07 (on low) also SCL
#define SCL       2
#define SDA      13

#define ENC_A    5  // Encoder is on GPIO4 and 5
#define ENC_B    4
//------------------------
Display display;
eventHandler event(dataJson);

HVAC hvac;

#ifdef SHT21_H
SHT21 sht(SDA, SCL, 5);
#endif
#ifdef dht_h
DHT dht;
#endif

XML_tag_t Xtags[] =
{
  {"time-layout", "time-coordinate", "local", 19},
  {"temperature", "type", "hourly", 19},
  {NULL}
};

bool bGettingForecast;
char buffer[260]; // buffer for xml when in use

void xml_callback(int8_t item, int8_t idx, char *p)
{
  int8_t newtz;
  int8_t h;
  int8_t d;
  static int8_t hO;
  static int8_t lastd;

  switch(item)
  {
    case 0:            // valid time
      if(idx == 0)     // first item isn't really data
      {
        hO = 0;        // reset hour offset
        lastd = day();
        hvac.m_fcData[0].t = hvac.m_fcData[1].t; // keep a copy of first hour data
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

      if(idx == 1 && newtz != hvac.m_EE.tz) // DST change occurs this hour
      {
        hvac.m_EE.tz = newtz;
        getUdpTime(); // correct for new DST
      }
      break;
    case 1:                  // temperature
      if(idx)               // 1st value is not temp
        hvac.m_fcData[idx].t = atoi(p);
      break;
  }
}

XMLReader xml(buffer, 257, xml_callback);

void GetForecast()
{
  char *p_cstr_array[] =
  {
    (char *)"/xml/sample_products/browser_interface/ndfdXMLclient.php?zipCodeList=",
    hvac.m_EE.zipCode,
    (char*)"&Unit=e&temp=temp&Submit=Submit",
    NULL
  };

  bGettingForecast = xml.begin("graphical.weather.gov", p_cstr_array);
  if(!bGettingForecast)
    event.alert("Forecast failed");
}

//-----

Encoder rot(ENC_B, ENC_A);

bool EncoderCheck()
{
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
  startServer();
  eeRead(); // don't access EE before WiFi init
  hvac.init();
  display.init();
  getUdpTime(); // start the SNTP get
#ifdef SHT21_H
  sht.init();
#endif
#ifdef dht_h
  dht.setup(SDA, DHT::DHT22);
#endif
}

void loop()
{
  static uint8_t hour_save, min_save, sec_save;
  static int8_t lastSec;
  static int8_t lastHour;

  while( EncoderCheck() );
  display.checkNextion();  // check for touch, etc.
  handleServer(); // handles mDNS, web
#ifdef SHT21_H
  if(sht.service())
  {
    hvac.updateIndoorTemp( sht.getTemperatureF() * 10, sht.getRh() * 10 );
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
      float temp = dht.toFahrenheit(dht.getTemperature());

      if(dht.getStatus() == DHT::ERROR_NONE)
      {
        hvac.updateIndoorTemp( temp * 10, dht.getHumidity() * 10);
      }
      read_delay = 5; // update every 5 seconds
    }
#endif

    if(min_save != minute()) // only do stuff once per minute
    {
      min_save = minute();
      if (hour_save != hour()) // update our IP and time daily (at 2AM for DST)
      {
        eeWrite(); // update EEPROM if needed while we're at it (give user time to make many adjustments)
        if( (hour_save = hour()) == 2)
          getUdpTime();
      }

      if(--display.m_updateFcst <= 0 )  // usually every hour / 3 hours
      {
        GetForecast();
      }
    }
 
    if(bGettingForecast)
    {
      if(! (bGettingForecast = xml.service(Xtags)) )
      {
        switch(xml.getStatus())
        {
          case XML_DONE:
            hvac.enable();
            hvac.updatePeaks();
            event.alert("Forecast success");
            display.drawForecast(true);
            break;
          default:
            hvac.disable();
            hvac.m_notif = Note_Forecast;
            display.m_updateFcst = 5;    // retry in 5 mins
            break;
        }
      }
    }
  }
  delay(8); // rotary encoder and lines() need 8ms minimum
}

extern WiFiManager wifi;

void eeWrite() // write the settings if changed
{
  uint16_t old_sum = hvac.m_EE.sum;
  hvac.m_EE.sum = 0;
  hvac.m_EE.sum = Fletcher16((uint8_t *)&hvac.m_EE, sizeof(EEConfig));

  if(old_sum == hvac.m_EE.sum)
    return; // Nothing has changed?
  wifi.eeWriteData((uint8_t*)&hvac.m_EE, sizeof(EEConfig)); // WiFiManager already has an instance open, so use that at offset 64+
}

void eeRead()
{
  EEConfig eeTest;

  wifi.eeReadData((uint8_t*)&eeTest, sizeof(EEConfig));
  if(eeTest.size != sizeof(EEConfig)) return; // revert to defaults if struct size changes
  uint16_t sum = eeTest.sum;
  eeTest.sum = 0;
  eeTest.sum = Fletcher16((uint8_t *)&eeTest, sizeof(EEConfig));
  if(eeTest.sum != sum) return; // revert to defaults if sum fails
  memcpy(&hvac.m_EE, &eeTest, sizeof(EEConfig));
}

uint16_t Fletcher16( uint8_t* data, int count)
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
