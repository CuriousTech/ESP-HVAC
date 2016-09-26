// Do all the web stuff here (Remote unit)

#include <ESP8266mDNS.h>
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
#include "WiFiManager.h"
#include "eeMem.h"

//-----------------
uint8_t serverPort = 86;            // firewalled

//-----------------
AsyncWebServer server( serverPort );
AsyncEventSource events("/events"); // event source (Server-Sent events)
extern HVAC hvac;
extern Display display;
WiFiManager wifi;

void startListener(void);
void getSettings(void);
void startRemote(void);

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient remoteStream(remoteCallback);
void setCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient remoteSet(setCallback);
void handleChart(AsyncWebServerRequest *request);

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
  WiFi.hostname("HVACRemote");
  wifi.autoConnect("HVACRemote");  // AP you'll see on your phone

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if ( !MDNS.begin ( "HVACRemote", WiFi.localIP() ) ) {
    Serial.println ( "MDNS responder failed" );
  }

  // attach AsyncEventSource
  events.onConnect(onEvents);
  server.addHandler(&events);

  server.on ( "/", HTTP_GET | HTTP_POST, handleRoot );
  server.on ( "/json", HTTP_GET | HTTP_POST, handleJson );
  server.on ( "/chart.html", HTTP_GET, handleChart );

  server.onNotFound(onRequest);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);

  server.begin();
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", serverPort);
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
}

void secondsServer() // called once per second
{
  if(hvac.tempChange())
    events.send(dataJson().c_str(), "state");

  static uint8_t cnt = 5;
  if(--cnt == 0)
  {
    cnt = 60; // refresh settings every 60 seconds
    getSettings();
  }

  static uint8_t start = 15; // give it time to settle before initial connect
  if(start)
  {
    if(--start == 0)
      startListener();
  }
  else
  {
    if(remoteStream.status() == JC_RETRY_FAIL)
      startListener();
  }
}

void parseParams(AsyncWebServerRequest *request)
{
  char temp[100];
  int val;

//  Serial.println("parseArgs");

  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);
    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
//    Serial.println( i + " " + server.argName ( i ) + ": " + s);
    int val = s.toInt();
 
    switch( p->name().charAt(0)  )
    {
      case 'F': // temp offset
          ee.adj = val;
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
      case 'P': // host port
          ee.hostPort = s.toInt();
          startListener();
          break;
    }
  }
}

void handleRoot(AsyncWebServerRequest *request) // Main webpage interface
{
//  Serial.println("handleRoot");
  parseParams(request);

  String page = 
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
   "<small>Copyright &copy 2016 CuriousTech.net</small>\n"
   "</body>\n"
   "</html>\n";

  request->send( 200, "text/html", page );
}

// Return lots of vars as JSON
void handleJson(AsyncWebServerRequest *request)
{
//  Serial.println("handleJson\n");
  String s = hvac.settingsJson();
  request->send( 200, "text/json", s + "\n");
}

// Pushed data
String dataJson()
{
  return hvac.getPushData();
}

// values sent at an interval of 30 seconds unless they change sooner
const char *jsonList1[] = { "state", "r", "fr", "s", "it", "rh", "tt", "fm", "ot", "ol", "oh", "ct", "ft", "rt", "h", "lt", "lh", "rmt", NULL };
const char *jsonList2[] = { "alert", NULL };

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  static bool bConn = false;

  switch(iEvent)
  {
    case -1:
      if(iName != JC_CONNECTED)
      {
        if(bConn)
        {
          String s = String("remote error ");
          s += iName;
          s += " ";
          s += psValue;
          s += ":";
          s += iValue;
          events.send(s.c_str(), "print");
          events.send("Remote link disconnected", "alert");
          hvac.m_notif = Note_Network;
          bConn = false;
          hvac.m_bLocalTempDisplay = true;
        }
      }
      else if(!bConn)
      {
//        event.alert("Remote link connected");
        hvac.m_bLocalTempDisplay = false;
        bConn = true;
        if(hvac.m_notif == Note_Network)
          hvac.m_notif = Note_None;
      }
      break;
    case 0: // state
      hvac.updateVar(iName, iValue);
      break;
    case 1: // alert
      display.Note(psValue);
      break;
  }
}

void startListener()
{
  IPAddress ip(ee.hostIp);
  remoteStream.begin(ip.toString().c_str(), "/events", ee.hostPort, true);
  remoteStream.addList(jsonList1);
  remoteStream.addList(jsonList2);
}

// settings read about every minute
const char *jsonList3[] = { "", "m", "am", "hm", "fm", "ot", "ht", "c0", "c1", "h0", "h1", "im", "cn", "cx", "ct", "fd", "ov", "rhm", "rh0", "rh1", NULL };

void setCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  switch(iEvent)
  {
    case -1:
      if(iName != JC_CONNECTED)
      {
        String s = String("getSettings error ");
        s += iName;
        s += " ";
        s += psValue;
        s += ":";
        s += iValue;
        events.send(s.c_str(), "print");
      }
      break;
    case 0: // settings
      hvac.setSettings(iName, iValue);
      break;
  }
}

void getSettings()
{
  String path = "/json";

  static bool bInit = false;
  if(bInit == false) // send eventlistener data only once
  {
    bInit = true;
    path += "?key=";
    path += ee.password;
    path += "&port=";
    path += serverPort;
  }
  IPAddress ip(ee.hostIp);
  remoteSet.begin(ip.toString().c_str(), path.c_str(), ee.hostPort, false);
  remoteSet.addList(jsonList3);
}
