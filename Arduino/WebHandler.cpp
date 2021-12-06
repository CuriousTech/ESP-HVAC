// Do all the web stuff here

#define OTA_ENABLE  //uncomment to enable Arduino IDE Over The Air update code
#define USE_SPIFFS  // saves 11K of program space, loses 800 bytes dynamic (ESP8266 64K SPIFFS)

#ifdef ESP32
#include <ESPmDNS.h>
#else
#include <ESP8266mDNS.h>
#endif

#ifdef OTA_ENABLE
#include <ArduinoOTA.h>
#endif
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include "WiFiManager.h"
#include "WebHandler.h"
#include "HVAC.h"
#include <JsonParse.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonParse

#ifdef REMOTE
#include <JsonClient.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonClient
#include <WebSocketsClient.h> // https://github.com/Links2004/arduinoWebSockets
#endif

#include "display.h"
#include "eeMem.h"
#ifdef USE_SPIFFS
#include <FS.h>
 #ifdef ESP32
 #include <SPIFFS.h>
 #endif
#endif
#include "pages.h"
#include <XMLReader.h>
#include "jsonstring.h"
#include "forecast.h"
//-----------------
int serverPort = 80;

#ifdef REMOTE
const char *hostName = "HVACRemote";
WebSocketsClient wsc;
bool bWscConnected;
#else
const char *hostName = "HVAC";
IPAddress ipFcServer(192,168,31,100);    // local forecast server and port
int nFcPort = 80;
Forecast localFC;
#endif

//-----------------
AsyncWebServer server( serverPort );
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
extern HVAC hvac;
extern Display display;
WiFiManager wifi;  // AP page:  192.168.4.1

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonParse remoteParse(remoteCallback);

#ifdef REMOTE
void startListener(void);
#else
int xmlState;
void GetForecast(void);
#endif

int nWrongPass;
bool bKeyGood;
int WsClientID;
int WsRemoteID;
IPAddress lastIP;
IPAddress WsClientIP;

#ifdef REMOTE
const char pageR_T[] = R"rawliteral(
<!DOCTYPE html>
<html>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<head>
<title>ESP-HVAC Remote</title>
</head>
<body">
<strong><em>CuriousTech HVAC Remote</em></strong><br>
)rawliteral";

const char pageR_B[] = R"rawliteral(
<br><small>&copy 2016 CuriousTech.net</small>
</body>
</html>
)rawliteral";

// values sent at an interval of 30 seconds unless they change sooner
const char *jsonList1[] = { "state", "r", "fr", "s", "it", "rh", "tt", "fm", "ot", "ol", "oh", "ct", "ft", "rt", "h", "lt", "lh", "rmt", NULL };
const char *jsonList2[] = { "settings", "m", "am", "hm", "fm", "ot", "ht", "c0", "c1", "h0", "h1", "im", "cn", "cx", "ct", "fd", "ov", "rhm", "rh0", "rh1", "tu", NULL };
const char *cmdList[] = { "cmd",
  "key",
  "data",
  "sum",
  NULL};
  
#else
const char *jsonList1[] = { "state",  "rmttemp", "rmtrh", NULL };
extern const char *cmdList[];
#endif

const char *jsonList3[] = { "alert", NULL };

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
      client->text( hvac.settingsJson() );
      client->text( dataJson() );
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
    case WS_EVT_ERROR:    //error was received from the other end
#ifndef REMOTE
      if(hvac.m_bRemoteStream && client->id() == WsRemoteID) // stop remote
      {
        hvac.m_bRemoteStream = false;
        hvac.m_notif = Note_RemoteOff;
      }
#endif
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
          WsClientIP = client->remoteIP();
          remoteParse.process(pCmd, pData);
        }
      }
      break;
  }
}

void startServer()
{
  WiFi.hostname(hostName);
  wifi.autoConnect(hostName, ee.password); // Tries configured AP, then starts softAP mode for config
  hvac.m_notif = Note_Connecting;

#ifdef USE_SPIFFS
  SPIFFS.begin();
//  server.addHandler(new SPIFFSEditor("admin", ee.password));
#endif

  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on ( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    if(wifi.state() == ws_config)
      request->send( 200, "text/html", wifi.page() );
#ifdef REMOTE
    else
    {
      String s = pageR_T;
      s += "RemoteStream "; s += hvac.m_bRemoteStream; s += "<br>";
      s += "WsConnected "; s += bWscConnected; s += "<br>";
      IPAddress ip(ee.hostIp);
      s += "HVAC IP "; s += ip.toString(); s += "<br>";

      s += pageR_B;
      request->send( 200, "text/html", s );
    }
#endif
  });
#ifndef REMOTE
  server.on ( "/iot", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);
#ifdef USE_SPIFFS
    request->send(SPIFFS, "/index.html");
#else
    request->send_P(200, "text/html", page_index);
#endif
  });
#endif
  server.on ( "/s", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);
    request->send ( 200, "text/html", "OK" );
  });

  server.on ( "/json", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    request->send ( 200, "text/json",  hvac.settingsJson());
  });
#ifndef REMOTE
  server.on ( "/settings", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);
#ifdef USE_SPIFFS
    request->send(SPIFFS, "/settings.html");
#else
    request->send_P(200, "text/html", page_settings);
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
  server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request){
#ifdef USE_SPIFFS
    request->send(SPIFFS, "/styles.css");
#else
    request->send_P(200, "text/html", page_styles);
#endif
  });
#endif // !REMOTE
  server.on ( "/wifi", HTTP_GET|HTTP_POST, [](AsyncWebServerRequest *request)
  {
    parseParams(request);
    jsonString js;
    js.Var("time", (long)(now() - ((ee.tz + hvac.m_DST) * 3600)) );
    js.Var("ppkw", ee.ppkwh );
    request->send(200, "text/plain", js.Close());
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
//#ifdef USE_SPIFFS
//    request->send(SPIFFS, "/favicon.ico");
//#else
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon, sizeof(favicon));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
//#endif
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

  remoteParse.addList(jsonList1);
#ifdef REMOTE
  remoteParse.addList(jsonList2);
#endif
  remoteParse.addList(cmdList);
  remoteParse.addList(jsonList3);

#ifdef OTA_ENABLE
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    hvac.disable();
    hvac.dayTotals(day() - 1); // save for reload
    ee.filterMinutes = hvac.m_filterMinutes;
    if(eemem.check())
      eemem.update();
    SPIFFS.end();
  });
#endif
}

#ifdef REMOTE
void findHVAC() // find the HVAC on iot domain
{
  // Find HVAC
  int cnt = MDNS.queryService("iot", "tcp");
  for(int i = 0; i < cnt; ++i)
  {
    char szName[38];
    MDNS.hostname(i).toCharArray(szName, sizeof(szName));
    strtok(szName, "."); // remove .local

    if(!strcmp(szName, "HVAC"))
    {
      ee.hostIp[0] = MDNS.IP(i)[0]; // update IP
      ee.hostIp[1] = MDNS.IP(i)[1];
      ee.hostIp[2] = MDNS.IP(i)[2];
      ee.hostIp[3] = MDNS.IP(i)[3];
      break;
    }
  }
}
#endif

bool handleServer()
{
  bool bConn = false;
#ifdef ESP8266
  MDNS.update();
#endif

  wifi.service();
  if(wifi.connectNew())
  {
//    Serial.println("WiFi connected");
//    Serial.println("IP address: ");
//    Serial.println(WiFi.localIP());
    MDNS.begin( hostName );
    // Add service to MDNS-SD
    MDNS.addService("iot", "tcp", serverPort);
    hvac.m_notif = Note_Connected;
    bConn = true;
#ifdef REMOTE
    findHVAC();
#endif
  }

#ifndef REMOTE
  static int n;
  if(++n >= 10)
  {
    historyDump(false);
    n = 0;
  }
#endif
#ifdef OTA_ENABLE
// Handle OTA server.
  ArduinoOTA.handle();
#endif
  return bConn;
}

void WsSend(String s) // mostly for debug
{
  ws.textAll(s);
}

#ifdef REMOTE
void WscSend(String s) // remote WebSocket
{
  wsc.sendTXT(s);
}
#endif

void secondsServer() // called once per second
{
#ifdef REMOTE
  if(hvac.tempChange())
  {
    ws.textAll(dataJson()); // not used?
  }

  static uint8_t start = 4; // give it time to settle before initial connect
  if(start && wifi.state() == ws_connected)
    if(--start == 0)
        startListener();

  if(display.m_bUpdateFcst && bWscConnected)
  {
     display.m_bUpdateFcst = false;
     WscSend("cmd;{\"bin\":1}"); // forcast data
  }
#else
  if(wifi.state() != ws_connected)
    return;

  ws.cleanupClients();
  String s = hvac.settingsJsonMod(); // returns "{}" if nothing has changed
  if(s.length() > 2)
    ws.textAll(s); // update anything changed

  if(hvac.stateChange() || hvac.tempChange())
    ws.textAll( dataJson() );

  if(nWrongPass)
    nWrongPass--;

  if(display.m_fc.loadDate + (3600*6) < now() && display.m_bUpdateFcstIdle) // > 6 hours old
    display.m_bUpdateFcst = true;

  static uint8_t nUpdateDelay = 5;
  if(nUpdateDelay)
    nUpdateDelay--;
  if(display.m_bUpdateFcst && display.m_bUpdateFcstIdle)
  {
    if(nUpdateDelay == 0)
    {
      display.m_bUpdateFcst = false;
      display.m_bUpdateFcstIdle = false;
      nUpdateDelay = 60; // delay retries by 1 minute
      if(ee.b.bNotLocalFcst)
        GetForecast();
      else
        localFC.start(ipFcServer, nFcPort, &display.m_fc, ee.b.bCelcius);    // get preformatted data from local server
    }
  }
  if(localFC.checkStatus())
  {
    hvac.enable();
    display.m_fc.loadDate = now();
    display.m_bUpdateFcstIdle = true;
    display.m_bFcstUpdated = true;
  }

  if(xmlState)
  {
      switch(xmlState)
      {
        case XML_COMPLETED:
        case XML_DONE:
          hvac.enable();
          display.m_bUpdateFcstIdle = true;
          display.m_bFcstUpdated = true;
          display.m_fc.loadDate = now();
          break;
        case XML_TIMEOUT:
          hvac.disable();
          hvac.m_notif = Note_Forecast;
          display.m_bUpdateFcstIdle = true;
          break;
      }
      xmlState = 0;
  }
#endif // !REMOTE
}

void parseParams(AsyncWebServerRequest *request)
{
  if(request->params() == 0)
    return;

#ifdef REMOTE
  int val;

  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);
    String s = request->urlDecode(p->value());
    int val = s.toInt();
 
    switch( p->name().charAt(0) )
    {
      case 'T': // temp offset
          ee.adj = val;
          break;
      case 'f': // get forecast
          display.m_bUpdateFcst = true;
          break;
      case 'H': // host  (from browser type: hTtp://thisip/?H=hostip)
          {
            IPAddress ip;
            ip.fromString(s);
            ee.hostIp[0] = ip[0];
            ee.hostIp[1] = ip[1];
            ee.hostIp[2] = ip[2];
            ee.hostIp[3] = ip[3];
            startListener(); // reset the URI
          }
          break;
      case 'R': // remote
          if(ee.b.bLock) break;
          if(val)
          {
            if(hvac.m_bRemoteStream == false)
              hvac.enableRemote(); // enable
          }
          else
          {
            if(hvac.m_bRemoteStream == true)
              hvac.enableRemote(); // disable
          }
          break;
      case 'Z': // Timezone
          ee.tz = val;
          break;
      case 's': // SSID
          s.toCharArray(ee.szSSID, sizeof(ee.szSSID));
          break;
      case 'p': // AP password
          wifi.setPass(s.c_str());
          break;
    }
  }
#else
  bool bPassGood;

  for( uint8_t i = 0; i < request->params(); i++ ) // password may be at end
  {
    AsyncWebParameter* p = request->getParam(i);
    String s = request->urlDecode(p->value());

    if(p->name() == "key")
      bPassGood = s.equals(String(ee.password));
  }

  IPAddress ip = request->client()->remoteIP();

  if( (ip[0] != 192 && ip[1] != 168 && !bPassGood) || nWrongPass )
  {
    if(nWrongPass == 0)
    {
      nWrongPass = 10;
      jsonString js("hack");
      js.Var("ip", ip.toString() );
      ws.textAll(js.Close());
    }
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 10;

    lastIP = ip;
    return;
  }

  lastIP = ip;

  for ( uint8_t i = 0; i < request->params(); i++ )
  {
    AsyncWebParameter* p = request->getParam(i);
    String s = request->urlDecode(p->value());

    if(p->name() == "key");
    else if(p->name() == "ssid")
      s.toCharArray(ee.szSSID, sizeof(ee.szSSID));
    else if(p->name() == "pass")
      wifi.setPass(s.c_str());
    else if(p->name() == "restart")
    {
#ifdef ESP32
      ESP.restart();
#else
      ESP.reset();
#endif
    }
    else if(p->name() == "fc")
    {
      ee.b.bNotLocalFcst = s.toInt() ? true:false;
      display.m_bUpdateFcst = true;
    }
    else
    {
      hvac.setVar(p->name(), s.toInt(), (char *)s.c_str(), ip );
    }
  }
#endif // !REMOTE
}

// Pushed data
String dataJson()
{
  return hvac.getPushData();
}

void historyDump(bool bStart)
{
#ifndef REMOTE
  static bool bSending;
  static int entryIdx;
  static int tempMin;
  static int lMin;
  static int hMin;
  static int rhMin;
  static int otMin;

  if(bStart)
    bSending = true;
  if(bSending == false)
    return;

  gPoint gpt;

  if(bStart)
  {
    entryIdx = 0;
    if( display.getGrapthPoints(&gpt, 0) == false)
    {
      bSending = false;
      return;
    }

    jsonString js("ref");
    tempMin = display.minPointVal(0);
    lMin = display.minPointVal(1);
    hMin = display.minPointVal(2);
    rhMin = display.minPointVal(3);
    otMin = display.minPointVal(4);
  
    js.Var("tb", display.m_lastPDate);
    js.Var("th", ee.cycleThresh[ (hvac.m_modeShadow == Mode_Heat) ? 1:0] ); // threshold
    js.Var("tm", tempMin); // temp min
    js.Var("lm", lMin); // threshold low min
    js.Var("rm", rhMin); // rh min
    js.Var("om", otMin); // ot min
    ws.text(WsClientID, js.Close());
  }

  String out;
#define CHUNK_SIZE 800
  out.reserve(CHUNK_SIZE + 100);

  out = "data;{\"d\":[";

  bool bC = false;

  for(; entryIdx < GPTS - 1 && out.length() < CHUNK_SIZE && display.getGrapthPoints(&gpt, entryIdx); entryIdx++)
  {
    int len = out.length();
    if(bC) out += ",";
    bC = true;
    out += "[";         // [seconds, temp, rh, lowThresh, state, outTemp],
    out += gpt.bits.tmdiff;
    out += ",";
    out += gpt.t.inTemp - tempMin;
    out += ",";
    out += gpt.bits.rh - rhMin;
    out += ",";
    out += gpt.t.target - lMin;
    out += ",";
    out += gpt.bits.u & 7; // state + fan
    out += ",";
    out += gpt.t.outTemp - otMin;
    if(gpt.t2.sens0 || gpt.t2.sens1)
    {
      out += ",";
      out += gpt.t2.localTemp - tempMin;
      out += ",";
      out += gpt.t2.sens0 - tempMin;
      if(gpt.t2.sens1)
      {
        out += ",";
        out += gpt.t2.sens1 - tempMin;
      }
    }
    out += "]";
    if( out.length() == len) // memory full
      break;
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
#endif
}

void appendDump(uint32_t startTime)
{
#ifndef REMOTE
  String out = "data2;{\"d\":[";

  uint32_t tb = display.m_lastPDate;
  bool bC = false;
  gPoint gpt;

  for(int entryIdx = 0; entryIdx < GPTS - 1 && out.length() < CHUNK_SIZE && display.getGrapthPoints(&gpt, entryIdx) && tb > startTime; entryIdx++)
  {
    if(bC) out += ",";
    bC = true;
    out += "[";         // [seconds, temp, rh, lowThresh, state, outTemp],
    out += tb;
    tb -= gpt.bits.tmdiff;
    out += ",";
    out += gpt.t.inTemp;
    out += ",";
    out += gpt.bits.rh;
    out += ",";
    out += gpt.t.target;
    out += ",";
    out += gpt.bits.u & 7;
    out += ",";
    out += gpt.t.outTemp;
    if(gpt.t2.sens0 || gpt.t2.sens1)
    {
      out += ",";
      out += gpt.t2.localTemp;
      out += ",";
      out += gpt.t2.sens0;
      if(gpt.t2.sens1)
      {
        out += ",";
        out += gpt.t2.sens1;
      }
    }
    out += "]";
  }
  if(bC) // don't send blank
  {
    out += "]}";
    ws.text(WsClientID, out);
  }
#endif
}

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
#ifdef REMOTE
  switch(iEvent)
  {
    case 0: // state
      hvac.updateVar(iName, iValue);
      break;
    case 1: // settings
      hvac.setSettings(iName, iValue);
      break;
    case 2: // cmdList
      switch(iName)
      {
        case 0: // key
          break;
        case 1: // data
          break;
        case 2: // sum
          break;
      }
      break;
    case 3: // alert
      display.Note(psValue);
      break;
  }
#else
  switch(iEvent)
  {
    case 0: // state
      switch(iName)
      {
        case 0: // temp
          hvac.setVar("rmttemp", iValue, psValue, WsClientIP);
          break;
        case 1: // rh
          hvac.setVar("rmtrh", iValue, psValue, WsClientIP);
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
          out += ee.iSecsMon[i][0];
          out += ",";
          out += ee.iSecsMon[i][1];
          out += ",";
          out += ee.iSecsMon[i][2];
          out += "]";
        }
        out += "],\"day\":[";
        for(int i = 0; i < 31; i++)
        {
          if(i) out += ",";
          out += "[";
          out += ee.iSecsDay[i][0];
          out += ",";
          out += ee.iSecsDay[i][1];
          out += ",";
          out += ee.iSecsDay[i][2];
          out += "]";
        }
        out += "],";
        out += "\"fcDate\":";
        out += display.m_fc.Date;
        out += ",\"fcFreq\":";
        out += display.m_fc.Freq;
        out += ",\"fc\":[";
        for(int i = 0; display.m_fc.Data[i] != -127 && i < FC_CNT; i++)
        {
          if(i) out += ",";
          out += display.m_fc.Data[i];
        }        
        out += "]}";
        ws.text(WsClientID, out);
      }
      else if(iName == 3) // 3 = bin
      {
        WsRemoteID = WsClientID; // Only remote uses binary
        switch(iValue)
        {
          case 1: // forecast data
            ws.binary(WsClientID, (uint8_t*)&display.m_fc, sizeof(display.m_fc));
            break;
        }
      }
      else // 4+
      {
        if(bKeyGood)
          hvac.setVar(cmdList[iName+1], iValue, psValue, WsClientIP); // 5 is "fanmode"
      }
      break;
    case 2: // alert
      display.Note(psValue);
      break;
  }
#endif
}

#ifdef REMOTE
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length)
{
  switch(type)
  {
    case WStype_DISCONNECTED:
      bWscConnected = false;
      hvac.m_notif = Note_Network;
      break;
    case WStype_CONNECTED:
      bWscConnected = true;
      if(hvac.m_notif == Note_Network) // remove net disconnect error
        hvac.m_notif = Note_None;
      break;
    case WStype_TEXT:
      {
        char *pCmd = strtok((char *)payload, ";");
        char *pData = strtok(NULL, "");
        if(pCmd == NULL || pData == NULL) break;
        remoteParse.process(pCmd, pData);
      }
      break;
    case WStype_BIN:
      if(length == sizeof(forecastData) )
      {
        memcpy((void*)&display.m_fc, payload, length);
        display.m_bUpdateFcstIdle = true;
        display.m_bFcstUpdated = true;
      }
      break;
  }
}

void startListener()
{
  wsc.onEvent(webSocketEvent);
  IPAddress ip(ee.hostIp);
  wsc.begin(ip, ee.hostPort, "/ws");
}
#endif

//---

#ifndef REMOTE
int shiftFc(uint32_t newTm)
{
  if(display.m_fc.Date == 0) // not filled in yet
    return 0;

  uint32_t tm2 = display.m_fc.Date;
  int fcIdx;
  for(fcIdx = 0; fcIdx < FC_CNT-4 && display.m_fc.Data[fcIdx] != -127; fcIdx++)
  {
    if(tm2 >= newTm)
      break;
    tm2 += display.m_fc.Freq;
  }
  if(fcIdx > (FC_CNT - 56)) // not enough room left
  {
    int n = fcIdx - (FC_CNT - 56);
    uint8_t *p = (uint8_t*)display.m_fc.Data;
    memcpy(p, p + n, FC_CNT - n); // make room
    display.m_fc.Date += display.m_fc.Freq * n;
    fcIdx -= n;
  }
  return fcIdx;
}

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
  static int startIdx;
  static int cnt;
  static uint32_t lastTm;
  switch(item)
  {
    case -1: // done
      xmlState = idx;
      break;
    case 0: // the current local time
      break;
    case 1:            // valid time
      if(idx == 0)     // first item isn't really data
        break;

      if((idx % 6) != 1) // just skip all but <start-time> every 3 hours
        break;

      tm.Year = CalendarYrToTm( atoi(p)); // 2014-mm-ddThh:00:00-tz:00
      tm.Month = atoi(p+5);
      tm.Day  = atoi(p+8);
      tm.Hour = atoi(p+11);
      tm.Minute = atoi(p+14);
      tm.Second = atoi(p+17);

      if(idx == 1) // first entry
      {
        if(display.m_fc.Date == 0)
          display.m_fc.Date = makeTime(tm);
        startIdx = cnt = shiftFc( makeTime(tm) );
      }
      else if(idx == 2) // 2nd entry
      {
        display.m_fc.Freq = makeTime(tm) - lastTm;
      }
      lastTm = makeTime(tm);
      cnt++;
      display.m_fc.Data[cnt] = -127; // end of data
      break;
    case 2:                  // temperature
      if(idx == 0)
        cnt = startIdx;
      if((idx % 3) != 0) // skip every 3 hours
        break;

      display.m_fc.Data[cnt++] = atoi(p);
      break;
  }
}

XMLReader xml(xml_callback, Xtags);

void GetForecast()
{
  // Full 7 day hourly
  //  Go here first:  http://www.weather.gov
  // Enter City or Zip
  // Click on "Hourly Weather Forecast"
  // Scroll down, Click on "Tabular Forecast"
  // Then click [XML] and copy that URL to the line below
  // Then send "?key=<your key>&fc=1" to the thermostat to enable calls to this

  String path = "/MapClick.php?lat=&lon=&FcstType=digitalDWML";

  if(!xml.begin("forecast.weather.gov", 80, path))
    WsSend("alert;Forecast URL invalid");
}

#endif
