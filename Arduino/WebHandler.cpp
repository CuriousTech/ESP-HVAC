// Do all the web stuff here

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
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
const char *controlPassword = "password"; // device password for modifying any settings
int serverPort = 85;            // Change to 80 for normal access
const char pbToken[] = "pushbullet token goes here";

//-----------------
ESP8266WebServer server( serverPort );
WiFiManager wifi(0);  // AP page:  192.168.4.1
extern eventHandler event;
extern HVAC hvac;
extern Display display;

void remoteCallback(uint16_t iEvent, uint16_t iName, uint16_t iValue, char *psValue);
JsonClient remoteStream(remoteCallback);

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
  server.on ( "/s", handleS );
  server.on ( "/json", handleJson );
  server.on ( "/events", handleEvents );
  server.on ( "/remote", handleRemote );
  server.onNotFound ( handleNotFound );
  server.begin();
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
}

void handleServer()
{
  MDNS.update();
  server.handleClient();
  hvac.m_bRemoteConnected = remoteStream.service();
  if(hvac.m_bRemoteConnected && hvac.m_bRemoteDisconnect)
  {
    remoteStream.end();
    hvac.m_bRemoteDisconnect = false;
  }
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

  for ( uint8_t i = 0; i < server.args(); i++ ) // password may be at end
  {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);

    if(server.argName(i) == "key")
    {
      password = s;
    }
  }

  for ( uint8_t i = 0; i < server.args(); i++ )
  {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);

    if(server.argName(i) != "key")
    {
      if(nWrongPass == 0 && password == controlPassword)
        hvac.setVar(server.argName(i), s.toInt() );
      display.screen(true); // switch to main page, undim when varabled are changed
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
   ".style5 {background-color: #00D0D0;}\n"
   ".style6 {border-style: solid;border-width: 1px;background-color: #C0C0C0;}\n"
   "</style>\n"
   "\n"
   "<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/1.3.2/jquery.min.js\" type=\"text/javascript\" charset=\"utf-8\"></script>\n"
   "\n"
   "<script type=\"text/javascript\">\n"
   "<!--\n"
   "\n"
   "var Json,mode,autoMode,heatMode,fanMode,running,fan,humidMode,ovrActive\n"
   "var a=document.all\n"
   "var states = new Array('Idle','Cooling','HP Heat','NG Heat')\n"
   "\n"
   "function startEvents()\n"
   "{\n"
   "  eventSource = new EventSource(\"events?i=30&p=1\")\n"
   "  eventSource.addEventListener('open',function(e){},false)\n"
   "  eventSource.addEventListener('error',function(e){},false)\n"
   "  eventSource.addEventListener('state',function(e){\n"
   "    console.log(e)\n"
   "    console.log(e.data)\n"
   "    Json=JSON.parse(e.data)\n"
   "\n"
   "    running= +Json.r\n"
   "    fan= +Json.fr\n"
   "    rh= +Json.h\n"
   "\n"
   "    a.intemp.innerHTML= +Json.it/10\n"
   "    a.rh.innerHTML= +Json.rh/10\n"
   "    a.target.innerHTML= +Json.tt/10\n"
   "    a.outtemp.innerHTML= +Json.ot/10\n"
   "    a.cyctimer.innerHTML=secsToTime(+Json.ct)\n"
   "    a.runtotal.innerHTML=secsToTime(+Json.rt)\n"
   "\n"
   "    a.fan.innerHTML=fan?\"Fan On\":\"Fan Off\"\n"
   "    a.run.innerHTML=states[+Json.s]\n"
   "    a.hm.innerHTML=rh?\"Humid On\":\"Humid Off\"\n"
   "    setAtt()\n"
   "  },false)\n"
   "  eventSource.addEventListener('alert',function(e){\n"
   "    alert(e.data)\n"
   "  },false)\n"
   "}\n"
   "\n"
   "function readSettings()\n"
   "{\n"
   "  $.getJSON(\"json\", function(Json){\n"
   "      mode= +Json.m\n"
   "    autoMode= +Json.am\n"
   "    heatMode= +Json.hm\n"
   "    fanMode= +Json.fm\n"
   "    humidMode= +Json.rhm\n"
   "    ovrActive= +Json.ot\n"
   "    setAtt()\n"
   "\n"
   "    a.cooll.value= +Json.c0/10\n"
   "    a.coolh.value= +Json.c1/10\n"
   "    a.heatl.value= +Json.h0/10\n"
   "    a.heath.value= +Json.h1/10\n"
   "    a.humidl.value= +Json.rh0/10\n"
   "    a.humidh.value= +Json.rh1/10\n"
   "    a.idlemin.value= +Json.im\n"
   "    a.cycmin.value= +Json.cn\n"
   "    a.cycmax.value= +Json.cx\n"
   "    a.thresh.value= +Json.ct/10\n"
   "    a.fandelay.value= +Json.fd\n"
   "    a.ovrtime.value= +Json.ov\n"
   "    a.fanpre.value= +Json.fp\n"
   "  \n"
   "    if( +a.ovrtemp.value==0)\n"
   "      a.ovrtemp.value= -2.0\n"
   "  });\n"
   "}\n"
   "\n"
   "function setVar(varName, value)\n"
   "{\n"
   "  $.get('s?key='+a.myToken.value+'&'+varName+'='+value)\n"
   "}\n"
   "\n"
   "function setfan(n)\n"
   "{\n"
   "  fanMode=n\n"
   "  fan = fanMode?1:run\n"
   "  setVar('fanmode',fanMode?1:0)\n"
   "  setAtt()\n"
   "}\n"
   "\n"
   "function setMode(m)\n"
   "{\n"
   "  setVar('mode',mode=m)\n"
   "  setAtt()\n"
   "}\n"
   "\n"
   "function setHeatMode(m)\n"
   "{\n"
   "  setVar('heatmode',heatMode=m)\n"
   "  setAtt()\n"
   "}\n"
   "\n"
   "function setHumidMode(m)\n"
   "{\n"
   "  setVar('humidmode',humidMode=m)\n"
   "  setAtt()\n"
   "}\n"
   "\n"
   "function setAtt()\n"
   "{\n"
   "  a.runCell.setAttribute('class',running?'style5':'style1')\n"
   "  a.hmCell.setAttribute('class',rh?'style5':'style1')\n"
   "  a.fAuto.setAttribute('class',fanMode==0?'style5':'')\n"
   "  a.fOn.setAttribute('class',fanMode==1?'style5':'')\n"
   "  a.fan.innerHTML = \"Fan \"+(fanMode?\"On\":(fan?\"On\":\"Off\"))\n"
   "  a.fanCell.setAttribute('class',fan?'style5' : 'style1')\n"
   "  a.ovrCell.setAttribute('class',ovrActive?'style5':'style1')\n"
   "\n"
   "  a.mOff.setAttribute('class',mode==0?'style5':'')\n"
   "  a.mCool.setAttribute('class',mode==1?'style5':'')\n"
   "  a.mHeat.setAttribute('class',mode==2?'style5':'')\n"
   "  a.mAuto.setAttribute('class',mode==3?'style5':'')\n"
   "\n"
   "  a.hHP.setAttribute('class',heatMode==0?'style5':'')\n"
   "  a.hGas.setAttribute('class',heatMode==1?'style5':'')\n"
   "  a.hAuto.setAttribute('class',heatMode==2?'style5':'')\n"
   "\n"
   "  a.hmOff.setAttribute('class',humidMode==0?'style5':'')\n"
   "  a.hmHeat.setAttribute('class',humidMode==1?'style5':'')\n"
   "  a.hmCool.setAttribute('class',humidMode==2?'style5':'')\n"
   "  a.hmBoth.setAttribute('class',humidMode==3?'style5':'')\n"
   "  a.hmAuto1.setAttribute('class',humidMode==4?'style5':'')\n"
   "  a.hmAuto2.setAttribute('class',humidMode==5?'style5':'')\n"
   "  a.hmManual.setAttribute('class',humidMode==6?'style5':'')\n"
   "}\n"
   "\n"
   "function setCoolHi()\n"
   "{\n"
   "  setVar('cooltemph',(+a.coolh.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setCoolLo()\n"
   "{\n"
   "  setVar('cooltempl',(+a.cooll.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function incCool(n)\n"
   "{\n"
   "  a.coolh.value= +a.coolh.value+n\n"
   "  a.cooll.value= +a.cooll.value+n\n"
   "\n"
   "  setVar('cooltemph',(+a.coolh.value*10).toFixed())\n"
   "  setVar('cooltempl',(+a.cooll.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setHeatHi()\n"
   "{\n"
   "  setVar('heattemph',(+a.heath.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setHeatLo()\n"
   "{\n"
   "  setVar('heattempl',(+a.heatl.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function incHeat(n)\n"
   "{\n"
   "  a.heath.value= +a.heath.value+n\n"
   "  a.heatl.value= +a.heatl.value+n\n"
   "\n"
   "  setVar('heattemph',(+a.heath.value*10).toFixed())\n"
   "  setVar('heattempl',(+a.heatl.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setHumidHi()\n"
   "{\n"
   "  setVar('humidh',(+a.humidh.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setHumidLo()\n"
   "{\n"
   "  setVar('humidl',(+a.humidl.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function incHumid(n)\n"
   "{\n"
   "  a.humidh.value= +a.humidh.value+n\n"
   "  a.humidl.value= +a.humidl.value+n\n"
   "\n"
   "  setVar('humidh',(+a.humidh.value*10).toFixed())\n"
   "  setVar('humidl',(+a.humidl.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setThresh()\n"
   "{\n"
   "  setVar('cyclethresh',(+a.thresh.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setFanDelay()\n"
   "{\n"
   "  setVar('fanpostdelay',+a.fandelay.value)\n"
   "}\n"
   "\n"
   "function setFanPre()\n"
   "{\n"
   "  setVar('fanpretime',+a.fanpre.value)\n"
   "}\n"
   "\n"
   "function setIdleMin()\n"
   "{\n"
   "  setVar('idlemin',a.idlemin.value)\n"
   "}\n"
   "\n"
   "function setCycMin()\n"
   "{\n"
   "  setVar('cyclemin',a.cycmin.value)\n"
   "}\n"
   "\n"
   "function setCycMax()\n"
   "{\n"
   "  setVar('cyclemax',a.cycmax.value)\n"
   "}\n"
   "\n"
   "function setOvrTime()\n"
   "{\n"
   "  setVar('overridetime',a.ovrtime.value)\n"
   "}\n"
   "\n"
   "function setOvrTemp()\n"
   "{\n"
   "  setVar('override',(+a.ovrtemp.value *10).toFixed())\n"
   "}\n"
   "\n"
   "function cancelOvr()\n"
   "{\n"
   "  setVar('override',0)\n"
   "}\n"
   "\n"
   "function secsToTime( elap )\n"
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
   "  if(myStorage1  != null){\n"
   "  a.myToken.style.visibility = 'hidden'\n"
   "  a.hide.value='Show'\n"
   "  document.getElementById('myToken').value=myStorage1\n"
   "  }\n"
   "  readSettings()\n"
   "  \n"
   "  myStorage3 = localStorage.getItem('myStoredText3')\n"
   "  if(myStorage3  != null){\n"
   "  document.getElementById('ovrtemp').value=myStorage3\n"
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
   "   localStorage.setItem('myStoredText1', a.myToken.value)\n"
   "   alert(a.myToken.value+' Has been stored')\n"
   "}\">\n"
   "<input type=\"button\" value=\"Hide\" name=\"hide\" onClick=\"{\n"
   "  if(a.myToken.style.visibility == 'hidden'){\n"
   "    a.myToken.style.visibility = 'visible'\n"
   "  a.hide.value='Hide'\n"
   "  }else{\n"
   "    a.myToken.style.visibility = 'hidden'\n"
   "  a.hide.value='Show'\n"
   "    }\n"
   "}\">\n"
   "<input type=\"button\" value=\"Refresh\" onClick=\"{readSettings()}\">\n"
   "<br>\n"
   "<br>\n"
   "<font size=4>\n"
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
   "</font>\n"
   "<br>\n"
   "<table style=\"width: 350px\" class=\"style6\" cellspacing=\"1\" cellpadding=\"0\">\n"
   "  <tr>\n"
   "    <td id=\"fanCell\"><div id=\"fan\">Fan Off</div></td>\n"
   "    <td colspan=\"2\" style=\"width: 200px\" class=\"style1\">\n"
   "    <input type=\"button\" value=\"Auto\" name=\"fAuto\" onClick=\"{setfan(0)}\">\n"
   "    <input type=\"button\" value=\"On\" name=\"fOn\" onClick=\"{setfan(1)}\">\n"
   "    </td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td id=\"runCell\"><div id=\"run\">off</div></td>\n"
   "    <td colspan=\"2\">\n"
   "    <input type=\"button\" value=\"Off\" name=\"mOff\" onClick=\"{setMode(0)}\">\n"
   "    <input type=\"button\" value=\"Cool\" name=\"mCool\" onClick=\"{setMode(1)}\">\n"
   "    <input type=\"button\" value=\"Heat\" name=\"mHeat\" onClick=\"{setMode(2)}\">\n"
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
   "    <td id=\"hmCell\"><div id=\"hm\">NA</div></td>\n"
   "    <td colspan=\"2\">\n"
   "    <input type=\"button\" value=\"Off\" name=\"hmOff\" onClick=\"{setHumidMode(0)}\">\n"
   "    <input type=\"button\" value=\"H\" name=\"hmHeat\" onClick=\"{setHumidMode(1)}\">\n"
   "    <input type=\"button\" value=\"C\" name=\"hmCool\" onClick=\"{setHumidMode(2)}\">\n"
   "    <input type=\"button\" value=\"Both\" name=\"hmBoth\" onClick=\"{setHumidMode(3)}\">\n"
   "    <input type=\"button\" value=\"A1\" name=\"hmAuto1\" onClick=\"{setHumidMode(4)}\">\n"
   "    <input type=\"button\" value=\"A2\" name=\"hmAuto2\" onClick=\"{setHumidMode(5)}\">\n"
   "    <input type=\"button\" value=\"Man\" name=\"hmManual\" onClick=\"{setHumidMode(6)}\">\n"
   "    </td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td>Cool Hi</td>\n"
   "    <td><input type=text size=5 value=0 id=\"coolh\"></td>\n"
   "    <td><input type=\"button\" value=\"Set\" onClick=\"{setCoolHi()}\"><input type=\"button\" value=\"+1\" onClick=\"{incCool(1)}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td style=\"width: 81px\">Cool Lo</td>\n"
   "    <td style=\"width: 44px\"><input type=text size=5 value=0 id=\"cooll\"></td>\n"
   "    <td style=\"width: 200px\"><input type=\"button\" value=\"Set\" onClick=\"{setCoolLo()}\"><input type=\"button\" value=\" -1\" onClick=\"{incCool(-1)}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td>Heat Hi</td>\n"
   "    <td><input type=text size=5 value=0 id=\"heath\"></td>\n"
   "    <td><input type=\"button\" value=\"Set\" onClick=\"{setHeatHi()}\"><input type=\"button\" value=\"+1\" onClick=\"{incHeat(1)}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td>Heat Lo</td>\n"
   "    <td><input type=text size=5 value=0 id=\"heatl\"></td>\n"
   "    <td><input type=\"button\" value=\"Set\" onClick=\"{setHeatLo()}\"><input type=\"button\" value=\" -1\" onClick=\"{incHeat(-1)}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td>Threshold</td>\n"
   "    <td><input type=text size=5 value=0 id=\"thresh\"></td>\n"
   "    <td><input type=\"button\" value=\"Set\" onClick=\"{setThresh()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td>Humid Hi</td>\n"
   "    <td><input type=text size=5 value=0 id=\"humidh\"></td>\n"
   "    <td><input type=\"button\" value=\"Set\" onClick=\"{setHumidHi()}\"><input type=\"button\" value=\"+1\" onClick=\"{incHumid(1)}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td>Humid Lo</td>\n"
   "    <td><input type=text size=5 value=0 id=\"humidl\"></td>\n"
   "    <td><input type=\"button\" value=\"Set\" onClick=\"{setHumidLo()}\"><input type=\"button\" value=\" -1\" onClick=\"{incHumid(-1)}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td>Fan Pre</td>\n"
   "    <td><input type=text size=5 value=0 id=\"fanpre\"></td>\n"
   "    <td><input type=\"button\" value=\"Set\" onClick=\"{setFanPre()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td>Fan Post</td>\n"
   "    <td><input type=text size=5 value=0 id=\"fandelay\"></td>\n"
   "    <td><input type=\"button\" value=\"Set\" onClick=\"{setFanDelay()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td>Idle Min</td>\n"
   "    <td><input type=text size=5 value=0 id=\"idlemin\"></td>\n"
   "    <td><input type=\"button\" value=\"Set\" onClick=\"{setIdleMin()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td>cycle Min</td>\n"
   "    <td><input type=text size=5 value=0 id=\"cycmin\"></td>\n"
   "    <td><input type=\"button\" value=\"Set\" onClick=\"{setCycMin()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td>cycle Max</td>\n"
   "    <td><input type=text size=5 value=0 id=\"cycmax\"></td>\n"
   "    <td><input type=\"button\" value=\"Set\" onClick=\"{setCycMax()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td>ovr Time</td>\n"
   "    <td><input type=text size=5 value=0 id=\"ovrtime\"></td>\n"
   "    <td><input type=\"button\" value=\"Set\" onClick=\"{setOvrTime()}\"></td>\n"
   "  </tr>\n"
   "  <tr>\n"
   "    <td id=\"ovrCell\">ovrTemp</td>\n"
   "    <td><input type=text size=5 value=0 id=\"ovrtemp\"></td>\n"
   "    <td><input type=\"button\" value=\"Start\" onClick=\"{\n"
   "    localStorage.setItem('myStoredText3', a.ovrtemp.value)\n"
   "    setOvrTemp()}\">\n"
   "    <input type=\"button\" value=\"Cancel\" onClick=\"{cancelOvr()}\">\n"
   "    </td>\n"
   "  </tr>\n"
   "</table>\n"
   "<table style=\"width: 350px\">\n"
   "  <tr>\n"
   "    <td style=\"width: 80px\" class=\"style3\">Cycle</td>\n"
   "    <td style=\"width: 147px\" class=\"style3\">\n"
   "    <div id=\"cyctimer\" style=\"width: 87px\">cycle</div></td>\n"
   "    <td style=\"width: 81px\" class=\"style3\">Total</td>\n"
   "    <td class=\"style3\"><div id=\"runtotal\">total</div></td>\n"
   "  </tr>\n"
   "</table>\n"
   "<small>Copyright &copy 2016 CuriousTech.net</small>\n"
   "</body>\n"
   "</html>\n";

  server.send ( 200, "text/html", page );
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
  int nPort = 80;
  bool bEnd = false;
//  Serial.println("handleRemote");

  ipString(server.client().remoteIP()).toCharArray(ip, 64); // default host IP is client

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
//    Serial.println( i + " " + server.argName ( i ) + ": " + s);

    if(server.argName(i) == "ip") // optional non-client source
       s.toCharArray(ip, 64);
    else if(server.argName(i) == "path")
       s.toCharArray(path, 64);
    else if(server.argName(i) == "port")
       nPort = s.toInt();
    else if(server.argName(i) == "end")
       bEnd = true;
    else if(server.argName(i) == "key")
       sKey = s;
  }

  server.send ( 200, "text/html", "OK" );

  if(sKey != controlPassword)
    return;

  if(bEnd) // end a remote sensor
  {
    remoteStream.end();
    hvac.m_notif = Note_RemoteOff;
    return;
  }

  remoteStream.begin(ip, path, nPort, true);
  remoteStream.addList(jsonList1);
  remoteStream.addList(jsonList2);
  hvac.m_notif = Note_RemoteOn;
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

void pushBullet(const char *pTitle, const char *pBody)
{
  WiFiClientSecure client;
  const char host[] = "api.pushbullet.com";
  const char url[] = "/v2/pushes";

  if (!client.connect(host, 443))
  {
    event.print("PushBullet connection failed");
    return;
  }

  String data = "{\"type\": \"note\", \"title\": \"";
  data += pTitle;
  data += "\", \"body\": \"";
  data += pBody;
  data += "\"}";

  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
              "Host: " + host + "\r\n" +
              "Content-Type: application/json\r\n" +
              "Access-Token: " + pbToken + "\r\n" +
              "User-Agent: Arduino\r\n" +
              "Content-Length: " + data.length() + "\r\n" + 
              "Connection: close\r\n\r\n" +
              data + "\r\n\r\n");
 
  int i = 0;
  while (client.connected() && ++i < 10)
  {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }
}
