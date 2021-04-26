// Do all the web stuff here (Remote unit)

//uncomment to enable Arduino IDE Over The Air update code
#define OTA_ENABLE

//#define USE_SPIFFS // saves 11K of program space, loses 800 bytes dynamic (at 64K)

#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#ifdef OTA_ENABLE
#include <FS.h>
#include <ArduinoOTA.h>
#endif
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include "WebHandler.h"
#include "HVAC.h"
#include <JsonParse.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonParse
#include <JsonClient.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonClient
#include "display.h" // for display.Note()
#include "WiFiManager.h"
#include "eeMem.h"
#include <WebSocketsClient.h> // https://github.com/Links2004/arduinoWebSockets
//switch WEBSOCKETS_NETWORK_TYPE to NETWORK_ESP8266_ASYNC in WebSockets.h
#if (WEBSOCKETS_NETWORK_TYPE != NETWORK_ESP8266_ASYNC)
#error "network type must be ESP8266 ASYNC!"
#endif
#ifdef USE_SPIFFS
#include <FS.h>
#include <SPIFFSEditor.h>
#else
#include "pages.h"
#endif

//-----------------
uint8_t serverPort = 80;

//-----------------
AsyncWebServer server( serverPort );
AsyncWebSocket ws("/ws");
extern HVAC hvac;
extern Display display;
WiFiManager wifi;
WebSocketsClient wsc;
bool bKeyGood;
int WsClientID;
bool bWscConnected;

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonParse remoteParse(remoteCallback);
void startListener(void);
void fcPage(AsyncWebServerRequest *request);

int xmlState;
void GetForecast(void);

const char page_index[] PROGMEM = R"rawliteral(
const char pageR[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<head>
<title>ESP-HVAC Remote</title>
</head>
<body">
<strong><em>CuriousTech HVAC Remote</em></strong><br>
<small>&copy 2016 CuriousTech.net</small>
</body>
</html>
)rawliteral";

// values sent at an interval of 30 seconds unless they change sooner
const char *jsonList1[] = { "state", "r", "fr", "s", "it", "rh", "tt", "fm", "ot", "ol", "oh", "ct", "ft", "rt", "h", "lt", "lh", "rmt", NULL };
const char *jsonList2[] = { "settings", "m", "am", "hm", "fm", "ot", "ht", "c0", "c1", "h0", "h1", "im", "cn", "cx", "ct", "fd", "ov", "rhm", "rh0", "rh1", NULL };
const char *cmdList[] = { "cmd",
  "key",
  "data",
  "sum",
  NULL};
  
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
      client->text(hvac.settingsJson());
      client->text(dataJson());
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
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
          bKeyGood = false; // for callback (all commands need a key)
          WsClientID = client->id();
          remoteParse.process(pCmd, pData);
        }
      }
      break;
  }
}

void startServer()
{
  WiFi.hostname("HVACRemote");
  wifi.autoConnect("HVACRemote", ee.password);  // AP you'll see on your phone

  Serial.println("");
  if(wifi.isCfg() == false)
  {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    if ( !MDNS.begin( "HVACRemote" ) )
      Serial.println( "MDNS responder failed" );
  }

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

#ifdef USE_SPIFFS
  SPIFFS.begin();
  server.addHandler(new SPIFFSEditor("admin", ee.password));
#endif

  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on ( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);
    if(wifi.isCfg())
      request->send( 200, "text/html", wifi.page() );
    else
      request->send_P( 200, "text/html", pageR );
  });

  server.on ( "/s", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){ // only used for config mode
    if(wifi.isCfg())
    {
      request->send( 200, "text/html", "Restarting" );
      delay(1000); // give it time to send
      parseParams(request);
    }
  });

  server.on ( "/json", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    String s = hvac.settingsJson();
    request->send( 200, "text/json", s + "\n");
  });

  // respond to GET requests on URL /heap
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    //Handle Unknown Request
//    request->send(404);
  });
  server.on( "/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.ico");
    request->send(404);
  });
  server.onFileUpload([](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    //Handle upload
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    //Handle body
  });

  server.begin();
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", serverPort);
#ifdef OTA_ENABLE
  ArduinoOTA.begin();
#endif

  remoteParse.addList(jsonList1);
  remoteParse.addList(jsonList2);
  remoteParse.addList(cmdList);
  remoteParse.addList(jsonList3);
}

void handleServer()
{
  MDNS.update();
#ifdef OTA_ENABLE
// Handle OTA server.
  ArduinoOTA.handle();
//  yield();
#endif
}

void WsSend(String s) // Browser WebSocket
{
  ws.textAll(s);
}

void WscSend(String s) // remote WebSocket
{
  wsc.sendTXT(s);
}

void secondsServer() // called once per second
{
  if(hvac.tempChange())
  {
    ws.textAll(dataJson());
  }

  static uint8_t start = 4; // give it time to settle before initial connect
  if(wifi.isCfg())
  {
    wifi.seconds();
  }
  else
  {
    if(start)
      if(--start == 0)
          startListener();
  }

  if(display.m_bUpdateFcst && bWscConnected)
  {
     display.m_bUpdateFcst = false;
     WscSend("cmd;{\"bin\":1}"); // forcast data
  }
}

void parseParams(AsyncWebServerRequest *request)
{
  char temp[100];
  int val;

  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);
    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
    int val = s.toInt();
 
    switch( p->name().charAt(0)  )
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
          if(ee.bLock) break;
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
}

// Pushed data
String dataJson()
{
  return hvac.getPushData();
}

void historyDump(bool bStart)
{
}

void appendDump(int startTime)
{
}

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  static bool bConn = false;
  String out;

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
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length)
{
  String s;

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
        switch(payload[0])
        {
          case 1: // forecast (512+1 bytes)
            if(length == sizeof(display.m_fcData)+1)
            {
              memcpy(display.m_fcData, payload+1, sizeof(display.m_fcData));
              display.m_bUpdateFcstDone = true;
            }
            break;
        }
      break;
  }
}

void startListener()
{
  wsc.onEvent(webSocketEvent);
  IPAddress ip(ee.hostIp);
  wsc.begin(ip.toString().c_str(), ee.hostPort, "/ws");
}
