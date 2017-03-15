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
AsyncEventSource events("/events"); // event source (Server-Sent events)
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
extern HVAC hvac;
extern Display display;
WiFiManager wifi;  // AP page:  192.168.4.1
AsyncClient fc_client;

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonParse remoteParse(remoteCallback);
int chartFiller(uint8_t *buffer, int maxLen, int index);
void dataPage(AsyncWebServerRequest *request);
void fcPage(AsyncWebServerRequest *request);

int xmlState;
void GetForecast(void);

int nWrongPass;
uint32_t lastIP;
bool bKeyGood;
int WsClientID;
int WsRemoteID;

// Handle event stream
void onEvents(AsyncEventSourceClient *client)
{
  static bool rebooted = true;
  if(rebooted)
  {
    rebooted = false;
    events.send("Restarted", "alert");
  }
  events.send(dataJson().c_str(), "state");
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{  //Handle WebSocket event
  static bool rebooted = true;
  switch(type)
  {
    case WS_EVT_CONNECT:      //client connected
      if(rebooted)
      {
        rebooted = false;
        client->printf("alert;Restarted");
      }
      client->printf("settings;%s", hvac.settingsJson().c_str()); // update everything on start
      client->printf("state;%s", dataJson().c_str());
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

  // attach AsyncEventSource
  events.onConnect(onEvents);
  server.addHandler(&events);

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
    request->send_P(200, "text/html", chart);
#endif
  });
  server.on ( "/data", HTTP_GET, dataPage);  // history for chart
  server.on ( "/forecast", HTTP_GET, fcPage); // forecast data for remote unit

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(404);
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

// Send the array formated chart data and any modified variables
void dataPage(AsyncWebServerRequest *request)
{
  int32_t tb = 0; // time subtracted from entries (saves 5 bytes each)

  AsyncResponseStream *response = request->beginResponseStream("text/javascript");

  for(int entryIdx = 0; entryIdx < GPTS - 12; entryIdx++)
  {
    gPoint gpt;
    if( display.getGrapthPoints(&gpt, entryIdx) == false)
      break;
   
    if(gpt.time == 0)
      continue; // some glitch?

    String out = "";
    if(tb == 0) // first entry found
    {
      tb = gpt.time - (60*60*26);  // subtract 26 hours form latest entry
      out += "tb=";      // first data opening statements
      out += tb;
      out += "\ncost=";
      out +=  String(hvac.m_fCostE + hvac.m_fCostG, 2);
      out += "\ndata=[\n";
    }
    out += "[";         // [seconds/10, temp, rh, high, low, state],
    out += (gpt.time - tb)/10;
    out += ",";
    out += gpt.temp;
    out += ",";
    out += gpt.bits.b.rh;
    out += ",";
    out += gpt.h;
    out += ",";
    out += gpt.l;
    out += ",";
    out += gpt.bits.u & 7;
    out += "],";
    response->print(out);
  }
  response->print("]\n");
  request->send ( response );
}

// Send the comma delimited forecast data
void fcPage(AsyncWebServerRequest *request)
{
  AsyncResponseStream *response = request->beginResponseStream("text/javascript");

  for(int i = 0; i < FC_CNT; i++)
  {
    if(display.m_fcData[i].tm == 0)
      break;

    String out = "";
    out += display.m_fcData[i].tm;
    out += ",";
    out += display.m_fcData[i].temp;
    out += "\r\n";
    response->print(out);
  }
  request->send( response );
}

void handleServer()
{
  MDNS.update();
#ifdef OTA_ENABLE
// Handle OTA server.
  ArduinoOTA.handle();
  yield();
#endif
}

void WsSend(char *txt, const char *type)
{
  events.send(txt, type);
  ws.printfAll("%s;%s", type, txt);
}

void secondsServer() // called once per second
{
  if(nWrongPass)
    nWrongPass--;

  static int n = 10;
  if(hvac.stateChange() || hvac.tempChange())
  {
    WsSend((char*)dataJson().c_str(), "state" );
    n = 10;
  }
  else if(--n == 0)
  {
    events.send("", "" ); // keepalive
    n = 10;
  }

  String s = hvac.settingsJsonMod(); // returns "{}" if nothing has changed
  if(s.length() > 2)
    ws.printfAll("settings;%s", s.c_str()); // update anything changed

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
          WsSend("Forecast success", "print");
          display.screen(true);
          display.drawForecast(true);
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
      else
      {
        if(bKeyGood)
          hvac.setVar(cmdList[iName+1], iValue); // 1 is "fanmode"
      }
      break;
    case 2: // alert
      display.Note(psValue);
      break;
  }
}

// local server forecast retrieval 
int fcIdx;

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
  fcIdx = 1; // 0 is reserved
}

// read data as comma delimited 'time,temp,rh' per line
void fc_onData(AsyncClient* client, char* data, size_t len)
{
  while(fcIdx < FC_CNT-1 && *data)
  {
    uint32_t tm = atoi(data);
    if(tm) // skip the headers
    {
      display.m_fcData[fcIdx].tm = tm;
      while(*data && *data != ',') data ++;
      if(*data == ',') data ++;
      else return;
      display.m_fcData[fcIdx].temp = atoi(data);
      fcIdx++;
    }
    while(*data && *data != '\r' && *data != '\n') data ++;
    while(*data == '\r' || *data == '\n') data ++;
  }
  display.m_fcData[fcIdx].tm = 0;
  display.m_bUpdateFcstDone = true;
  hvac.enable();
}

void fc_onDisconnect(AsyncClient* client)
{
  (void)client;
}

void fc_onTimeout(AsyncClient* client, uint32_t time)
{
  (void)client;
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
