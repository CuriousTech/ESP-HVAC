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

//uncomment to enable Arduino IDE Over The Air update code
#define OTA_ENABLE
//#define USE_OLED
#define DEBUG

//#define USE_SPIFFS // Uses 7K more program space

#include <Wire.h>
#ifdef USE_OLED
#include <ssd1306_i2c.h> // https://github.com/CuriousTech/WiFi_Doorbell/tree/master/Libraries/ssd1306_i2c
#endif

#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include "RunningMedian.h"
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include <UdpTime.h>
#include "eeMem.h"
#include <JsonParse.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonParse
#include <JsonClient.h>
#ifdef OTA_ENABLE
#include <FS.h>
#include <ArduinoOTA.h>
#endif
#ifdef USE_SPIFFS
#include <FS.h>
#include <SPIFFSEditor.h>
#else
#include "pages.h"
#endif
#include "jsonstring.h"
#include <AM2320.h>

int serverPort = 80;

#define ESP_LED  2  // low turns on ESP blue LED
#define PIR     12 // PIR sensor

enum reportReason
{
  Reason_Setup,
  Reason_Status,
  Reason_Alert,
  Reason_Motion,
};

// From HVAC.h
#define SNS_PRI   (1 << 0) // Single sensor overrides all others including internal
#define SNS_EN    (1 << 1) // Enabled = averaged between all enabled
#define SNS_C     (1 << 2) // Data from remote sensor is C or F
#define SNS_F     (1 << 3) // ""
#define SNS_TOPRI (1 << 4) // 1=timer is for priority, 0=for average
#define SNS_LO    (1 << 5) // lo/hi unused as of now
#define SNS_HI    (1 << 6)
#define SNS_WARN  (1 << 7) // internal flag for data timeout
#define SNS_NEG   (1 << 8)  // From remote or page, set this bit to disable a flag above

uint32_t lastIP;
uint32_t verifiedIP;
int nWrongPass;
bool bResetPri;

const char hostName[] ="Sensor1";

uint32_t sleepTimer = 60; // seconds delay after startup to enter sleep (Note: even if no AP found)
int8_t openCnt;

#ifdef USE_OLED
SSD1306 display(0x3c, 5, 4); // Initialize the oled display for address 0x3c, sda=5, sdc=4
#endif

AM2320 am;

WiFiManager wifi;  // AP page:  192.168.4.1
AsyncWebServer server( serverPort );
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonParse jsonParse(jsonCallback);
void jsonPushCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient jsonPush(jsonPushCallback);

UdpTime utime;

uint16_t temp;
uint16_t rh;

eeMem eemem;

bool bMotion;
uint16_t displayTimer;

String dataJson()
{
  jsonString js("state");

  js.Var("t", (uint32_t)now() - ( (ee.tz + utime.getDST() ) * 3600) );
  js.Var("temp", String((float)temp/10 + ((float)ee.tempCal/10), 1) );
  js.Var("rh", String((float)rh/10, 1) );
  js.Var("st", sleepTimer);
  return js.Close();
}

String settingsJson()
{
  jsonString js("settings");

  js.Var("tz",  ee.tz);
  js.Var("to", ee.time_off );
  js.Var("rate", ee.rate);
  js.Var("sleep", ee.sleep);
  js.Var("o", ee.bEnableOLED);
  js.Var("pir", ee.bPIR);
  js.Var("pri", ee.PriEn);
  js.Var("prisec", ee.priSecs);
  return js.Close();
}

void displayStart()
{
  if(ee.bEnableOLED == false && displayTimer == 0)
  {
#ifdef USE_OLED
    display.init();
#endif
  }
  displayTimer = 30;
}

void parseParams(AsyncWebServerRequest *request)
{
  char sztemp[100];
  char password[64];
 
  if(request->params() == 0)
    return;

  // get password first
  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);

    p->value().toCharArray(sztemp, 100);
    String s = wifi.urldecode(sztemp);
    switch( p->name().charAt(0)  )
    {
      case 'k': // key
        s.toCharArray(password, sizeof(password));
        break;
    }
  }

  uint32_t ip = request->client()->remoteIP();

  if( ip && ip == verifiedIP ); // can skip if last verified
  else if( strcmp(password, ee.szControlPassword))
  {
    if(nWrongPass == 0) // it takes at least 10 seconds to recognize a wrong password
      nWrongPass = 10;
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 10;

    jsonString js("hack");
    js.Var("ip", request->client()->remoteIP().toString() );
    js.Var("pass", password);
    ws.textAll(js.Close());

    lastIP = ip;
    return;
  }

  verifiedIP = ip;
  lastIP = ip;

  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);
    p->value().toCharArray(sztemp, 100);
    String s = wifi.urldecode(sztemp);
    bool which = (tolower(p->name().charAt(1) ) == 'd') ? 1:0;
    int val = s.toInt();
 
    switch( p->name().charAt(0)  )
    {
      case 'F': // temp offset
          ee.tempCal = val;
          break;
      case 'T': // to
          ee.time_off = val;
          break;
      case 'O': // OLED
          ee.bEnableOLED = (s == "true") ? true:false;
#ifdef USE_OLED
          display.clear();
          display.display();
#endif
          break;
      case 'r': // reset
          ESP.reset();
          break;
      case 's': // ssid
          s.toCharArray(ee.szSSID, sizeof(ee.szSSID));
          break;
      case 'p': // pass
          wifi.setPass(s.c_str());
          break;
    }
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

const char *jsonList1[] = { "cmd",
  "key",
  "tempOffset",
  "oled",
  "TZ",
  "TO",
  "rate",
  "sleep",
  "pir",
  "pri",
  "prisec",
  NULL
};

bool bKeyGood;
bool bDataMode;

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  if(bKeyGood == false && iName) return;  // only allow key set

  switch(iEvent)
  {
    case 0: // cmd
      switch(iName)
      {
        case 0: // key
          if(!strcmp(psValue, ee.szControlPassword)) // first item must be key
            bKeyGood = true;
          break;
        case 1: // tempOffset
          ee.tempCal = iValue;
          break;
        case 2: // OLED
          ee.bEnableOLED = iValue ? true:false;
          break;
        case 3: // TZ
          ee.tz = iValue;
          break;
        case 4: // TO
          ee.time_off = iValue;
          break;
        case 5: // rate
          ee.rate = iValue;
          break;
        case 6: // sleep
          ee.sleep = iValue;
          break;
        case 7: // pir
          ee.bPIR = iValue ? true:false;
          break;
        case 8: // pri
          ee.PriEn = iValue & 0xF; // can take C or F as well as EN or PRI
          bResetPri = true;
          break;
        case 9: // prisec
          ee.priSecs = iValue;
          break;
      }
      break;
  }
}

const char *jsonListPush[] = { "time",
  "time", // 0
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
          setTime(iValue + ( (ee.tz + utime.getDST() ) * 3600));
          break;
      }
      break;
  }
}

void CallHost(reportReason r, String sStr)
{
  if(wifi.state() != ws_connected || ee.hostIP[0] == 0) // no host set
    return;

  String sUri = "/wifi?name=\"";
  sUri += hostName;
  sUri += "\"&reason=";

  switch(r)
  {
    case Reason_Setup:
      sUri += "setup&port="; sUri += serverPort;
      break;
    case Reason_Status:
      sUri += "status&temp="; sUri += String(temp/10, 1);
      sUri += "&rh="; sUri += String(rh/10, 1);
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
  String url = ip.toString();
  jsonPush.begin(url.c_str(), sUri.c_str(), ee.hostPort, false, false, NULL, NULL);
  jsonPush.addList(jsonListPush);
}

void sendTemp()
{
  if(wifi.state() != ws_connected || ee.hvacIP[0] == 0) // not set
    return;

  String sUri = String("/s?key=");
  sUri += ee.szControlPassword;
  sUri += "&rmttemp="; sUri += temp;
  sUri += "&rmtrh="; sUri += rh;
  if(bResetPri)
  {
    sUri += "&rmtflg="; sUri += (SNS_NEG | SNS_PRI | SNS_EN); // clear priority and en if it changed
    sUri += "&rmtflg="; sUri += ee.PriEn; // 1=priority, 2=enable(avg)
    bResetPri = false;
  }
  if(ee.bPIR)
  {
    sUri += "&rmtflg=";
    sUri += ee.PriEn; // 1=priority, 2=enable(avg)
    if(ee.priSecs)
    {
      sUri += "&rmtto="; sUri += ee.priSecs;
    }
  }
  sUri += "&rmtname="; sUri += '1SNS';

  IPAddress ip(ee.hvacIP);
  String url = ip.toString();
  jsonPush.begin(url.c_str(), sUri.c_str(), 80, false, false, NULL, NULL);
  jsonPush.addList(jsonListPush);
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
      client->text( dataJson() );
      client->text( settingsJson() );
      client->ping();
      openCnt++;
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
      bDataMode = false; // turn off numeric display and frequent updates
      if(openCnt)
        openCnt--;
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

          char *pCmd = strtok((char *)data, ";"); // assume format is "name;{json:x}"
          char *pData = strtok(NULL, "");

          if(pCmd == NULL || pData == NULL) break;

          uint32_t ip = client->remoteIP();

          bKeyGood = (ip && verifiedIP == ip) ? true:false; // if this IP sent a good key, no need for more
          jsonParse.process(pCmd, pData);
          if(bKeyGood)
            verifiedIP = ip;
        }
      }
      break;
  }
}

void setup()
{
  pinMode(ESP_LED, OUTPUT);
  digitalWrite(ESP_LED, LOW);
//  pinMode(PIR, INPUT);
#ifdef DEBUG
  Serial.begin(115200);
//  delay(3000);
  Serial.println();
#endif

  // initialize dispaly
#ifdef USE_OLED
  display.init();
//  display.flipScreenVertically();
  display.clear();
  display.display();
#else
  am.begin(5, 4);
#endif

  WiFi.hostname(hostName);
  wifi.autoConnect(hostName, ee.szControlPassword);

#ifdef USE_SPIFFS
  SPIFFS.begin();
  server.addHandler(new SPIFFSEditor("admin", controlPassword));
#endif

  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    if(wifi.isCfg())
      request->send( 200, "text/html", wifi.page() );
    else
    {
      parseParams(request);
      bDataMode = true;
#ifdef USE_SPIFFS
      request->send(SPIFFS, "/index.html");
#else
      request->send_P(200, "text/html", page_index);
#endif
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
//    request->send(404);
  });

  server.onFileUpload([](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  });

  server.begin();

#ifdef OTA_ENABLE
  ArduinoOTA.begin();
#endif

  jsonParse.addList(jsonList1);
  digitalWrite(ESP_LED, HIGH);
  if(ee.rate == 0) ee.rate = 60;
  sleepTimer = ee.sleep;
}

uint16_t stateTimer = ee.rate;

void sendState()
{
  ws.textAll(dataJson());
  stateTimer = ee.rate;
}

RunningMedian<uint16_t, 20> tempMedian[2];

void findHVAC() // This seems to show only 3 at a time
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

void loop()
{
  static uint8_t hour_save, sec_save;
  static uint8_t cnt = 0;
  bool bNew;

  MDNS.update();
#ifdef OTA_ENABLE
  ArduinoOTA.handle();
#endif
  if(!wifi.isCfg() && ee.useTime)
    utime.check(ee.tz);

  wifi.service();
  if(wifi.connectNew())
  {
    MDNS.begin( hostName );
    MDNS.addService("iot", "tcp", serverPort);
    if(ee.useTime)
      utime.start();
    findHVAC();
    CallHost(Reason_Setup, "");
  }

  static bool last_pir;
  if(digitalRead(PIR) != last_pir)
  {
    last_pir = digitalRead(PIR);
    if(last_pir)
      CallHost(Reason_Motion, "");
  }

  if(sec_save != second()) // only do stuff once per second (loop is maybe 20-30 Hz)
  {
    sec_save = second();

    if (hour_save != hour())
    {
      hour_save = hour();
      if((hour_save&1) == 0)
        CallHost(Reason_Setup, "");
      if(hour_save == 2 && ee.useTime)
      {
        utime.start(); // update time daily at DST change
      }
      eemem.update(); // update EEPROM if needed while we're at it (give user time to make many adjustments)
    }

    if((second() % 5) == 0)
    {
      float temp2, rh2;
      if(am.measure(temp2, rh2))
      {
        tempMedian[0].add(( 1.8 * temp2 + 32.0) * 10 );
        tempMedian[0].getAverage(2, temp2);
        tempMedian[1].add(rh2 * 10);
        tempMedian[1].getAverage(2, rh2);
        temp = temp2;
        rh = rh2;
        sendState();
        static bool bTog;
        if(bTog = !bTog)
          sendTemp();
        else
          CallHost(Reason_Status, "");
        sleepTimer = ee.sleep;
      }
      else
      {
       CallHost(Reason_Alert, "AM2320 error");
      }
    }

    if(nWrongPass)
      nWrongPass--;

    if(--stateTimer == 0) // a 60 second keepAlive
      sendState();

    if(displayTimer) // temp display on thing
      displayTimer--;

    if(sleepTimer && openCnt == 0 && wifi.isCfg() == false) // don't sleep until all ws connections are closed
    {
      if(--sleepTimer == 0)
      {
        if(ee.time_off)
        {
          uint32_t us = ee.time_off * 60022000;  // minutes to us adjusted
#ifdef USE_OLED
          display.displayOff();
#endif
          ESP.deepSleep(us, WAKE_RF_DEFAULT);
        }
      }
    }

    digitalWrite(ESP_LED, LOW);
    delay(8);
    digitalWrite(ESP_LED, HIGH);
  }

  if(wifi.state() == ws_config) // WiFi cfg prints AP IP
  {
    delay(40);
    digitalWrite(ESP_LED, !digitalRead(ESP_LED) );
    return;
  }
  else if(wifi.state() == ws_connecting) // WiFi connect will draw OLED
  {
    digitalWrite(ESP_LED, LOW);
    delay(8);
    digitalWrite(ESP_LED, HIGH);
    return;
  }

#ifdef USE_OLED
  static bool bClear;
  bool bDraw = (ee.bEnableOLED || displayTimer || bDataMode);
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
