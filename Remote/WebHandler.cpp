// Do all the web stuff here (Remote unit)

#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESP8266WebServer.h>
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include "WebHandler.h"
#include <Event.h>
#include "HVAC.h"
#include <JsonClient.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonClient
#include "display.h" // for display.Note()

//-----------------
uint8_t serverPort = 86;            // firewalled

//-----------------
ESP8266WebServer server( serverPort );
WiFiManager wifi(0);  // AP page:  192.168.4.1
extern eventHandler event;
extern HVAC hvac;
extern Display display;

void startListener(void);
void getSettings(void);

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient remoteStream(remoteCallback);
void setCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient remoteSet(setCallback);

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

  server.on ( "/", handleRoot );
  server.on ( "/events", handleEvents );
  server.onNotFound ( handleNotFound );
  server.begin();
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", serverPort);
}

void handleServer()
{
  MDNS.update();
  server.handleClient();
  remoteStream.service();
  remoteSet.service();
}

void secondsServer() // called once per second
{
  if(hvac.tempChange())
    event.pushInstant();
  else event.heartbeat();

  static uint8_t cnt = 5;
  if(--cnt == 0)
  {
    cnt = 60; // refresh settings every 60 seconds
    getSettings();
  }

  static uint8_t start = 10; // give it time to settle before initial connect
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

void parseParams()
{
  char temp[100];
  int val;

//  Serial.println("parseArgs");

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
//    Serial.println( i + " " + server.argName ( i ) + ": " + s);
    int val = s.toInt();
 
    switch( server.argName(i).charAt(0)  )
    {
      case 'F': // temp offset
          hvac.m_EE.adj = val;
          break;
      case 'H': // host  (from browser type: hTtp://thisip/?H=hostip&P=85)
          {
            IPAddress ip;
            ip.fromString(s);
            hvac.m_EE.hostIp = ip;
            startListener(); // reset the URI
          }
          break;
      case 'P': // host port
          hvac.m_EE.hostPort = s.toInt();
          startListener();
          break;
    }
  }
}

void handleRoot() // Main webpage interface
{
//  Serial.println("handleRoot");
  parseParams();

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

  server.send ( 200, "text/html", page );
}

// event streamer (assume keep-alive)
void handleEvents()
{
  char temp[100];
//  Serial.println("handleEvents");
  uint16_t interval = 60; // default interval
  uint8_t nType = 0;
  String key;

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
//    Serial.println( i + " " + server.argName ( i ) + ": " + s);
    int val = s.toInt();
 
    switch( server.argName(i).charAt(0)  )
    {
      case 'i': // interval
        interval = val;
        break;
      case 'p': // push
        nType = 1;
        break;
      case 'c': // critical
        nType = 2;
        break;
      case 'k': // key
        key = s;
        break;
    }
  }

  if(nType == 2 && key != hvac.m_EE.password) // demote to plain if no/incorrect password
    nType = 0;

  String content = "HTTP/1.1 200 OK\n"
      "Connection: keep-alive\n"
      "Access-Control-Allow-Origin: *\n"
      "Content-Type: text/event-stream\n\n";
  server.sendContent(content);
  event.set(server.client(), interval, nType);
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
          event.print(String("remote error ") + iName + " " + psValue + ":" + iValue);
          event.alert("Remote link disconnected");
          hvac.m_notif = Note_Network;
          bConn = false;
          hvac.m_bLocalTempDisplay = true;
        }
      }
      else if(!bConn)
      {
        event.alert("Remote link connected");
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
//  static char path[] = "/events?i=30&p=1";

  String path = "/events?key=";
  path += hvac.m_EE.password;
  path += "&i=30&p=1&s=%2Fevents%3Fi=30%26p=1&r=";  // the path needs to be URL encoded
  path += serverPort;

  IPAddress ip(hvac.m_EE.hostIp);
  remoteStream.begin(ip.toString().c_str(), path.c_str(), hvac.m_EE.hostPort, true);
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
        event.print(String("getSettings error ") + iName + " " + psValue + ":" + iValue);
      break;
    case 0: // settings
      hvac.setSettings(iName, iValue);
      break;
  }
}

void getSettings()
{
  static char path[] = "/json";
  IPAddress ip(hvac.m_EE.hostIp);
  remoteSet.begin(ip.toString().c_str(), path, hvac.m_EE.hostPort, false);
  remoteSet.addList(jsonList3);
}

void handleNotFound() {
//  Serial.println("handleNotFound");

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}
