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
uint8_t serverPort = 86;            // firewalled

//-----------------
AsyncWebServer server( serverPort );
AsyncEventSource events("/events"); // event source (Server-Sent events)
extern HVAC hvac;
extern Display display;
WiFiManager wifi;
WebSocketsClient ws;
AsyncClient fc_client;

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonParse remoteParse(remoteCallback);
void startListener(void);
void dataPage(AsyncWebServerRequest *request);
void fcPage(AsyncWebServerRequest *request);
int chartFiller(uint8_t *buffer, int maxLen, int index);

int xmlState;
void GetForecast(void);

void onEvents(AsyncEventSourceClient *client)
{
  static bool rebooted = true;
  events.send(dataJson().c_str(), "state");
  if(rebooted)
  {
    events.send("Restarted", "alert");
    rebooted = false;
  }
}

const char pageR[] PROGMEM = 
   "<!DOCTYPE html>\n"
   "<html>\n"
   "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n"
   "<head>\n"
   "\n"
   "<title>ESP-HVAC Remote</title>\n"
   "\n"
   "<style type=\"text/css\">\n"
   ".style1 {border-width: 0;}\n"
   ".style2 {text-align: right;}\n"
   ".style3 {background-color: #C0C0C0;}\n"
   ".style4 {text-align: right;background-color: #C0C0C0;}\n"
   ".style5 {background-color: #00D0D0;}\n"
   ".style6 {border-style: solid;border-width: 1px;background-color: #C0C0C0;}\n"
   "</style>\n"
   "\n"
   "</head>\n"
   "<body\">\n"
   "<strong><em>CuriousTech HVAC Remote</em></strong><br>\n"
   "<input type=\"submit\" value=\"Chart\" onClick=\"window.location='/chart.html';\">\n"
   "<small>Copyright &copy 2016 CuriousTech.net</small>\n"
   "</body>\n"
   "</html>\n";

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
    if ( !MDNS.begin ( "HVACRemote", WiFi.localIP() ) )
      Serial.println ( "MDNS responder failed" );
  }

#ifdef USE_SPIFFS
  SPIFFS.begin();
  server.addHandler(new SPIFFSEditor("admin", ee.password));
#endif

  // attach AsyncEventSource
  events.onConnect(onEvents);
  server.addHandler(&events);

  server.on ( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
//    Serial.println("handleRoot");
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

  server.on ( "/chart.html", HTTP_GET, [](AsyncWebServerRequest *request){
    parseParams(request);
#ifdef USE_SPIFFS
      request->send(SPIFFS, "/chart.html");
#else
      request->send_P(200, "text/html", chart);
#endif
  });
  server.on ( "/data", HTTP_GET, dataPage);
  server.on ( "/forecast", HTTP_GET, fcPage);

  server.onNotFound([](AsyncWebServerRequest *request){
    //Handle Unknown Request
//    request->send(404);
  });
  server.on( "/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
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
  ArduinoOTA.onStart([]()
  {
    String sType = "Begin ";
    sType += (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "SPIFFS";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    events.send(sType.c_str(), "OTA");
  });
  ArduinoOTA.onEnd([]()
  {
    events.send("End", "OTA");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  {
  });
  ArduinoOTA.onError([](ota_error_t error)
  {
    events.send("Error " + error, "OTA");
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    delay(200);
    ESP.restart();
  });
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
  String s = type;
  s += ";";
  s += txt;
  ws.sendTXT(s);
}

void secondsServer() // called once per second
{
  static uint8_t timer = 10;

  if(--timer == 0) // event stream needs a keep-alive of 10 seconds
  {
    timer = 10;
    events.send("", "");
  }

  if(hvac.tempChange())
  {
    events.send(dataJson().c_str(), "state");
    WsSend((char*)dataJson().c_str(), "state");
    timer = 10;
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

  if(display.m_bUpdateFcst)
  {
     display.m_bUpdateFcst = false;

     // Request data from main unit
     if(fc_client.connected() == false)
     {
       display.m_fcData[0].temp = display.m_fcData[1].temp; // keep a copy of first 3hour data
       display.m_fcData[0].tm = display.m_fcData[1].tm;
       IPAddress ip(ee.hostIp);
       fc_client.connect(ip, ee.hostPort);
     }
  }
}

void parseParams(AsyncWebServerRequest *request)
{
  char temp[100];
  int val;

//  Serial.println("parseParams");

  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);
    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
    Serial.println( i + " " + p->name() + ": " + s);
    int val = s.toInt();
 
    switch( p->name().charAt(0)  )
    {
      case 'T': // temp offset
          ee.adj = val;
          break;
      case 'f': // get forecast
          display.m_bUpdateFcst = true;
          break;
      case 'H': // host  (from browser type: hTtp://thisip/?H=hostip&P=85)
          {
            IPAddress ip;
            ip.fromString(s);
            ee.hostIp = ip;
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
      case 'P': // host port
          ee.hostPort = s.toInt();
          startListener();
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

// values sent at an interval of 30 seconds unless they change sooner
const char *jsonList1[] = { "state", "r", "fr", "s", "it", "rh", "tt", "fm", "ot", "ol", "oh", "ct", "ft", "rt", "h", "lt", "lh", "rmt", NULL };
const char *jsonList2[] = { "settings", "m", "am", "hm", "fm", "ot", "ht", "c0", "c1", "h0", "h1", "im", "cn", "cx", "ct", "fd", "ov", "rhm", "rh0", "rh1", NULL };
const char *jsonList3[] = { "alert", NULL };

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  static bool bConn = false;

  switch(iEvent)
  {
    case 0: // state
      hvac.updateVar(iName, iValue);
      break;
    case 1: // settings
      hvac.setSettings(iName, iValue);
      break;
    case 2: // alert
      display.Note(psValue);
      break;
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length)
{
  switch(type)
  {
    case WStype_DISCONNECTED:
      hvac.m_bLocalTempDisplay = true;
      hvac.m_notif = Note_Network;
      break;
    case WStype_CONNECTED:
      hvac.m_bLocalTempDisplay = false;
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
//      USE_SERIAL.printf("[WSc] get binary lenght: %u\n", length);
//      hexdump(payload, length);

      // send data to server
      // ws.sendBIN(payload, length);
      break;
  }
}

void startListener()
{
  IPAddress ip(ee.hostIp);
  remoteParse.addList(jsonList1);
  remoteParse.addList(jsonList2);
  remoteParse.addList(jsonList3);
  ws.onEvent(webSocketEvent);
  ws.begin(ip.toString().c_str(), ee.hostPort, "/ws");
}

int fcIdx;

void fc_onConnect(AsyncClient* client)
{
  (void)client;

  IPAddress ip(ee.hostIp);
  String s = "GET /forecast HTTP/1.1\n"
    "Host: ";
  s += ip.toString();
  s += "\nConnection: close\n"
    "Accept: */*\n\n";

  fc_client.add(s.c_str(), s.length());
  fcIdx = 0; // 0 is reserved
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
