// Do all the web stuff here

//uncomment to enable Arduino IDE Over The Air update code
//#define OTA_ENABLE

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
#include <JsonClient.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonClient
#include "display.h" // for display.Note()
#include "pages.h"
#include "eeMem.h"

//-----------------
int serverPort = 85;            // Change to 80 for normal access

//-----------------
AsyncWebServer server( serverPort );
AsyncEventSource events("/events"); // event source (Server-Sent events)
WiFiManager wifi;  // AP page:  192.168.4.1
extern HVAC hvac;
extern Display display;

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient remoteStream(remoteCallback);

int nWrongPass;
uint32_t lastIP;

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //Handle body
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  //Handle upload
}

void onRequest(AsyncWebServerRequest *request){
  //Handle Unknown Request
  request->send(404);
}

void onEvents(AsyncEventSourceClient *client)
{
//  client->send(":ok", NULL, millis(), 1000);
  events.send(dataJson().c_str(), "state");
}

void startServer()
{
  WiFi.hostname("HVAC");
  wifi.autoConnect("HVAC"); // Tries all open APs, then starts softAP mode for config

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if( !MDNS.begin ( "HVAC", WiFi.localIP() ) ) {
    Serial.println ( "MDNS responder failed" );
  }

  // attach AsyncEventSource
  events.onConnect(onEvents);
  server.addHandler(&events);

  server.on ( "/", HTTP_GET | HTTP_POST, handleRoot );
  server.on ( "/s", HTTP_GET | HTTP_POST, handleS );
  server.on ( "/json", HTTP_GET | HTTP_POST, handleJson );
  server.on ( "/settings", HTTP_GET | HTTP_POST, handleSettings );
//  server.on ( "/remote", HTTP_GET | HTTP_POST, handleRemote );
  server.on ( "/chart.html", HTTP_GET, handleChart );

  server.onNotFound(onRequest);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);

  server.begin();

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", serverPort);

#ifdef OTA_ENABLE
  ArduinoOTA.begin();
#endif
}

void handleChart(AsyncWebServerRequest *request)
{
  AsyncResponseStream *response = request->beginResponseStream("text/html");

  String out = String(chart1); // Todo: Get from PROGMEM correctly

  gPoint gpt;

  int32_t tb = 0;

  for(int x = 0; x < GPTS; x++)
  {
    if( display.getGrapthPoints(&gpt, x) == false)
      break;
    if(gpt.time == 0)
      continue; // some glitch?
    if(tb == 0) // first entry found
    {
      tb = gpt.time - (60*60*26);
      out += tb;
      out += "\ndata = { values:[\n";
    }
    out += "{t:";
    out += gpt.time - tb;
    out += ",temp:\"";
    out += String((float)(gpt.temp * 110 / 101 + 660)/10, 1);
    out += "\",rh:\"";
    out += String((float)(gpt.rh * 250 / 55) / 10, 1);
    out += "\",h:\"";
    out += String((float)(gpt.h * 110 / 101 + 660)/10, 1);
    out += "\",l:\"";
    out += String((float)(gpt.l * 110 / 101 + 660)/10, 1);
    out += "\",s:";
    out += gpt.state;
    out += ",f:";
    out += gpt.fan;
    out += "},\n";
  }
  response->print(out);
  response->print(String(chart2));

  request->send ( response );
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

void secondsServer() // called once per second
{
  static int n = 10;

  if(nWrongPass)
    nWrongPass--;
  if(hvac.stateChange() || hvac.tempChange() || --n == 0)
  {
    events.send(dataJson().c_str(), "state" );
    n = 10;
  }
}

const char *jsonList1[] = { "state",  "temp", "rh", "tempi", "rhi", "rmt", NULL };
const char *jsonList2[] = { "cmd",
  "fanmode", // HVAC commands
  "mode",
  "heatmode",
  "resettotal",
  "resetfilter",
  "fanpostdelay",
  "cooltempl",
  "cooltemph",
  "heattempl",
  "heattemph",
  "humidmode",
  "avgrmt",
  NULL
};
const char *jsonList3[] = { "alert", NULL };

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
    events.send(data.c_str(), "hack"); // log attempts
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
    else
    {
      hvac.setVar(p->name(), s.toInt() );
//      display.screen(true); // switch to main page, undim when variables are changed
    }
  }
}

void handleS(AsyncWebServerRequest *request) { // standard params, but no page
//  Serial.println("handleS\n");
  parseParams(request);

  request->send ( 200, "text/html", "OK" );
}


void handleRoot(AsyncWebServerRequest *request) // Main webpage interface
{
//  Serial.println("handleRoot");

  parseParams(request);

  request->send_P ( 200, "text/html", page1 );
}

void handleSettings(AsyncWebServerRequest *request) // Settings webpage interface
{
//  Serial.println("handleSettings");

  parseParams(request);

  request->send_P ( 200, "text/html", page2 );
}

// Return lots of vars as JSON
void handleJson(AsyncWebServerRequest *request)
{
//  Serial.println("handleJson\n");
  String s = hvac.settingsJson();
  s += "\n";
  request->send ( 200, "text/json", s);
  handleRemote(request);
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
    case -1: // connection status
//      if(iName == JC_CONNECTED) event.print("Remote connected");
//      else event.print("Remote disconnected " + iName);
      break;
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
      hvac.setVar(jsonList2[iName+1], iValue); // 0 is "fanmode"
      break;
    case 2: // alert
      display.Note(psValue);
      break;
  }
}

// remote streamer url/ip
void handleRemote(AsyncWebServerRequest *request) // Todo: WebSocket
{
  char temp[100];
  String sIp;
  char path[64];
  char password[64];
  int nPort = 80;
  bool bEnd = false;
//  Serial.println("handleRemote");

  if(request->params() == 0)
    return;

  sIp = request->client()->localIP().toString(); // default host IP is client

  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);
    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
//    Serial.println( i + " " + p->name() + ": " + s);

    if(p->name() == "ip") // optional non-client source
      sIp = s;
    else if(p->name() == "path")
      s.toCharArray(path, 64);
    else if(p->name() == "port")
      nPort = s.toInt();
    else if(p->name() == "end")
      bEnd = true;
    else if(p->name() == "key")
      s.toCharArray(password, sizeof(password));
  }

  if(strcmp(ee.password, password))
  {
    String data = "{\"ip\":\"";
    data += request->client()->localIP().toString();
    data += "\",\"pass\":\"";
    data += password;
    data += "\"}";
    events.send(data.c_str(), "hack"); // log attempts
    return;
  }

  if(bEnd) // end remote control and sensor
  {
    remoteStream.end();
    hvac.m_bRemoteStream = false;
    return;
  }

  remoteStream.begin(sIp.c_str(), path, nPort, true);
  remoteStream.addList(jsonList1);
  remoteStream.addList(jsonList2);
  remoteStream.addList(jsonList3);
}
