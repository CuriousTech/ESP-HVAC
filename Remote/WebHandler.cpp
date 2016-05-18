// Do all the web stuff here (Remote unit)

#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include "WebHandler.h"
#include <Event.h>
#include "HVAC.h"
#include <JsonClient.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonClient
#include "display.h" // for display.Note()

//-----------------
const char *controlPassword = "password"; // password on main unit
int serverPort = 80;            // Doesn't really matter
const char *hostIp = "192.168.0.100"; // Main unit address and port
uint8_t hostPort = 85;

//-----------------
ESP8266WebServer server( serverPort );
WiFiManager wifi(0);  // AP page:  192.168.4.1
extern MDNSResponder mdns;
extern eventHandler event;
extern HVAC hvac;
extern Display display;

void startRemote(void);
void getSettings(void);

void remoteCallback(uint16_t iEvent, uint16_t iName, uint16_t iValue, char *psValue);
JsonClient remoteStream(remoteCallback);
void setCallback(uint16_t iEvent, uint16_t iName, uint16_t iValue, char *psValue);
JsonClient remoteSet(setCallback);

void startServer()
{
  wifi.autoConnect("HVACRemote");  // AP you'll see on your phone

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if ( mdns.begin ( "esp8266", WiFi.localIP() ) ) {
    Serial.println ( "MDNS responder started" );
  }

  server.on ( "/", handleRoot );
  server.on ( "/events", handleEvents );
  server.onNotFound ( handleNotFound );
  server.begin();
}

void handleServer()
{
  mdns.update();
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
    {
      startRemote();
    }
  }
}

void handleRoot() // Main webpage interface
{
//  Serial.println("handleRoot");

  server.send ( 200, "text/html", "OK" );
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

  if(nType == 2 && key != controlPassword) // demote to plain if no/incorrect password
    nType = 0;

  String content = "HTTP/1.1 200 OK\n"
      "Connection: keep-alive\n"
      "Access-Control-Allow-Origin: *\n"
      "Content-Type: text/event-stream\n\n";
  server.sendContent(content);
  event.set(server.client(), interval, nType); // copying the client before the send makes it work with SDK 2.2.0
}

// Pushed data
String dataJson()
{
  return hvac.getPushData();
}

// values sent at an interval of 30 seconds unless they change sooner
const char *jsonList1[] = { "state", "r", "fr", "s", "it", "rh", "tt", "fm", "ot", "ol", "oh", "ct", "ft", "rt", "h", NULL };
const char *jsonList2[] = { "alert", NULL };

void remoteCallback(uint16_t iEvent, uint16_t iName, uint16_t iValue, char *psValue)
{
  switch(iEvent)
  {
    case 0: // state
      hvac.updateVar(iName, iValue);
      break;
    case 1: // alert
      display.Note(psValue);
      break;
  }
}

void startRemote()
{
  static char path[] = "/events?i=30&p=1";
  remoteStream.begin(hostIp, path, hostPort, true);
  remoteStream.addList(jsonList1);
  remoteStream.addList(jsonList2);
}

// settings read about every minute
const char *jsonList3[] = { "", "m", "am", "hm", "fm", "ot", "ht", "c0", "c1", "h0", "h1", "im", "cn", "cx", "ct", "fd", "ov", "rhm", "rh0", "rh1", NULL };

void setCallback(uint16_t iEvent, uint16_t iName, uint16_t iValue, char *psValue)
{
  switch(iEvent)
  {
    case 0: // settings
      hvac.setSettings(iName, iValue);
      break;
  }
}

void getSettings()
{
  static char path[] = "/json";
  remoteSet.begin(hostIp, path, hostPort, false);
  remoteSet.addList(jsonList3);
}

void handleNotFound() {
//  Serial.println("handleNotFound\n");

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
