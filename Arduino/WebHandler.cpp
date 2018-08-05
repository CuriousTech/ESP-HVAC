// Do all the web stuff here

//uncomment to enable Arduino IDE Over The Air update code
#define OTA_ENABLE

#define USE_SPIFFS // saves 11K of program space, loses 800 bytes dynamic

#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#ifdef OTA_ENABLE
#include <FS.h>
#include <ArduinoOTA.h>
#endif
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include "WebHandler.h"
#include "HVAC.h"
#include <JsonParse.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonParse
#include "display.h" // for display.Note()
#include "eeMem.h"
#ifdef USE_SPIFFS
#include <FS.h>
#include <SPIFFSEditor.h>
#else
#include "pages.h"
#endif
#include <XMLReader.h>

//-----------------
int serverPort = 85;            // Change to 80 for normal access

IPAddress ipFcServer(192,168,0,100);    // local forecast server and port
int nFcPort = 83;

//-----------------
AsyncWebServer server( serverPort );
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
extern HVAC hvac;
extern Display display;
WiFiManager wifi;  // AP page:  192.168.4.1
AsyncClient fc_client;

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonParse remoteParse(remoteCallback);
void fcPage(AsyncWebServerRequest *request);

int xmlState;
void GetForecast(void);

int nWrongPass;
uint32_t lastIP;
bool bKeyGood;
int WsClientID;
int WsRemoteID;

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{  //Handle WebSocket event
  static bool rebooted = true;
  String s;

  switch(type)
  {
    case WS_EVT_CONNECT:      //client connected
      if(rebooted)
      {
        rebooted = false;
        client->text("alert;Restarted");
      }
      s = String("settings;") + hvac.settingsJson().c_str(); // update everything on start
      client->text(s);
      s = String("state;") + dataJson().c_str();
      client->text(s);
      client->ping();
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
    case WS_EVT_ERROR:    //error was received from the other end
      if(hvac.m_bRemoteStream && client->id() == WsRemoteID) // stop remote
      {
         hvac.m_bRemoteStream = false;
         hvac.m_bLocalTempDisplay = !hvac.m_bRemoteStream; // switch to showing local/remote color
         hvac.m_notif = Note_RemoteOff;
      }
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
          bKeyGood = false; // for callback (all commands need a key)
          WsClientID = client->id();
          remoteParse.process(pCmd, pData);
        }
      }
      break;
  }
}

const char *jsonList1[] = { "state",  "temp", "rh", "tempi", "rhi", "rmt", NULL };
extern const char *cmdList[];
const char *jsonList3[] = { "alert", NULL };

void startServer()
{
  WiFi.hostname("HVAC");
  wifi.autoConnect("HVAC", ee.password); // Tries configured AP, then starts softAP mode for config

  Serial.println("");
  if(wifi.isCfg() == false)
  {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  
    if( !MDNS.begin ( "HVAC", WiFi.localIP() ) )
      Serial.println ( "MDNS responder failed" );
  }

#ifdef USE_SPIFFS
  SPIFFS.begin();
  server.addHandler(new SPIFFSEditor("admin", ee.password));
#endif

  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on ( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    if(wifi.isCfg())
      request->send( 200, "text/html", wifi.page() );
  });
  server.on ( "/iot", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);
#ifdef USE_SPIFFS
    request->send(SPIFFS, "/index.html");
#else
    request->send_P(200, "text/html", page1);
#endif
  });

  server.on ( "/s", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);
    request->send ( 200, "text/html", "OK" );
  });

  server.on ( "/json", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    String s = hvac.settingsJson();
    request->send ( 200, "text/json", s);
  });

  server.on ( "/settings", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);
#ifdef USE_SPIFFS
    request->send(SPIFFS, "/settings.html");
#else
    request->send_P(200, "text/html", page2);
#endif
  });
  server.on ( "/chart.html", HTTP_GET, [](AsyncWebServerRequest *request){
    parseParams(request);
#ifdef USE_SPIFFS
    request->send(SPIFFS, "/chart.html");
#else
    request->send_P(200, "text/html", page_chart);
#endif
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.ico");
//    request->send(404);
  });
  server.onNotFound([](AsyncWebServerRequest *request){
//    request->send(404);
  });
  server.onFileUpload([](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  });

  // respond to GET requests on URL /heap
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server.begin();

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", serverPort);

  remoteParse.addList(jsonList1);
  remoteParse.addList(cmdList);
  remoteParse.addList(jsonList3);

#ifdef OTA_ENABLE
  ArduinoOTA.begin();
#endif

  fc_client.onConnect([](void* obj, AsyncClient* c) { fc_onConnect(c); });
  fc_client.onData([](void* obj, AsyncClient* c, void* data, size_t len){fc_onData(c, static_cast<char*>(data), len); });
  fc_client.onDisconnect([](void* obj, AsyncClient* c) { fc_onDisconnect(c); });
  fc_client.onTimeout([](void* obj, AsyncClient* c, uint32_t time) { fc_onTimeout(c, time); });
}

void handleServer()
{
  MDNS.update();
  static int n;

  if(++n >= 10)
  {
    historyDump(false);
    n = 0;
  }
#ifdef OTA_ENABLE
// Handle OTA server.
  ArduinoOTA.handle();
  yield();
#endif
}

void WsSend(char *txt, const char *type)
{
  ws.textAll(String(type) + String(";") + String(txt));
}

void secondsServer() // called once per second
{
  if(nWrongPass)
    nWrongPass--;

  if(hvac.stateChange() || hvac.tempChange())
    WsSend((char*)dataJson().c_str(), "state" );

  String s = hvac.settingsJsonMod(); // returns "{}" if nothing has changed
  if(s.length() > 2)
    ws.textAll(String("settings;") + s); // update anything changed

  if(display.m_bUpdateFcst == true && display.m_bUpdateFcstDone == false)
  {
    display.m_bUpdateFcst = false;
    if(ee.bNotLocalFcst)
      GetForecast();
    else if(fc_client.connected() == false)    // get preformatted data from local server
    {
       display.m_fcData[0].temp = display.m_fcData[1].temp; // keep a copy of first 3hr data
       display.m_fcData[0].tm = display.m_fcData[1].tm;
       fc_client.connect(ipFcServer, nFcPort);
    }
  }

  if(xmlState)
  {
      switch(xmlState)
      {
        case XML_COMPLETED:
        case XML_DONE:
          hvac.enable();
          display.m_bUpdateFcstDone = true;
          break;
        case XML_TIMEOUT:
          WsSend("Forcast timeout", "print");
          hvac.disable();
          hvac.m_notif = Note_Forecast;
          display.m_bUpdateFcstDone = true;
          break;
      }
      xmlState = 0;
  }
}

void parseParams(AsyncWebServerRequest *request)
{
  char temp[100];
  char password[64];

  if(request->params() == 0)
    return;

  for ( uint8_t i = 0; i < request->params(); i++ ) // password may be at end
  {
    AsyncWebParameter* p = request->getParam(i);
    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);

    if(p->name() == "key")
    {
      s.toCharArray(password, sizeof(password));
    }
  }

  uint32_t ip = request->client()->remoteIP();

  if(strcmp(ee.password, password))
  {
    if(nWrongPass == 0)
      nWrongPass = 10;
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 10;
    String data = "{\"ip\":\"";
    data += request->client()->remoteIP().toString();
    data += "\",\"pass\":\"";
    data += password; // bug - String object adds a NULL
    data += "\"}";
    WsSend((char*)data.c_str(), "hack"); // log attempts
    lastIP = ip;
    return;
  }

  lastIP = ip;

  for ( uint8_t i = 0; i < request->params(); i++ )
  {
    AsyncWebParameter* p = request->getParam(i);
    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);

    if(p->name() == "key");
    else if(p->name() == "rest")
      display.init();
    else if(p->name() == "ssid")
      s.toCharArray(ee.szSSID, sizeof(ee.szSSID));
    else if(p->name() == "pass")
      wifi.setPass(s.c_str());
    else if(p->name() == "fc")
    {
      ee.bNotLocalFcst = s.toInt() ? true:false;
      display.m_bUpdateFcst = true;
    }
    else
    {
      hvac.setVar(p->name(), s.toInt() );
    }
  }
}

// Pushed data
String dataJson()
{
  return hvac.getPushData();
}

void historyDump(bool bStart)
{
  static bool bSending;
  static int entryIdx;
  static int32_t tb;
  static int tempMin;
  static int lMin;
  static int hMin;
  static int rhMin;
  static int otMin;

  if(bStart) bSending = true;
  if(bSending == false)
    return;

  gPoint gpt;

  String out;
#define CHUNK_SIZE 800
  out.reserve(CHUNK_SIZE + 100);

  if(bStart)
  {
    entryIdx = 0;
    if( display.getGrapthPoints(&gpt, 0) == false)
    {
      bSending = false;
      return;
    }
    out = String("ref;{\"tb\":");
    tb = gpt.time; // latest entry
    tempMin = display.minPointVal(0);
    lMin = display.minPointVal(1);
    hMin = display.minPointVal(2);
    rhMin = display.minPointVal(3);
    otMin = display.minPointVal(4);
  
    out += tb;
    out += ",\"th\":"; out += gpt.h - gpt.l; // threshold
    out += ",\"tm\":"; out += tempMin; // temp min
    out += ",\"lm\":"; out += lMin; // threshold low min
    out += ",\"rm\":"; out += rhMin; // rh min
    out += ",\"om\":"; out += otMin; // ot min
    out += "}";
    ws.text(WsClientID, out);
  }

  out = String("data;{\"d\":[");

  bool bC = false;
  for(; entryIdx < GPTS - 1 && out.length() < CHUNK_SIZE && display.getGrapthPoints(&gpt, entryIdx); entryIdx++)
  {
    if(bC) out += ",";
    bC = true;
    out += "[";         // [seconds/10, temp, rh, lowThresh, state, outTemp],
    out += (tb - (int32_t)gpt.time) / 10;
    out += ",";
    out += gpt.temp - tempMin;
    out += ",";
    out += gpt.bits.b.rh - rhMin;
    out += ",";
    out += gpt.l - lMin;
    out += ",";
    out += gpt.bits.u & 7;
    out += ",";
    out += gpt.ot - otMin;
    out += "]";
  }
  if(bC) // don't send blank
  {
    out += "]}";
    ws.text(WsClientID, out);
  }
  else
    bSending = false;
  if(bSending == false)
    ws.text(WsClientID, "draw;{}"); // tell page to draw after all is sent
}


void appendDump(int startTime)
{
  String out;

  out.reserve(CHUNK_SIZE + 100);
  out = String("data2;{\"d\":[");
  bool bC = false;
  gPoint gpt;

  for(int entryIdx = 0; entryIdx < GPTS - 1 && out.length() < CHUNK_SIZE && display.getGrapthPoints(&gpt, entryIdx) && gpt.time > startTime; entryIdx++)
  {
    if(bC) out += ",";
    bC = true;
    out += "[";         // [seconds, temp, rh, lowThresh, state, outTemp],
    out += gpt.time;
    out += ",";
    out += gpt.temp;
    out += ",";
    out += gpt.bits.b.rh;
    out += ",";
    out += gpt.l;
    out += ",";
    out += gpt.bits.u & 7;
    out += ",";
    out += gpt.ot;
    out += "]";
  }
  if(bC) // don't send blank
  {
    out += "]}";
    ws.text(WsClientID, out);
  }
}

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  switch(iEvent)
  {
    case 0: // state
      switch(iName)
      {
        case 0: // temp
          if(hvac.m_bRemoteStream)
            hvac.m_inTemp = (int)(atof(psValue)*10);
          break;
        case 1: // rh
          if(hvac.m_bRemoteStream)
            hvac.m_rh = (int)(atof(psValue)*10);
          break;
        case 2: // tempi
          if(hvac.m_bRemoteStream)
            hvac.m_inTemp = iValue;
          break;
        case 3: // rhi
          if(hvac.m_bRemoteStream)
            hvac.m_rh = iValue;
          break;
        case 4: // rmt
          if(hvac.m_bRemoteStream != (iValue ? true:false) )
          {
            WsRemoteID = WsClientID;
            hvac.m_bRemoteStream = (iValue ? true:false);
            hvac.m_bLocalTempDisplay = !hvac.m_bRemoteStream; // switch to showing local/remote color

            if(hvac.m_bRemoteStream)
              hvac.m_notif = Note_RemoteOn;
            else
              hvac.m_notif = Note_RemoteOff;
          }
          break;
      }
      break;
    case 1: // cmd
      if(iName == 0) // 0 = key
      {
        if(!strcmp(psValue, ee.password)) // first item must be key
          bKeyGood = true;
      }
      else if(iName == 1) // 1 = data
      {
        if(iValue) appendDump(iValue);
        else historyDump(true);
      }
      else if(iName == 2) // 2 = summary
      {
        String out = String("sum;{\"mon\":[");

        for(int i = 0; i < 12; i++)
        {
          if(i) out += ",";
          out += "[";
          out += ee.fCostE[i];
          out += ",";
          out += ee.fCostG[i];
          out += "]";
        }
        out += "],\"day\":[";
        for(int i = 0; i < 31; i++)
        {
          if(i) out += ",";
          out += "[";
          out += ee.fCostDay[i][0];
          out += ",";
          out += ee.fCostDay[i][1];
          out += "]";
        }
        out += "]}";
        ws.text(WsClientID, out);
      }
      else if(iName == 3) // 3 = bin
      {
        switch(iValue)
        {
          case 1: // forecast data
          {
            uint8_t pl[sizeof(display.m_fcData)+1];
            memcpy(pl+1, (uint8_t*)display.m_fcData, sizeof(display.m_fcData));
            pl[0] = 1;
            ws.binary(WsClientID, pl, sizeof(pl));
          }
          break;
        }
      }
      else // 4+
      {
        if(bKeyGood)
          hvac.setVar(cmdList[iName+1], iValue); // 5 is "fanmode"
      }
      break;
    case 2: // alert
      display.Note(psValue);
      break;
  }
}

// local server forecast retrieval 
String sfcBuffer;

void fc_onConnect(AsyncClient* client)
{
  (void)client;

  String s = "GET /Forecast.log HTTP/1.1\n"
    "Host: ";
  s += ipFcServer.toString();
  s += "\n"
    "Connection: close\n"
    "Accept: */*\n\n";

  fc_client.add(s.c_str(), s.length());
  sfcBuffer = String("");
  sfcBuffer.reserve(1200);  // about 1010 bytes
}

// build file in chunks
void fc_onData(AsyncClient* client, char* data, size_t len)
{
  sfcBuffer += data;
}

// read data as comma delimited 'time,temp,rh' per line
void fc_onDisconnect(AsyncClient* client)
{
  (void)client;

  int fcIdx;
  const char *p = sfcBuffer.c_str();
  if(p == NULL)
    return;

  for(fcIdx = 1; fcIdx < FC_CNT-1 && *p;)
  {
    uint32_t tm = atoi(p);
    if(tm > 15336576) // skip the headers
    {
      display.m_fcData[fcIdx].tm = tm;
      while(*p && *p != ',') p ++;
      if(*p == ',') p ++;
      else break;
      display.m_fcData[fcIdx].temp = atoi(p);
      fcIdx++;
    }
    while(*p && *p != '\r' && *p != '\n') p ++;
    while(*p == '\r' || *p == '\n') p ++;
  }
  sfcBuffer = String("");
  display.m_fcData[fcIdx].tm = 0;
  display.m_bUpdateFcstDone = true;

  if(display.m_fcData[0].tm == 0) // initial read
  {
    display.m_fcData[0].temp = display.m_fcData[1].temp;
    display.m_fcData[0].tm = display.m_fcData[1].tm;
  }
  if(display.m_fcData[0].tm)
    hvac.enable();
}

void fc_onTimeout(AsyncClient* client, uint32_t time)
{
  (void)client;
  WsSend("Error getting local server forecast", "print");
}

//---
const XML_tag_t Xtags[] =
{
  {"creation-date", NULL, NULL, 1},
  {"time-layout", "time-coordinate", "local", FC_CNT * 2 * 3}, // only 3rd value, start/end for each
  {"temperature", "type", "hourly", FC_CNT * 3},
  // "temperature", "type", "dewpoint"
  // "temperature", "type", "wind chill"
  // wind-speed type="sustained"
  // cloud-amount type="total"
  // probability-of-precipitation type="floating"
  // humidity type="relative"
  // direction type="wind"
  // hourly-qpf type="floating"
  // weather layout=
  {NULL}
};

void xml_callback(int item, int idx, char *p, char *pTag)
{
  static tmElements_t tm;
  static int cnt;

  switch(item)
  {
    case -1: // done
      xmlState = idx;
      break;
    case 0: // the current local time
      break;
    case 1:            // valid time
      if(idx == 0)     // first item isn't really data
      {
        cnt = 0;
        break;
      }
//      if(pTag[0] != 's') // start only
//        break;

      if((idx % 6) != 1) // just skip all but <start-time> every 3 hours
        break;

      tm.Year = CalendarYrToTm(atoi(p)); // 2014-mm-ddThh:00:00-tz:00
      tm.Month = atoi(p+5);
      tm.Day = atoi(p+8);
      tm.Hour = atoi(p+11);
      tm.Minute = atoi(p+14);
      tm.Second = atoi(p+17);

      display.m_fcData[cnt].tm = makeTime(tm);
      cnt++;
      display.m_fcData[cnt].tm = 0; // end of data
      break;
    case 2:                  // temperature
      if(idx == 0)
        cnt = 0;
      if((idx % 3) != 0) // skip every 3 hours
        break;

      display.m_fcData[cnt++].temp = atoi(p);
      break;
  }
}

XMLReader xml(xml_callback, Xtags);

void GetForecast()
{
  display.m_fcData[0].temp = display.m_fcData[1].temp; // keep a copy of first 3hour data
  display.m_fcData[0].tm = display.m_fcData[1].tm;

  // Full 7 day hourly
  //  Go here first:  http://www.weather.gov
  // Enter City or Zip
  // Click on "Hourly Weather Forecast"
  // Scroll down, Click on "Tabular Forecast"
  // Then click [XML] and copy that URL to the line below
  // Then send "?key=<your key>&fc=1" to the thermostat to enable calls to this

  String path = "/MapClick.php?lat=&lon=&FcstType=digitalDWML";

  if(!xml.begin("forecast.weather.gov", 80, path))
    WsSend("Forecast failed", "alert");
}
