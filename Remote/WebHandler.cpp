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
#include <WebSocketsClient.h> // https://github.com/Links2004/arduinoWebSockets
//switch WEBSOCKETS_NETWORK_TYPE to NETWORK_ESP8266_ASYNC in WebSockets.h

//-----------------
uint8_t serverPort = 86;            // firewalled

//-----------------
AsyncWebServer server( serverPort );
AsyncEventSource events("/events"); // event source (Server-Sent events)
extern HVAC hvac;
extern Display display;
WiFiManager wifi;
WebSocketsClient ws;

void startListener(void);

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient remoteStream(remoteCallback);

int chartFiller(uint8_t *buffer, int maxLen, int index);

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
   "<small>Copyright &copy 2016 CuriousTech.net</small>\n"
   "</body>\n"
   "</html>\n";

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

  server.on ( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);
    request->send_P( 200, "text/html", pageR );
  });

  server.on ( "/json", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    String s = hvac.settingsJson();
    request->send( 200, "text/json", s + "\n");
  });

  server.on ( "/chart.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->sendChunked("text/html", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    return chartFiller(buffer, maxLen, index);});
  });

  server.onNotFound(onRequest);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);

  server.begin();
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", serverPort);
}

// Send chart page in chunks.  First part is page HTML data.  Second is the data array.
int chartFiller(uint8_t *buffer, int maxLen, int index)
{
  int len = 0;
  static int entryIdx; // data array index, reset by start of response
  static int32_t tb; // time subtracted from entries (saves 5 bytes each)

  if(index == 0) // first call.  reset vars
  {
    entryIdx = 0;
    tb = 0;
  }

  if(maxLen <= 0) // This is -7 often
  {
    Serial.print("chartFiller error maxLen=");
    Serial.print(maxLen);
    Serial.print(" index=");
    Serial.println(index);
    return 0;
  }

  if(index < strlen_P(chart)) // Inside page data 
  {
    len = min(strlen_P(chart + index), maxLen);
    memcpy_P(buffer, chart + index, len);
  }

  if(len >= maxLen) // full
  {
    return len;
  }

  gPoint gpt;

  if(display.getGrapthPoints(&gpt, entryIdx) == false) // end
  {
    if(entryIdx == 0) // No data to fill
    { // Todo: complete it with a valid page end
      Serial.println("NO DATA");
    }
    return len;
  }

  while(entryIdx < GPTS)
  {
    if( display.getGrapthPoints(&gpt, entryIdx) == false)
      break;
   
    if(gpt.time == 0)
      continue; // some glitch?

    String out = "";
    if(tb == 0) // first entry found
    {
      tb = gpt.time - (60*60*26);  // subtract 26 hours form latest entry
      out += "<script>\ntb=";      // first data opening statements
      out += tb;
      out += "\ndata = { values:[\n";
    }
    out += "{t:";
    out += gpt.time - tb;
    out += ",temp:";
    out += gpt.temp * 110 / 101 + 660;
    out += ",rh:";
    out += gpt.rh * 250 / 55;
    out += ",h:";
    out += gpt.h * 110 / 101 + 660;
    out += ",l:";
    out += gpt.l * 110 / 101 + 660;
    out += ",s:";
    out += gpt.state;
    out += ",f:";
    out += gpt.fan;
    out += "},\n";

    if(len + out.length() >= maxLen)  // out of space
    {
      return len;
    }
    out.toCharArray((char *)buffer+len, maxLen-len);
    len += out.length();
    entryIdx++;
  }

  String out = "]};</script></html>";  // final closing statements

  out.toCharArray((char *)buffer+len, maxLen-len); // could get cut short
  len += out.length();
  return len;
}

void handleServer()
{
  MDNS.update();
}

void WsSend(String s)
{
  ws.sendTXT(s);
}

void secondsServer() // called once per second
{
  if(hvac.tempChange())
  {
    events.send(dataJson().c_str(), "state");
    String s = "state\n" + dataJson();
    ws.sendTXT(s);
  }

  static uint8_t cnt = 5;
  if(--cnt == 0)
  {
    cnt = 60; // refresh settings every 60 seconds
    WsSend("getSettings\nx");
  }

  static uint8_t start = 4; // give it time to settle before initial connect
  if(start)
  {
    if(--start == 0)
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
//    Serial.println( i + " " + p->name() + ": " + s);
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

// Pushed data
String dataJson()
{
  return hvac.getPushData();
}

// values sent at an interval of 30 seconds unless they change sooner
const char *jsonList1[] = { "state", "r", "fr", "s", "it", "rh", "tt", "fm", "ot", "ol", "oh", "ct", "ft", "rt", "h", "lt", "lh", "rmt", NULL };
// settings read about every minute
const char *jsonList3[] = { "", "m", "am", "hm", "fm", "ot", "ht", "c0", "c1", "h0", "h1", "im", "cn", "cx", "ct", "fd", "ov", "rhm", "rh0", "rh1", NULL };
const char *jsonList2[] = { "alert", NULL };

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
          char *pCmd = strtok((char *)payload, "\n");
          char *pData = strtok(NULL, "\n");
          remoteStream.process(pCmd, pData);
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
  ws.begin(ip.toString().c_str(), ee.hostPort, "/ws");
/*  Serial.print("WS begin ");
  Serial.print(ip.toString());
  Serial.print(" ");
  Serial.println(ee.hostPort);
  */
  ws.onEvent(webSocketEvent);
  remoteStream.addList(jsonList1);
  remoteStream.addList(jsonList2);
  remoteStream.addList(jsonList3);
}
