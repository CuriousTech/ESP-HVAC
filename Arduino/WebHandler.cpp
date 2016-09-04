// Do all the web stuff here

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
#include "pages.h"

//-----------------
int serverPort = 85;            // Change to 80 for normal access

//-----------------
ESP8266WebServer server( serverPort );
WiFiManager wifi(0);  // AP page:  192.168.4.1
extern eventHandler event;
extern HVAC hvac;
extern Display display;

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient remoteStream(remoteCallback);
void graphData(void);

int nWrongPass;
uint32_t lastIP;

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

  server.on ( "/", handleRoot );
  server.on ( "/settings", handleSettings );
  server.on ( "/s", handleS );
  server.on ( "/json", handleJson );
  server.on ( "/events", handleEvents );
  server.on ( "/remote", handleRemote );
  server.on ( "/chart.html", graphData );
  server.onNotFound ( handleNotFound );
  server.begin();
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", serverPort);
}

void graphData()
{
  String content = "HTTP/1.1 200 OK\n"
      "Connection: close\n"
      "Content-Type: text/html\n\n";
  server.sendContent(content);
  server.sendContent(chart1);

  char szTemp[100];
  uint8_t pts[6];
  String out = "";

  for(int x = 0; x < display.m_pointsAdded; x++)
  {
    display.getGrapthPoints(pts, x);
    out += "{temp:\"";
    out += (float)(pts[0] * 110 / 101 + 660)/10;
    out += "\",rh:\"";
    out += (float)(pts[1] * 250 / 55) / 10;
    out += "\",h:\"";
    out += (float)(pts[2] * 110 / 101 + 660)/10;
    out += "\",l:\"";
    out += (float)(pts[3] * 110 / 101 + 660)/10;
    out += "\",s:";
    out += pts[4];
    out += "},\n";
  }

  server.sendContent(out);
  server.sendContent(chart2);
}

void handleServer()
{
  MDNS.update();
  server.handleClient();
  remoteStream.service();
}

void secondsServer() // called once per second
{
  if(nWrongPass)
    nWrongPass--;
  if(hvac.stateChange())
    event.push();
  else if(hvac.tempChange())
    event.pushInstant();
  else event.heartbeat();
}

void parseParams()
{
  char temp[100];
  char password[64];

  if(server.args() == 0)
    return;

  for ( uint8_t i = 0; i < server.args(); i++ ) // password may be at end
  {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);

    if(server.argName(i) == "key")
    {
      s.toCharArray(password, sizeof(password));
    }
  }

  uint32_t ip = server.client().remoteIP();

  if(strcmp(hvac.m_EE.password, password))
  {
    if(nWrongPass == 0)
      nWrongPass = 10;
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 10;
    String data = "{\"ip\":\"";
    data += server.client().remoteIP().toString();
    data += "\",\"pass\":\"";
    data += password; // bug - String object adds a NULL
    data += "\"}";
    event.push("hack", data); // log attempts
    lastIP = ip;
    return;
  }

  lastIP = ip;

  for ( uint8_t i = 0; i < server.args(); i++ )
  {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);

    if(server.argName(i) == "key");
    else if(server.argName(i) == "screen") // used by a PIR sensor elsewhere
      display.screen(true);
    else if(server.argName(i) == "rest")
      display.init();
    else if(server.argName(i) == "reset")
      ESP.reset();
    else
    {
      hvac.setVar(server.argName(i), s.toInt() );
      display.screen(true); // switch to main page, undim when variables are changed
    }
  }
}

void handleS() { // standard params, but no page
//  Serial.println("handleS\n");
  parseParams();

  server.send ( 200, "text/html", "OK" );
}


void handleRoot() // Main webpage interface
{
//  Serial.println("handleRoot");

  parseParams();

  server.send ( 200, "text/html", page1 );
}

void handleSettings() // Settings webpage interface
{
//  Serial.println("handleSettings");

  parseParams();

  server.send ( 200, "text/html", page2 );
}

// Return lots of vars as JSON
void handleJson()
{
//  Serial.println("handleJson\n");
  String s = hvac.settingsJson();
  server.send ( 200, "text/json", s + "\n");
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

// event streamer (assume keep-alive)
void handleEvents()
{
  char temp[100];
//  Serial.println("handleEvents");
  uint16_t interval = 60; // default interval
  uint8_t nType = 0;
  String key;
  uint16_t nPort = 80;
  char path[64] = "";

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
      case 's': // stream
        s.toCharArray(path, 64);
        break;
      case 'r': // port
        nPort = val;
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
  event.set(server.client(), interval, nType); // copying the client before the send makes it work with SDK 2.2.0

  if(key != hvac.m_EE.password || path[0] == 0)
    return;

  String sIp = server.client().remoteIP().toString();

  remoteStream.begin(sIp.c_str(), path, nPort, true);
  remoteStream.addList(jsonList1);
  remoteStream.addList(jsonList2);
  remoteStream.addList(jsonList3);
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
void handleRemote()
{
  char temp[100];
  String sIp;
  char path[64];
  char password[64];
  int nPort = 80;
  bool bEnd = false;
//  Serial.println("handleRemote");

  sIp = server.client().remoteIP().toString(); // default host IP is client

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
//    Serial.println( i + " " + server.argName ( i ) + ": " + s);

    if(server.argName(i) == "ip") // optional non-client source
      sIp = s;
    else if(server.argName(i) == "path")
      s.toCharArray(path, 64);
    else if(server.argName(i) == "port")
      nPort = s.toInt();
    else if(server.argName(i) == "end")
      bEnd = true;
    else if(server.argName(i) == "key")
      s.toCharArray(password, sizeof(password));
  }

  server.send ( 200, "text/html", "OK" );

  if(strcmp(hvac.m_EE.password, password))
  {
    String data = "{\"ip\":\"";
    data += server.client().remoteIP().toString();
    data += "\",\"pass\":\"";
    data += password;
    data += "\"}";
    event.push("hack", data); // log attempts
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
