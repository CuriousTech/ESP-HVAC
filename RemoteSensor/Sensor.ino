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

// Simple remote sensor for HVAC, with OLED display, AM2320 and PIR sensor or button
// This uses HTTP GET to send temp/rh

// Build with Arduino IDE 1.8.9, esp8266 SDK 2.5.0

// Uncomment if using a direct OLED display
//#define USE_OLED

//uncomment to enable Arduino IDE Over The Air update code
#define OTA_ENABLE

#include <Wire.h>
#ifdef USE_OLED
#include <ssd1306_i2c.h> // https://github.com/CuriousTech/WiFi_Doorbell/tree/master/Libraries/ssd1306_i2c
#endif

#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include <UdpTime.h> // https://github.com/CuriousTech/ESP07_WiFiGarageDoor/tree/master/libraries/UdpTime
#include "eeMem.h"
#include <JsonParse.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonParse
#include <JsonClient.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonClient
#ifdef OTA_ENABLE
#include <FS.h>
#include <ArduinoOTA.h>
#endif
#include <FS.h>
#include <SPIFFSEditor.h>
#include "pages.h"
#include "jsonstring.h"
#include "TempArray.h"

// Uncomment only one
#include "tuya.h"  // Uncomment device in tuya.cpp
//#include "BasicSensor.h"

int serverPort = 80;

enum reportReason
{
  Reason_Setup,
  Reason_Status,
  Reason_Alert,
  Reason_Motion,
};

IPAddress lastIP;
IPAddress verifiedIP;
int nWrongPass;
bool bResetPri = true; // send at start

uint32_t sleepTimer = 60; // seconds delay after startup to enter sleep (Note: even if no AP found)
int8_t nWsConnected;

WiFiManager wifi;  // AP page:  192.168.4.1
AsyncWebServer server( serverPort );
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
int WsClientID;

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonParse jsonParse(jsonCallback);
void jsonPushCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient jsonPush(jsonPushCallback);

UdpTime utime;

eeMem eemem;

bool bPIRTrigger;
uint16_t displayTimer;

#ifdef TUYA_H
TuyaInterface sensor;
#else
BasicInterface sensor;
#endif

#ifdef USE_OLED
SSD1306 display(0x3c, 5, 4); // Initialize the oled display for address 0x3c, sda=5, sdc=4
#endif

TempArray temps;

String settingsJson()
{
  jsonString js("settings");

  js.Var("tz",  ee.tz);
  js.Var("name", ee.szName);
  js.Var("to", ee.time_off );
  js.Var("srate", ee.sendRate);
  js.Var("lrate", ee.logRate);
  js.Var("sleep", ee.sleep);
  js.Var("o", ee.e.bEnableOLED);
  js.Var("l1", sensor.m_bLED[0]);
  js.Var("l2", sensor.m_bLED[1]);
  js.Var("pir", ee.e.bPIR);
  js.Var("pirpin", ee.pirPin);
  js.Var("pri", ee.e.PriEn);
  js.Var("prisec", ee.priSecs);
  js.Var("ch", ee.e.bCall);
  js.Var("ID", ee.sensorID);
  js.Var("cf", sensor.m_bCF);
  js.Var("df", sensor.m_dataFlags);
  js.Var("si", temps.m_bSilence);
  return js.Close();
}

String dataJson()
{
  jsonString js("state");

  js.Var("t", (uint32_t)now() - ( (ee.tz + utime.getDST() ) * 3600) );
  js.Var("df", sensor.m_dataFlags);
  js.Var("temp", String( (float)(sensor.m_values[DE_TEMP]+ee.tempCal) / 10, 1 ) );
  js.Var("rh", String((float)sensor.m_values[DE_RH] / 10, 1) );
  js.Var("st", sleepTimer);
  int sig = WiFi.RSSI();
  sensor.setSignal(sig);
  js.Var("rssi", sig);

  if(sensor.m_dataFlags & DF_CO2 )
    js.Var("co2", sensor.m_values[DE_CO2] );
  if(sensor.m_dataFlags & DF_CH2O )
    js.Var("ch2o", sensor.m_values[ DE_CH2O ] );
  if(sensor.m_dataFlags & DF_VOC )
    js.Var("voc", sensor.m_values[ DE_VOC ] );
  
  return js.Close();
}

void displayStart()
{
  if(ee.e.bEnableOLED == false && displayTimer == 0)
  {
#ifdef USE_OLED
    display.init();
#endif
  }
  displayTimer = 30;
}

const char *jsonList1[] = { "cmd",
  "key",
  "ssid",
  "password",
  "name",
  "reset",
  "tempOffset",
  "oled",
  "TZ",
  "TO",
  "srate",
  "lrate", // 10
  "pir",
  "pri",
  "prisec",
  "led1",
  "led2",
  "cf",
  "ch",
  "hostip",
  "hist",
  "ID", // 20
  "sleep",
  "pirpin",
  "alertidx",
  "alertlevel",
  "silence",
  NULL
};

void parseParams(AsyncWebServerRequest *request)
{
  if(nWrongPass && request->client()->remoteIP() != lastIP)  // if different IP drop it down
    nWrongPass = 10;
  lastIP = request->client()->remoteIP();

  for ( uint8_t i = 0; i < request->params(); i++ )
  {
    AsyncWebParameter* p = request->getParam(i);
    String s = request->urlDecode(p->value());

    uint8_t idx;
    for(idx = 1; jsonList1[idx]; idx++)
      if( p->name().equals(jsonList1[idx]) )
        break;
    if(jsonList1[idx])
    {
      int iValue = s.toInt();
      if(s == "true") iValue = 1;
      jsonCallback(0, idx - 1, iValue, (char *)s.c_str());
    }
  }
}

bool bKeyGood;
bool bDataMode;

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  if(bKeyGood == false && iName) return;  // only allow key set
  static int alertIdx;

  switch(iEvent)
  {
    case 0: // cmd
      switch(iName)
      {
        case 0: // key
          if(!strcmp(psValue, ee.szControlPassword)) // first item must be key
          {
            bKeyGood = true;
            verifiedIP = lastIP;
          }
          break;
        case 1: // wifi SSID
          strncpy((char *)&ee.szSSID, psValue, sizeof(ee.szSSID));
          break;
        case 2: // wifi password
          wifi.setPass(psValue);
          break;
        case 3:
          if(!strlen(psValue))
            break;
          strncpy(ee.szName, psValue, sizeof(ee.szName));
          eemem.update();
          delay(1000);
          ESP.reset();
          break;
        case 4: // reset
          eemem.update();
          delay(1000);
          ESP.reset();
          break;
        case 5: // tempOffset
          ee.tempCal = iValue;
          break;
        case 6: // OLED
          ee.e.bEnableOLED = iValue;
          break;
        case 7: // TZ
          ee.tz = iValue;
          break;
        case 8: // TO
          ee.time_off = iValue;
          break;
        case 9: // srate
          ee.sendRate = iValue;
          break;
        case 10: // lrate
          ee.logRate = iValue;
          break;
        case 11: // pir
          ee.e.bPIR = iValue;
          break;
        case 12: // pri
          ee.e.PriEn = iValue & 0x3; // EN or PRI
          bResetPri = true;
          break;
        case 13: // prisec
          ee.priSecs = iValue;
          break;
        case 14: // led1
          sensor.setLED(0, iValue ? true:false);
          break;
        case 15: // led2
          sensor.setLED(1, iValue ? true:false);
          break;
        case 16: // cf
          sensor.setCF(iValue ? true:false);
          bResetPri = true;
          break;
        case 17: // ch
          ee.e.bCall = iValue;
          if(iValue) CallHost(Reason_Setup, ""); // test
          break;
        case 18: // hostip
          ee.hostPort = 80;
          ee.hostIP[0] = lastIP[0];
          ee.hostIP[1] = lastIP[1];
          ee.hostIP[2] = lastIP[2];
          ee.hostIP[3] = lastIP[3];
          ee.e.bCall = 1;
          CallHost(Reason_Setup, ""); // test
          break;
        case 19: // hist
          temps.historyDump(true, ws, WsClientID);
          break;
        case 20:
          ee.sensorID = iValue;
          break;
        case 21:
          ee.sleep = iValue;
          break;
        case 22:
          ee.pirPin = iValue;
          break;
        case 23:
          alertIdx = constrain(iValue, 0, 16);
          break;
        case 24: // set alertidx first
          ee.wAlertLevel[alertIdx] = iValue;
          break;
        case 25:
          temps.m_bSilence = iValue ? true:false;
          break;
      }
      break;
  }
}

// Time in hh:mm[:ss][AM/PM]
String timeFmt(bool do_sec, bool do_M)
{
  String r = "";
  if(hourFormat12() < 10) r = " ";
  r += hourFormat12();
  r += ":";
  if(minute() < 10) r += "0";
  r += minute();
  if(do_sec)
  {
    r += ":";
    if(second() < 10) r += "0";
    r += second();
    r += " ";
  }
  if(do_M)
  {
      r += isPM() ? "PM":"AM";
  }
  return r;
}

const char *jsonListPush[] = { "time",
  "time", // 0
  "ppkw",
  NULL
};

void jsonPushCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  static uint8_t failCnt;

  switch(iEvent)
  {
    case -1: // status
      if(iName >= JC_TIMEOUT)
      {
        if(++failCnt > 5)
          ESP.restart();
      }
      else failCnt = 0;
      break;
    case 0: // time
      switch(iName)
      {
        case 0: // time
          setTime(iValue + (ee.tz * 3600));
          utime.DST();
          setTime(iValue + ((ee.tz + utime.getDST()) * 3600));
          temps.m_bValidDate = true;
          break;
      }
      break;
  }
}

struct cQ
{
  IPAddress ip;
  String sUri;
  uint16_t port;
};
#define CQ_CNT 8
cQ queue[CQ_CNT];
uint8_t qI;

void checkQueue()
{
  static uint32_t cqTime;
  static uint8_t qIdx;

  if(wifi.state() != ws_connected)
    return;
  if(jsonPush.status() != JC_IDLE) // These should be fast, so kill if not
  {
    if( (millis() - cqTime) > 500)
    {
      jsonPush.end();
      cqTime = millis();
    }
    return;
  }

  if(qIdx == qI || queue[qIdx].port == 0) // Nothing to do
    return;

  if( jsonPush.begin(queue[qIdx].ip, queue[qIdx].sUri.c_str(), queue[qIdx].port, false, false, NULL, NULL, 300) )
  {
    jsonPush.addList(jsonListPush);
    cqTime = millis();
    queue[qIdx].port = 0;
    if(++qIdx >= CQ_CNT)
      qIdx = 0;
  }
}

bool callQueue(IPAddress ip, String sUri, uint16_t port)
{
  if(queue[qI].port == 0)
  {
    queue[qI].ip = ip;
    queue[qI].sUri = sUri;
    queue[qI].port = port;
    if(++qI >= CQ_CNT)
      qI = 0;
    return true;
  }
  return false; // full
}

void CallHost(reportReason r, String sStr)
{
  if(wifi.state() != ws_connected || ee.hostIP[0] == 0 || ee.e.bCall == false)
    return;

  String sUri = "/wifi?name=\"";
  sUri += ee.szName;
  sUri += "\"&reason=";

  switch(r)
  {
    case Reason_Setup:
      sUri += "setup&port="; sUri += serverPort;
      break;
    case Reason_Status:
      if(sensor.m_values[DE_TEMP] == 0)
        return;

      sUri += "status&temp="; sUri += String( (float)sensor.m_values[DE_TEMP]/10 , 1);
      sUri += "&rh="; sUri += String( (float)sensor.m_values[DE_RH]/10 , 1);
      break;
    case Reason_Alert:
      sUri += "alert&value=\"";
      sUri += sStr;
      sUri += "\"";
      break;
    case Reason_Motion:
      sUri += "motion";
      break;
  }

  IPAddress ip(ee.hostIP);
  callQueue(ip, sUri, ee.hostPort);
}

void sendTemp()
{
  if(wifi.state() != ws_connected || ee.hvacIP[0] == 0) // not set
    return;

  String sUri = String("/s?key=");
  sUri += ee.szControlPassword;
  sUri += "&rmtname="; sUri += ee.sensorID;
  sUri += "&rmttemp="; sUri += sensor.m_values[DE_TEMP];
  sUri += "&rmtrh="; sUri += sensor.m_values[DE_RH];

  uint8_t flg = ee.e.PriEn | (ee.e.bCF ? SNS_F : SNS_C); // PriEn: 1=priority, 2=enable(avg)

  if(bResetPri)
  {
    sUri += "&rmtflg="; sUri += (SNS_NEG | SNS_PRI | SNS_EN | SNS_F | SNS_C); // clear all flags that may have changed
    sUri += "&rmtflg="; sUri += flg;
    bResetPri = false;
  }
  if(ee.e.bPIR && bPIRTrigger )
  {
    sUri += "&rmtflg=";
    sUri += flg;
    if(ee.priSecs)
    {
      sUri += "&rmtto="; sUri += ee.priSecs;
    }
    bPIRTrigger = false;
  }

  IPAddress ip(ee.hvacIP);
  callQueue(ip, sUri, 80);
}

uint16_t stateTimer = 10;

void sendState()
{
  if(nWsConnected)
    ws.textAll(dataJson());
  stateTimer = ee.sendRate;
}

void findHVAC() // This seems to show only 6 at a time
{
  int n = MDNS.queryService("iot", "tcp");
  int d;
  for(int i = 0; i < n; ++i)
  {
    char szName[38];
    MDNS.hostname(i).toCharArray(szName, sizeof(szName));
    strtok(szName, "."); // remove .local

    if(!strcmp(szName, "HVAC"))
    {
      ee.hvacIP[0] = MDNS.IP(i)[0]; // update IP
      ee.hvacIP[1] = MDNS.IP(i)[1];
      ee.hvacIP[2] = MDNS.IP(i)[2];
      ee.hvacIP[3] = MDNS.IP(i)[3];
      break;
    }
  }
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{  //Handle WebSocket event
  static bool bRestarted = true;
  String s;

  switch(type)
  {
    case WS_EVT_CONNECT:      //client connected
      if(bRestarted)
      {
        bRestarted = false;
//        client->text("alert;Restarted");
      }
      client->keepAlivePeriod(50);
      client->text( settingsJson() );
      client->text( dataJson() );
      client->ping();
      nWsConnected++;
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
      bDataMode = false; // turn off numeric display and frequent updates
      if(nWsConnected)
        nWsConnected--;
      break;
    case WS_EVT_ERROR:    //error was received from the other end
      break;
    case WS_EVT_PONG:    //pong message was received (in response to a ping request maybe)
      break;
    case WS_EVT_DATA:  //data packet
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      if(info->final && info->index == 0 && info->len == len){
        //the whole message is in a single frame and we got all of it's data
        if(info->opcode == WS_TEXT){
          data[len] = 0;

          char *pCmd = strtok((char *)data, ";"); // assume format is "event;{json:x}"
          char *pData = strtok(NULL, "");

          if(pCmd == NULL || pData == NULL) break;

          uint32_t ip = client->remoteIP();
          WsClientID = client->id();

          bKeyGood = (ip && verifiedIP == ip) ? true:false; // if this IP sent a good key, no need for more
          jsonParse.process(pCmd, pData);
          if(bKeyGood)
            verifiedIP = ip;
        }
      }
      break;
  }
}

void WsSend(String s)
{
  ws.textAll(s);
}

void alert(String txt)
{
  String s = "alert;{\"text\":\"";
  s += txt;
  s += "\"}";
  ws.textAll(s);
}

void setup()
{
  eemem.init();
  sensor.init(ee.e.bCF);

  // initialize dispaly
#ifdef USE_OLED
  display.init();
//  display.flipScreenVertically();
  display.clear();
  display.display();
#endif

  wifi.autoConnect(ee.szName, ee.szControlPassword);

  SPIFFS.begin();
  server.addHandler(new SPIFFSEditor("admin", ee.szControlPassword));

  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    if(wifi.state() == ws_config)
      request->send( 200, "text/html", wifi.page() );
    else
    {
      parseParams(request);
      bDataMode = true;
//      request->send(SPIFFS, "/index.html");
      request->send_P(200, "text/html", page_index);
    }
  });
  server.on( "/s", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);

    String page = "{\"ip\": \"";
    page += WiFi.localIP().toString();
    page += ":";
    page += serverPort;
    page += "\"}";
    request->send( 200, "text/json", page );
  });
  server.on( "/json", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);
    request->send( 200, "text/json", settingsJson() );
  });
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon, sizeof(favicon));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404);
  });

  server.onFileUpload([](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  });

  server.begin();

#ifdef OTA_ENABLE
  ArduinoOTA.setHostname(ee.szName);
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    eemem.update();
    sensor.setLED(0, false); // set it all to off
    temps.saveData();
    SPIFFS.end();
    alert("OTA Update Started");
    ws.closeAll();
  });
#endif

  jsonParse.addList(jsonList1);
  sensor.setLED(0, false);
  if(ee.sendRate == 0) ee.sendRate = 60;
  sleepTimer = ee.sleep;
  temps.init(sensor.m_dataFlags);
}

void loop()
{
  static uint8_t hour_save, sec_save;
  static uint8_t cnt = 0;
  bool bNew;

  MDNS.update();
#ifdef OTA_ENABLE
  ArduinoOTA.handle();
#endif
  if(wifi.state() == ws_connected && ee.e.bUseTime)
  {
    if(utime.check(ee.tz))
      temps.m_bValidDate = true;
  }
  wifi.service();
  if(wifi.connectNew())
  {
    MDNS.begin( ee.szName );
    MDNS.addService("iot", "tcp", serverPort);
    if(ee.e.bUseTime) // Host and HVAC return current time
      utime.start();
    findHVAC();
    CallHost(Reason_Setup, "");
  }

  if(ee.pirPin)
  {
    static bool last_pir;
    if(digitalRead(ee.pirPin) != last_pir)
    {
      last_pir = digitalRead(ee.pirPin);
      if(last_pir)
      {
        CallHost(Reason_Motion, "");
        if(ee.e.bPIR && bPIRTrigger)
          sendTemp();
      }
    }
  }

  static int htimer = 10;
  if(--htimer == 0)
  {
    temps.historyDump(false, ws, WsClientID);
    htimer = 30;
  }

  if(int err = sensor.service())
  {
    String s = "Sensor error ";
    s += err;
    alert(s);
  }

  checkQueue();

  if(sec_save != second()) // only do stuff once per second (loop is maybe 20-30 Hz)
  {
    sec_save = second();

    if(hour_save != hour())
    {
      hour_save = hour();
      if((hour_save&1) == 0)
        CallHost(Reason_Setup, "");
      if(hour_save == 2 && ee.e.bUseTime)
        utime.start(); // update time daily at DST change
      eemem.update(); // update EEPROM if needed while we're at it (give user time to make many adjustments)
    }

    if(nWrongPass)
      nWrongPass--;

    if(displayTimer) // temp display on thing
      displayTimer--;

    if(sleepTimer && nWsConnected == 0 && wifi.state() != ws_config) // don't sleep until all ws connections are closed
    {
      if(--sleepTimer == 0)
      {
        if(ee.time_off)
        {
          uint32_t us = ee.time_off * 60022000;  // minutes to us calibrated
#ifdef USE_OLED
          display.displayOff();
#endif
          ESP.deepSleep(us, WAKE_RF_DEFAULT);
        }
      }
    }

    if(--stateTimer == 0 || sensor.m_bUpdated) // a 60 second keepAlive
    {
      if(sensor.m_bUpdated)
        temps.update(sensor.m_values);
      sensor.m_bUpdated = false;
      sendState();
    }

    static uint8_t timer = 5;
    if(--timer == 0)
    {
      timer = 30;
      if(sensor.m_values[DE_RH])
        CallHost(Reason_Status, "");
    }

    static uint8_t sendTimer = 10;
    if(--sendTimer == 0)
    {
      sendTimer = ee.sendRate;
      sendTemp();
    }
    static uint8_t addTimer = 10;
    if(--addTimer == 0)
    {
      addTimer = ee.logRate;
      temps.add( (uint32_t)now() - ( (ee.tz + utime.getDST() ) * 3600), ws, WsClientID);
    }
  }

  if(wifi.state() == ws_config) // WiFi cfg prints AP IP
  {
    delay(40);
    sensor.setLED(0, !sensor.m_bLED[0] );
    return;
  }
  else if(wifi.state() == ws_connecting) // WiFi connect will draw OLED
  {
    sensor.setLED(0, true);
    delay(8);
    sensor.setLED(0, false);
    return;
  }

#ifdef USE_OLED
  static bool bClear;
  bool bDraw = (ee.e.bEnableOLED || displayTimer || bDataMode);
  if(bDraw == false && bClear)
    return;

  // draw the screen here
  display.clear();
  if(bDraw)
  {
    String s = timeFmt(true, true);
    s += "  ";
    s += dayShortStr(weekday());
    s += " ";
    s += String(day());
    s += " ";
    s += monthShortStr(month());
    s += "  ";

    Scroller(s);

    display.drawPropString(2, 47, String((float)temp/10 + ((float)ee.tempCal/10), 1) + "]");
    display.drawPropString(64, 47, String((float)rh/10, 1) + "%");
    bClear = false;
  }
  else
    bClear = true;
  display.display();
#endif
}

// Text scroller optimized for very long lines
#ifdef USE_OLED
void Scroller(String s)
{
  static int16_t ind = 0;
  static char last = 0;
  static int16_t x = 0;

  if(last != s.charAt(0)) // reset if content changed
  {
    x = 0;
    ind = 0;
  }
  last = s.charAt(0);
  int len = s.length(); // get length before overlap added
  s += s.substring(0, 18); // add ~screen width overlap
  int w = display.propCharWidth(s.charAt(ind)); // first char for measure
  String sPart = s.substring(ind, ind + 18);
  display.drawPropString(x, 0, sPart );

  if( --x <= -(w))
  {
    x = 0;
    if(++ind >= len) // reset at last char
      ind = 0;
  }
}
#endif
