// Do all the web stuff here

#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESP8266WebServer.h>
#include <Time.h>
#include <WiFiUdp.h>
#include "WebHandler.h"
#include "event.h"
#include "HVAC.h"
#include "JsonClient.h"
#include "display.h" // for display.Note()

//-----------------
const char *controlPassword = "esp8266"; // device password for modifying any settings
int serverPort = 85;            // Change to 80 for normal access
//-----------------
ESP8266WebServer server( serverPort );
WiFiManager wifi(0);  // AP page:  192.168.4.1
extern MDNSResponder mdns;
extern eventHandler event;
extern HVAC hvac;
extern Display display;

void remoteCallback(uint16_t iEvent, uint16_t iName, uint16_t iValue, char *psValue);
JsonClient remoteStream(remoteCallback);

int nWrongPass;
uint32_t lastIP;

void startServer()
{
  wifi.autoConnect("HVAC"); // Tries all open APs, then starts softAP mode for config

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if ( mdns.begin ( "esp8266", WiFi.localIP() ) ) {
    Serial.println ( "MDNS responder started" );
  }

  server.on ( "/", handleRoot );
  server.on ( "/s", handleS );
  server.on ( "/json", handleJson );
  server.on ( "/events", handleEvents );
  server.on ( "/remote", handleRemote );
  server.onNotFound ( handleNotFound );
  server.begin();
}

void handleServer()
{
  mdns.update();
  server.handleClient();
  hvac.m_bRemoteConnected = remoteStream.service();
}

void secondsServer() // called once per second
{
  if(nWrongPass)
    nWrongPass--;
  if(hvac.stateChange())
    event.pushInstant();
  event.heartbeat();
}

String ipString(IPAddress ip) // Convert IP to string
{
  String sip = String(ip[0]);
  sip += ".";
  sip += ip[1];
  sip += ".";
  sip += ip[2];
  sip += ".";
  sip += ip[3];
  return sip;
}

void parseParams()
{
  char temp[100];
  String password;

  for ( uint8_t i = 0; i < server.args(); i++ )
  {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
//    Serial.println( i + " " + server.argName ( i ) + ": " + s);

    if(server.argName(i) == "key")
    {
      password = s;
    }
    else if(server.argName(i) == "set")
    {
      if(password == controlPassword)
      {
        String cmd = s.substring(0, s.indexOf('='));
        int val = s.substring(s.indexOf('=')+1).toInt();
        hvac.setVar(cmd, val);
      }
    }
  }
 
  uint32_t ip = server.client().remoteIP();

  if(server.args() && (password != controlPassword) )
  {
    if(nWrongPass == 0)
      nWrongPass = 10;
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 10;
    event.alert("HackIP=" + ipString(ip) ); // log attempts
  }
  
  lastIP = ip;
}

void handleRoot() // Main webpage interface
{
//  Serial.println("handleRoot");

  parseParams();
  server.send ( 200, "text/html", "" ); // Send this in chunks.  Data limit is around 2048 bytes.
  WiFiClient client = server.client();

  String page =
   "<!DOCTYPE html>\n"
   "<html>\n"
   "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n"
   "<head>\n"
   "\n"
   "<title>ESP-HVAC</title>\n"
   "\n"
   "<style type=\"text/css\">\n"
   ".style1 {border-width: 0;}\n"
   ".style2 {text-align: right;}\n"
   ".style3 {background-color: #C0C0C0;}\n"
   ".style4 {text-align: right;background-color: #C0C0C0;}\n"
   ".style5 {background-color: #00AAC0;}\n"
   ".style6 {border-style: solid;border-width: 1px;background-color: #C0C0C0;}\n"
   "</style>\n"
   "\n"
   "<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/1.3.2/jquery.min.js\" type=\"text/javascript\" charset=\"utf-8\"></script>\n"
   "\n"
   "<script type=\"text/javascript\">\n"
   "<!--\n"
   "\n"
   "var Json,mode,autoMode,heatMode,fanMode,running,fan\n"
   "var states = new Array('Idle','Cooling','HP Heat','NG Heat')\n"
   "var heatmodes = new Array('HP','NG','Auto')\n"
   "\n"
   "function startEvents()\n"
   "{\n"
   "  eventSource = new EventSource(\"events?i=60&p=1\")\n"
   "  eventSource.addEventListener('open',function(e){},false)\n"
   "  eventSource.addEventListener('error',function(e){},false)\n"
   "  eventSource.addEventListener('state',function(e){ // listen to frequent changed values\n"
   "    Json = JSON.parse(e.data)\n"
   "\n"
   "    running = +Json.r\n"
   "    fan = +Json.fr\n"
   "  \n"
   "    document.all.intemp.innerHTML = +Json.it/10\n"
   "    document.all.rh.innerHTML = +Json.rh/10\n"
   "    document.all.target.innerHTML = +Json.tt/10\n"
   "    document.all.outtemp.innerHTML = +Json.ot/10\n"
   "    document.all.cyctimer.innerHTML = secsToTime(+Json.ct)\n"
   "    document.all.runtotal.innerHTML = secsToTime(+Json.rt)\n"
   "\n"
   "    document.all.fan.innerHTML = fan?\"Fan On\":\"Fan Off\"\n"
   "    document.all.fanCell.setAttribute('class',fan?'style5':'style1')\n"
   "    document.all.fAuto.setAttribute('class', fanMode==0?'style5':'')\n"
   "    document.all.fOn.setAttribute('class', fanMode==1?'style5':'')\n"
   "    document.all.run.innerHTML = states[+Json.s]\n"
   "    document.all.runCell.setAttribute('class',running?'style5':'style1')\n";
  client.print(page);
  page = 
   "  },false)\n"
   "}\n"
   "\n"
   "function readSettings() // read all settings\n"
   "{\n"
   "  $.getJSON(\"json\", function(Json){\n"
   "      mode = +Json.m\n"
   "    autoMode = +Json.am\n"
   "    heatMode = +Json.hm\n"
   "    fanMode = +Json.fm\n"
   "    ovrActive = +Json.ot\n"
   "\n"
   "    document.all.mOff.setAttribute('class',mode==0?'style5':'')\n"
   "    document.all.mCool.setAttribute('class',mode==1?'style5':'')\n"
   "    document.all.mHeat.setAttribute('class',mode==2?'style5':'')\n"
   "    document.all.mAuto.setAttribute('class',mode==3?'style5':'')\n"
   "\n"
   "    document.all.hHP.setAttribute('class',heatMode==0?'style5':'')\n"
   "    document.all.hGas.setAttribute('class',heatMode==1?'style5':'')\n"
   "    document.all.hAuto.setAttribute('class',heatMode==2?'style5':'')\n"
   "  \n"
   "    document.all.cooll.value= +Json.c0/10\n"
   "    document.all.coolh.value= +Json.c1/10\n"
   "    document.all.heatl.value= +Json.h0/10\n"
   "    document.all.heath.value= +Json.h1/10\n"
   "    document.all.idlemin.value= +Json.im\n"
   "    document.all.cycmin.value= +Json.cn\n"
   "    document.all.cycmax.value= +Json.cx\n"
   "    document.all.thresh.value= +Json.ct/10\n"
   "    document.all.fandelay.value= +Json.fd\n"
   "    document.all.ovrtime.value= +Json.ov\n"
   "  \n"
   "    if( +document.all.ovrtemp.value==0) // set a better default\n"
   "      document.all.ovrtemp.value= -1.0\n"
   "    document.all.ovrCell.setAttribute('class',ovrActive?'style5':'style1')\n"
   "  });\n"
   "}\n"
   "\n"
   "function setVar(varName, value)\n"
   "{\n"
   "  $.post(\"s\",{key: document.all.myToken.value, set: varName+'='+value})\n"
   "}\n"
   "\n"
   "function setfan(n)\n"
   "{\n"
   "  fanMode=n\n"
   "  fan = fanMode?1:run\n"
   "  setVar('fanmode',fanMode?1:0)\n"
   "  document.all.fAuto.setAttribute('class',fanMode==0?'style5':'')\n"
   "  document.all.fOn.setAttribute('class',fanMode==1?'style5':'')\n"
   "  document.all.fan.innerHTML=\"Fan \"+(fanMode?\"On\":(fan?\"On\":\"Off\"))\n"
   "  document.all.fanCell.setAttribute('class',fan?'style5' : 'style1');\n"
   "}\n"
   "\n"
   "function setMode(m)\n";
  client.print(page);
  page = 
   "{\n"
   "  setVar('mode',mode=m)\n"
   "  document.all.mOff.setAttribute('class',mode==0?'style5':'')\n"
   "  document.all.mCool.setAttribute('class',mode==1?'style5':'')\n"
   "  document.all.mHeat.setAttribute('class',mode==2?'style5':'')\n"
   "  document.all.mAuto.setAttribute('class',mode==3?'style5':'')\n"
   "}\n"
   "\n"
   "function setHeatMode(m)\n"
   "{\n"
   "  setVar('heatmode',heatMode=m)\n"
   "  document.all.hHP.setAttribute('class',heatMode==0?'style5':'')\n"
   "  document.all.hGas.setAttribute('class',heatMode==1?'style5':'')\n"
   "  document.all.hAuto.setAttribute('class',heatMode==2?'style5':'')\n"
   "}\n"
   "\n"
   "function setCoolHi()\n"
   "{\n"
   "  setVar('cooltemph',(+document.all.coolh.value * 10).toFixed())\n"
   "}\n"
   "\n"
   "function setCoolLo()\n"
   "{\n"
   "  setVar('cooltempl',(+document.all.cooll.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function incCool(n)\n"
   "{\n"
   "  document.all.coolh.value= +document.all.coolh.value+n\n"
   "  document.all.cooll.value= +document.all.cooll.value+n\n"
   "\n"
   "  setVar('cooltemph',(+document.all.coolh.value*10).toFixed())\n"
   "  setVar('cooltempl',(+document.all.cooll.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setHeatHi()\n"
   "{\n"
   "  setVar('heattemph',(+document.all.heath.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setHeatLo()\n"
   "{\n"
   "  setVar('heattempl',(+document.all.heatl.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function incHeat(n)\n"
   "{\n"
   "  document.all.heath.value= +document.all.heath.value+n\n"
   "  document.all.heatl.value= +document.all.heatl.value+n\n"
   "\n"
   "  setVar('heattemph',(+document.all.heath.value*10).toFixed())\n"
   "  setVar('heattempl',(+document.all.heatl.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setThresh()\n"
   "{\n"
   "  setVar('cyclethresh',(+document.all.thresh.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setFeanDelay()\n"
   "{\n"
   "  setVar('fanpostdelay', +document.all.fandelay.value)\n"
   "}\n"
   "\n"
   "function setIdleMin()\n"
   "{\n"
   "  setVar('idlemin',document.all.idlemin.value)\n";
  client.print(page);
  page = 
   "}\n"
   "\n"
   "function setCycMin()\n"
   "{\n"
   "  setVar('cyclemin',document.all.cycmin.value)\n"
   "}\n"
   "\n"
   "function setCycMax()\n"
   "{\n"
   "  setVar('cyclemax',document.all.cycmax.value)\n"
   "}\n"
   "\n"
   "function setOvrTime()\n"
   "{\n"
   "  setVar('overridetime',document.all.ovrtime.value)\n"
   "}\n"
   "\n"
   "function setOvrTemp()\n"
   "{\n"
   "  setVar('override',(+document.all.ovrtemp.value *10).toFixed())\n"
   "}\n"
   "\n"
   "function cancelOvr()\n"
   "{\n"
   "  setVar('override',0)\n"
   "}\n"
   "\n"
   "function secsToTime(elap)\n"
   "{\n"
   "  d=0\n"
   "  m=0\n"
   "  h=Math.floor(elap/3600)\n"
   "  if(h >23)\n"
   "  {\n"
   "    d=Math.floor(h/24)\n"
   "    h-=(d*24)\n"
   "  }\n"
   "  else\n"
   "  {\n"
   "    m=Math.floor((elap-(h*3600))/60)\n"
   "    s=elap-(h*3600)-(m*60)\n"
   "    if(s<10) s='0'+s\n"
   "    if(h==0)\n"
   "    {\n"
   "      if( m < 10) m='  '+m\n"
   "      return '    '+m +':'+s\n"
   "    }\n"
   "  }\n"
   "  if(m<10) m='0'+m\n"
   "  if(h<10) h='  '+h\n"
   "  if(d) return d+'d '+h+'h'\n"
   "  return h+':'+m+':'+s\n"
   "}\n"
   "//--></script>\n"
   "</head>\n"
   "<body onload=\"{\n"
   "  myStorage1 = localStorage.getItem('myStoredText1')\n"
   "  if(myStorage1!=null){\n"
   "  document.all.myToken.style.visibility = 'hidden'  // hide sensitive data. remove these if irritating\n"
   "  document.all.hide.value='Show'\n"
   "  document.getElementById('myToken').value = myStorage1\n"
   "  readSettings()\n"
   "  }\n"
   "  \n"
   "  myStorage3=localStorage.getItem('myStoredText3')\n"
   "  if(myStorage3!=null){\n"
   "  document.getElementById('ovrtemp').value = myStorage3\n"
   "  }\n"
   "  startEvents()\n"
   "}\">\n"
   "<strong><em>CuriousTech HVAC Remote</em></strong><br>\n"
   "<table style=\"width: 320px\">\n"
   "  <tr>\n"
   "    <td style=\"width: 70px\">Password</td>\n"
   "    <td>\n"
   "<input id=\"myToken\" name=\"access_token\" type=text size=50 placeholder=\"5622ce6bba702ef6bd3456d5ed26aaa4a28d7c9\" style=\"width: 265px\"></td>\n"
   "  </tr>\n"
   "</table>\n"
   "<input type=\"button\" value=\"Save\" onClick=\"{\n"
   "   localStorage.setItem('myStoredText1', document.all.myToken.value)\n";
  client.print(page);
  page = 
   "   alert( document.all.myToken.value + ' Has been stored')\n"
   "}\">\n"
   "<input type=\"button\" value=\"Hide\" name=\"hide\" onClick=\"{\n"
   "  if(document.all.myToken.style.visibility == 'hidden'){\n"
   "    document.all.myToken.style.visibility = 'visible'\n"
   "  document.all.hide.value='Hide'\n"
   "  }else{\n"
   "    document.all.myToken.style.visibility = 'hidden'\n"
   "  document.all.hide.value='Show'\n"
   "    }\n"
   "}\">\n"
   "<input type=\"button\" value=\"Refresh\" onClick=\"{readSettings()}\">\n"
   "<br>\n"
   "<br>\n"
   "<table style=\"width: 350px; height: 22px;\" cellspacing=\"0\">\n"
   "  <tr>\n"
   "    <td class=\"style3\">In</td>\n"
   "    <td class=\"style4\"><div id=\"intemp\" class=\"style2\">in</div></td>\n"
   "    <td class=\"style3\">&deg</td>\n"
   "    <td class=\"style3\">Trg</td>\n"
   "    <td class=\"style4\"><div id=\"target\" class=\"style2\">trg</div></td>\n"
   "    <td class=\"style3\">&deg</td>\n"
   "    <td class=\"style4\"><div id=\"rh\" class=\"style2\">rh</div></td>\n"
   "    <td class=\"style3\">%</td>\n"
   "    <td class=\"style3\">Out</td>\n"
   "    <td class=\"style4\"><div id=\"outtemp\" class=\"style2\">out</div></td>\n"
   "    <td class=\"style3\">&deg</td>\n"
   "  </tr>\n"
   "</table>\n"
   "<br>\n"
   "<table style=\"width: 350px\" class=\"style6\" cellspacing=\"1\" cellpadding=\"0\">\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\" id=\"fanCell\" class=\"style1\"><div id=\"fan\">Fan Off</div></td>\n"
   "    <td colspan=\"2\" style=\"width: 200px\" class=\"style1\">\n"
   "    <input type=\"button\" value=\"Auto\" name=\"fAuto\" onClick=\"{setfan(0)}\">\n"
   "    <input type=\"button\" value=\"On\" name=\"fOn\" onClick=\"{setfan(1)}\">\n"
   "    </td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 44px\" class=\"style1\" id=\"runCell\"><div id=\"run\">off</div></td>\n"
   "    <td colspan=\"2\">\n"
   "    <input type=\"button\" value=\"Off\" name=\"mOff\" onClick=\"{setMode(0)}\">\n"
   "    <input type=\"button\" value=\"Cool\" name=\"mCool\" onClick=\"{setMode(1)}\">\n"
   "    <input type=\"button\" value=\"Heat\" name=\"mHeat\" onClick=\"{setMode(2)}\">\n";
  client.print(page);
  page = 
   "    <input type=\"button\" value=\"Auto\" name=\"mAuto\" onClick=\"{setMode(3)}\">\n"
   "    </td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td></td>\n"
   "    <td colspan=\"2\">\n"
   "    <input type=\"button\" value=\"HP\" name=\"hHP\" onClick=\"{setHeatMode(0)}\">\n"
   "    <input type=\"button\" value=\"Gas\" name=\"hGas\" onClick=\"{setHeatMode(1)}\">\n"
   "    <input type=\"button\" value=\"Auto\" name=\"hAuto\" onClick=\"{setHeatMode(2)}\">\n"
   "    </td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\" class=\"style1\">Cool Hi</td>\n"
   "    <td style=\"width: 44px\" class=\"style1\"><input type=text size=5 value=0 id=\"coolh\"></td>\n"
   "    <td style=\"width: 200px\" class=\"style1\"><input type=\"button\" value=\"Set\" onClick=\"{setCoolHi()}\"><input type=\"button\" value=\"+1\" onClick=\"{incCool(1)}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\" class=\"style1\">Cool Lo</td>\n"
   "    <td style=\"width: 44px\" class=\"style1\"><input type=text size=5 value=0 id=\"cooll\"></td>\n"
   "    <td style=\"width: 200px\" class=\"style1\"><input type=\"button\" value=\"Set\" onClick=\"{setCoolLo()}\"><input type=\"button\" value=\" -1\" onClick=\"{incCool(-1)}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\" class=\"style1\">Heat Hi</td>\n"
   "    <td style=\"width: 44px\" class=\"style1\"><input type=text size=5 value=0 id=\"heath\"></td>\n"
   "    <td style=\"width: 200px\" class=\"style1\"><input type=\"button\" value=\"Set\" onClick=\"{setHeatHi()}\"><input type=\"button\" value=\"+1\" onClick=\"{incHeat(1)}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\" class=\"style1\">Heat Lo</td>\n"
   "    <td style=\"width: 44px\" class=\"style1\"><input type=text size=5 value=0 id=\"heatl\"></td>\n"
   "    <td style=\"width: 200px\" class=\"style1\"><input type=\"button\" value=\"Set\" onClick=\"{setHeatLo()}\"><input type=\"button\" value=\" -1\" onClick=\"{incHeat(-1)}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\" class=\"style1\">Threshold</td>\n"
   "    <td style=\"width: 44px\" class=\"style1\"><input type=text size=5 value=0 id=\"thresh\"></td>\n";
  client.print(page);
  page = 
   "    <td style=\"width: 200px\" class=\"style1\"><input type=\"button\" value=\"Set\" onClick=\"{setThresh()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\" class=\"style1\">Fan Delay</td>\n"
   "    <td style=\"width: 44px\" class=\"style1\"><input type=text size=5 value=0 id=\"fandelay\"></td>\n"
   "    <td style=\"width: 200px\" class=\"style1\"><input type=\"button\" value=\"Set\" onClick=\"{setFanDelay()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\" class=\"style1\">Idle Min</td>\n"
   "    <td style=\"width: 44px\" class=\"style1\"><input type=text size=5 value=0 id=\"idlemin\"></td>\n"
   "    <td style=\"width: 200px\" class=\"style1\"><input type=\"button\" value=\"Set\" onClick=\"{setIdleMin()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\" class=\"style1\">cycle Min</td>\n"
   "    <td style=\"width: 44px\" class=\"style1\"><input type=text size=5 value=0 id=\"cycmin\"></td>\n"
   "    <td style=\"width: 200px\" class=\"style1\"><input type=\"button\" value=\"Set\" onClick=\"{setCycMin()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\" class=\"style1\">cycle Max</td>\n"
   "    <td style=\"width: 44px\" class=\"style1\"><input type=text size=5 value=0 id=\"cycmax\"></td>\n"
   "    <td style=\"width: 200px\" class=\"style1\"><input type=\"button\" value=\"Set\" onClick=\"{setCycMax()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\" class=\"style1\">ovr Time</td>\n"
   "    <td style=\"width: 44px\" class=\"style1\"><input type=text size=5 value=0 id=\"ovrtime\"></td>\n"
   "    <td style=\"width: 200px\" class=\"style1\"><input type=\"button\" value=\"Set\" onClick=\"{setOvrTime()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\" id=\"ovrCell\" class=\"style1\">ovrTemp</td>\n"
   "    <td style=\"width: 44px\" class=\"style1\"><input type=text size=5 value=0 id=\"ovrtemp\"></td>\n"
   "    <td style=\"width: 200px\" class=\"style1\"><input type=\"button\" value=\"Start\" onClick=\"{\n"
   "    localStorage.setItem('myStoredText3', document.all.ovrtemp.value)\n";
  client.print(page);
  page = 
   "    setOvrTemp()}\">\n"
   "    <input type=\"button\" value=\"Cancel\" onClick=\"{cancelOvr()}\">\n"
   "    </td>\n"
   "  </tr>\n"
   "</table>\n"
   "\n"
   "<br>\n"
   "<table style=\"width: 350px\">\n"
   "  <tr>\n"
   "    <td style=\"width: 80px\" class=\"style3\">Cycle</td>\n"
   "    <td style=\"width: 147px\" class=\"style3\">\n"
   "    <div id=\"cyctimer\" style=\"width: 87px\">cycle</div></td>\n"
   "    <td style=\"width: 81px\" class=\"style3\">Total</td>\n"
   "    <td class=\"style3\"><div id=\"runtotal\">total</div></td>\n"
   "  </tr>\n"
   "</table>\n"
   "<small>Copyright (c) 2016 CuriousTech.net</small>\n"
   "</body>\n"
   "</html>\n";
  client.print(page);
}

// Time in hh:mm[:ss][AM/PM]
String timeFmt()
{
  String r = "";
  if(hourFormat12() < 10) r = " ";
  r += hourFormat12();
  r += ":";
  if(minute() < 10) r += "0";
  r += minute();
  r += ":";
  if(second() < 10) r += "0";
  r += second();
  r += " ";
  r += isPM() ? "PM":"AM";
  return r;
}

void handleS() { // standard params, but no page
//  Serial.println("handleS\n");
  parseParams();

  server.send ( 200, "text/html", "OK" );
}

// Return lots of vars as JSON
void handleJson()
{
//  Serial.println("handleJson\n");
  String s = hvac.settingsJson();
  server.send ( 200, "text/json", s + "\n");
}

// event streamer (assume keep-alive) (esp8266 2.1.0 can't handle this)
void handleEvents()
{
  char temp[100];
//  Serial.println("handleEvents");
  uint16_t interval = 60; // default interval
  uint8_t nType = 0;

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
    }
  }

  server.send( 200, "text/event-stream", "" );

  event.set(server.client(), interval, nType);
}

// Pushed data
String dataJson()
{
  return hvac.getPushData();
}

const char *jsonList1[] = { "state", "temp", "rh", "tempi", "rhi", NULL };
const char *jsonList2[] = { "alert", NULL };

void remoteCallback(uint16_t iEvent, uint16_t iName, uint16_t iValue, char *psValue)
{
  switch(iEvent)
  {
    case 0: // state
      switch(iName)
      {
        case 0: // temp
          hvac.m_inTemp = (int)(atof(psValue)*10);
          break;
        case 1: // rh
          hvac.m_rh = (int)(atof(psValue)*10);
          break;
        case 2: // tempi
          hvac.m_inTemp = iValue;
          break;
        case 3: // rhi
          hvac.m_rh = iValue;
          break;
      }
      break;
    case 1: // alert
      display.Note(psValue);
      break;
  }
}

// remote streamer url/ip
void handleRemote()
{
  char temp[100];
  char ip[64];
  char path[64];
  String sKey;
  int nPort;
//  Serial.println("handleRemote");

  ipString(server.client().remoteIP()).toCharArray(ip, 64); // default IP is client

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
//    Serial.println( i + " " + server.argName ( i ) + ": " + s);

    if(server.argName(i) == "ip")
       s.toCharArray(ip, 64);
    else if(server.argName(i) == "path")
       s.toCharArray(path, 64);
    else if(server.argName(i) == "port")
       nPort = s.toInt();
    else if(server.argName(i) == "key")
       sKey = s;
  }

  server.send ( 200, "text/html", "OK" );

  if(sKey != controlPassword)
    return;

  remoteStream.begin(ip, path, nPort, true);
  remoteStream.addList(jsonList1);
  remoteStream.addList(jsonList2);
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
