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
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

WiFiManager wifi;  // AP page:  192.168.4.1
extern HVAC hvac;
extern Display display;

void remoteCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient remoteStream(remoteCallback);
int chartFiller(uint8_t *buffer, int maxLen, int index);

int nWrongPass;
uint32_t lastIP;
bool bKeyGood;
int WsClientID;
int WsRemoteID;

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //Handle body
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  //Handle upload
}

//Handle Unknown Request
void onRequest(AsyncWebServerRequest *request){
  request->send(404);
}

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
  if(type == WS_EVT_CONNECT){
    //client connected
//    events.send("ws connect", "print");
    client->printf("settings;%s", hvac.settingsJson().c_str()); // update everything on start
    client->printf("state;%s", dataJson().c_str());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    if(hvac.m_bRemoteStream && client->id() == WsRemoteID) // stop remote
    {
       hvac.m_bRemoteStream = false;
       hvac.m_bLocalTempDisplay = !hvac.m_bRemoteStream; // switch to showing local/remote color
       hvac.m_notif = Note_RemoteOff;
    }
    //client disconnected
//    events.send("ws disconnect", "print");
  } else if(type == WS_EVT_ERROR){
    if(hvac.m_bRemoteStream && client->id() == WsRemoteID)
    {
       hvac.m_bRemoteStream = false;
       hvac.m_bLocalTempDisplay = !hvac.m_bRemoteStream; // switch to showing local/remote color
       hvac.m_notif = Note_RemoteOff;
    }
    //error was received from the other end
//    events.send("ws error\n", "print");// *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    //pong message was received (in response to a ping request maybe)
//    events.send("ws pong", "print");// len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    //data packet
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      if(info->opcode == WS_TEXT){
        data[len] = 0;

        char *pCmd = strtok((char *)data, ";"); // assume format is "name;{json:x}"
        char *pData = strtok(NULL, "");

        if(!strcmp(pCmd, "getSettings" ) )
        {
            client->printf("settings;%s", hvac.settingsJson().c_str());
        }
        else
        {
          bKeyGood = false; // for callback (all commands need a key)
          WsClientID = client->id();
          remoteStream.process(pCmd, pData);
 //       Serial.printf("%s\n", (char*)data);
        }
      } else { // binary
          events.send("ws binary", "print");// (info->opcode == WS_TEXT)?"text":"binary", info->len);
//        for(size_t i=0; i < info->len; i++){
//          Serial.printf("%02x ", data[i]);
//        }
//        Serial.printf("\n");
//        client->binary("I got your binary message");
      }
    } else {
      events.send("ws chunked", "print");
      //message is comprised of multiple frames or the frame is split into multiple packets
/*      if(info->index == 0){
        if(info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      os_printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
      if(info->message_opcode == WS_TEXT){
        data[len] = 0;
        Serial.printf("%s\n", (char*)data);
      } else {
        for(size_t i=0; i < len; i++){
          Serial.printf("%02x ", data[i]);
        }
        Serial.printf("\n");
      }

      if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }*/
    }
  }
}

const char *jsonList1[] = { "state",  "temp", "rh", "tempi", "rhi", "rmt", NULL };
const char *jsonList2[] = { "cmd",
  "key",
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
  "ppk",
  "ccf",
  NULL
};
const char *jsonList3[] = { "alert", NULL };

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

  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  server.on ( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request){
    parseParams(request);
    request->send_P ( 200, "text/html", page1 );
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
    request->send_P ( 200, "text/html", page2 );
  });
  server.on ( "/chart.html", HTTP_GET, [](AsyncWebServerRequest *request){
    parseParams(request);
    request->send_P ( 200, "text/html", chart );
  });
  server.on ( "/data.json", HTTP_GET, [](AsyncWebServerRequest *request){
    request->sendChunked("text/javascript", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    return chartFiller(buffer, maxLen, index);});
  });

  server.onNotFound(onRequest);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);

  // respond to GET requests on URL /heap
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server.begin();

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", serverPort);

  remoteStream.addList(jsonList1);
  remoteStream.addList(jsonList2);
  remoteStream.addList(jsonList3);

#ifdef OTA_ENABLE
  ArduinoOTA.begin();
#endif
}

// Send the JSON formated chart data and any modified variables
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

  if(maxLen <= 0) // This is -7 on 2nd chunk often
  {
    return 0;
  }

  gPoint gpt;

  if(display.getGrapthPoints(&gpt, entryIdx) == false) // end
  {
    if(entryIdx == 0) // No data to fill
    { // Todo: complete it with a valid page end
//      Serial.println("NO DATA");
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
      out += "tb=";      // first data opening statements
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

  String out = "]}\n";  // final closing statements

  out.toCharArray((char *)buffer+len, maxLen-len); // could get cut short
  len += out.length();
  return len;
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
  if(nWrongPass)
    nWrongPass--;

  static int n = 10;
  if(hvac.stateChange() || hvac.tempChange())
  {
    events.send(dataJson().c_str(), "state" );
    // push to all WebSockets
    ws.printfAll("state;%s", dataJson().c_str());
    n = 10;
  }
  else if(--n == 0)
  {
    events.send("", "" ); // keepalive
    n = 10;
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
          hvac.setVar(jsonList2[iName+1], iValue); // 1 is "fanmode"
      }
      break;
    case 2: // alert
      display.Note(psValue);
      break;
  }
}
