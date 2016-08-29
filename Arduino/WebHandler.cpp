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

const char page1[] PROGMEM = 
   "<!DOCTYPE html>\n"
   "<html>\n"
   "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n"
   "<head>\n"
   "\n"
   "<title>ESP-HVAC</title>\n"
   "<style type=\"text/css\">\n"
   "table,input{\n"
   "border-radius: 5px;\n"
   "box-shadow: 2px 2px 12px #000000;\n"
   "background-image: -moz-linear-gradient(top, #ffffff, #50a0ff);\n"
   "background-image: -ms-linear-gradient(top, #ffffff, #50a0ff);\n"
   "background-image: -o-linear-gradient(top, #ffffff, #50a0ff);\n"
   "background-image: -webkit-linear-gradient(top, #efffff, #50a0ff);\n"
   "background-image: linear-gradient(top, #ffffff, #50a0ff);\n"
   "background-clip: padding-box;\n"
   "}\n"
   ".style1 {border-width: 0;}\n"
   ".style2 {text-align: right;}\n"
   ".style5 {\n"
   "border-radius: 5px;\n"
   "box-shadow: 2px 2px 12px #000000;\n"
   "background-image: -moz-linear-gradient(top, #ff00ff, #ffa0ff);\n"
   "background-image: -ms-linear-gradient(top, #ff00ff, #ffa0ff);\n"
   "background-image: -o-linear-gradient(top, #ff00ff, #ffa0ff);\n"
   "background-image: -webkit-linear-gradient(top, #ff0000, #ffa0a0);\n"
   "background-image: linear-gradient(top, #ff00ff, #ffa0ff);\n"
   "}\n"
   "body{width:340px;display:block;font-family: Arial, Helvetica, sans-serif;}\n"
   "</style>\n"
   "\n"
   "<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/1.3.2/jquery.min.js\" type=\"text/javascript\" charset=\"utf-8\"></script>\n"
   "\n"
   "<script type=\"text/javascript\">\n"
   "<!--\n"
   "\n"
   "var Json,mode,autoMode,heatMode,fanMode,running,fan,humidMode,ovrActive,away,rh\n"
   "var a=document.all\n"
   "var states = new Array('Idle','Cooling','HP Heat','NG Heat')\n"
   "\n"
   "function startEvents()\n"
   "{\n"
   "ev= new EventSource(\"events?i=30&p=1\")\n"
   "ev.addEventListener('open',function(e){},false)\n"
   "ev.addEventListener('error',function(e){},false)\n"
   "ev.addEventListener('state',function(e){\n"
   "console.log(e)\n"
   "console.log(e.data)\n"
   "Json=JSON.parse(e.data)\n"
   "running= +Json.r\n"
   "fan= +Json.fr\n"
   "rh= +Json.h\n"
   "away=+Json.aw\n"
   "a.intemp.innerHTML= +Json.it/10\n"
   "a.rh.innerHTML= +Json.rh/10\n"
   "a.target.innerHTML= +Json.tt/10\n"
   "a.outtemp.innerHTML= +Json.ot/10\n"
   "a.cyctimer.innerHTML=secsToTime(+Json.ct)\n"
   "a.runtotal.value=secsToTime(+Json.rt)\n"
   "a.filter.value=s2t(+Json.fm)\n"
   "a.fan.innerHTML=fan?\"Fan On\":\"Fan Off\"\n"
   "a.run.innerHTML=states[+Json.s]\n"
   "a.hm.innerHTML=rh?\"Humidifier On\":\"Humidifier Off\"\n"
   "setAtt()\n"
   "},false)\n"
   "ev.addEventListener('alert',function(e){\n"
   "alert(e.data)\n"
   "},false)\n"
   "}\n"
   "\n"
   "function readSettings()\n"
   "{\n"
   "$.getJSON(\"json\", function(Json){\n"
   "     mode= +Json.m\n"
   "  autoMode= +Json.am\n"
   "  heatMode= +Json.hm\n"
   "  fanMode= +Json.fm\n"
   "  humidMode= +Json.rhm\n"
   "  ovrActive= +Json.ot\n"
   "  setAtt()\n"
   "  a.cooll.value= +Json.c0/10\n"
   "  a.coolh.value= +Json.c1/10\n"
   "  a.heatl.value= +Json.h0/10\n"
   "  a.heath.value= +Json.h1/10\n"
   "  a.ovrtime.value= s2t(+Json.ov)\n"
   "  a.fantime.value= s2t(+Json.fct)\n"
   "  a.awaytemp.value= +Json.ad/10\n"
   " if( +a.ovrtemp.value==0)\n"
   "  a.ovrtemp.value= -2.0\n"
   "});\n"
   "}\n"
   "\n"
   "function setVar(varName, value)\n"
   "{\n"
   "$.get('s?key='+a.myToken.value+'&'+varName+'='+value)\n"
   "}\n"
   "\n"
   "function setfan(n)\n"
   "{\n"
   "if(n<3) fanMode=n\n"
   "setVar('fanmode',n)\n"
   "setAtt()\n"
   "}\n"
   "\n"
   "function setMode(m)\n"
   "{\n"
   "setVar('mode',mode=m)\n"
   "setAtt()\n"
   "}\n"
   "\n"
   "function setHeatMode(m)\n"
   "{\n"
   "setVar('heatmode',heatMode=m)\n"
   "setAtt()\n"
   "}\n"
   "\n"
   "function setHumidMode(m)\n"
   "{\n"
   "setVar('humidmode',humidMode=m)\n"
   "setAtt()\n"
   "}\n"
   "\n"
   "function setAway()\n"
   "{\n"
   "away=!away\n"
   "setVar('away',away?1:0)\n"
   "setAtt()\n"
   "}\n"
   "\n"
   "function rstFlt()\n"
   "{\n"
   "setVar('resetfilter',0)\n"
   "}\n"
   "\n"
   "function rstTot()\n"
   "{\n"
   "setVar('resettotal',0)\n"
   "}\n"
   "\n"
   "function setAtt()\n"
   "{\n"
   "a.runCell.setAttribute('class',running?'style5':'style1')\n"
   "a.hmCell.setAttribute('class',rh?'style5':'style1')\n"
   "a.fAuto.setAttribute('class',fanMode==0?'style5':'')\n"
   "a.fOn.setAttribute('class',fanMode==1?'style5':'')\n"
   "a.fS.setAttribute('class',fanMode==2?'style5':'')\n"
   "a.fan.innerHTML = \"Fan \"+((fanMode==1)?\"On\":(fan?\"On\":\"Off\"))\n"
   "a.fanCell.setAttribute('class',fan?'style5' : 'style1')\n"
   "a.ovrCell.setAttribute('class',ovrActive?'style5':'style1')\n"
   "\n"
   "a.mOff.setAttribute('class',mode==0?'style5':'')\n"
   "a.mCool.setAttribute('class',mode==1?'style5':'')\n"
   "a.mHeat.setAttribute('class',mode==2?'style5':'')\n"
   "a.mAuto.setAttribute('class',mode==3?'style5':'')\n"
   "\n"
   "a.hHP.setAttribute('class',heatMode==0?'style5':'')\n"
   "a.hGas.setAttribute('class',heatMode==1?'style5':'')\n"
   "a.hAuto.setAttribute('class',heatMode==2?'style5':'')\n"
   "\n"
   "a.hmOff.setAttribute('class',humidMode==0?'style5':'')\n"
   "a.hmFan.setAttribute('class',humidMode==1?'style5':'')\n"
   "a.hmRun.setAttribute('class',humidMode==2?'style5':'')\n"
   "a.hmAuto1.setAttribute('class',humidMode==3?'style5':'')\n"
   "a.hmAuto2.setAttribute('class',humidMode==4?'style5':'')\n"
   "a.away.setAttribute('class',away?'style5':'')\n"
   "}\n"
   "\n"
   "function setCoolHi()\n"
   "{\n"
   "setVar('cooltemph',(+a.coolh.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setCoolLo()\n"
   "{\n"
   "setVar('cooltempl',(+a.cooll.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function incCool(n)\n"
   "{\n"
   "a.coolh.value= +a.coolh.value+n\n"
   "a.cooll.value= +a.cooll.value+n\n"
   "\n"
   "setVar('cooltemph',(+a.coolh.value*10).toFixed())\n"
   "setVar('cooltempl',(+a.cooll.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setHeatHi()\n"
   "{\n"
   "setVar('heattemph',(+a.heath.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setHeatLo()\n"
   "{\n"
   "setVar('heattempl',(+a.heatl.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function incHeat(n)\n"
   "{\n"
   "a.heath.value= +a.heath.value+n\n"
   "a.heatl.value= +a.heatl.value+n\n"
   "\n"
   "setVar('heattemph',(+a.heath.value*10).toFixed())\n"
   "setVar('heattempl',(+a.heatl.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setHumidHi()\n"
   "{\n"
   "setVar('humidh',(+a.humidh.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setHumidLo()\n"
   "{\n"
   "setVar('humidl',(+a.humidl.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function incHumid(n)\n"
   "{\n"
   "a.humidh.value= +a.humidh.value+n\n"
   "a.humidl.value= +a.humidl.value+n\n"
   "\n"
   "setVar('humidh',(+a.humidh.value*10).toFixed())\n"
   "setVar('humidl',(+a.humidl.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setOvrTime()\n"
   "{\n"
   "  setVar('overridetime',t2s(a.ovrtime.value))\n"
   "}\n"
   "\n"
   "function setOvrTemp()\n"
   "{\n"
   "  setVar('override',(+a.ovrtemp.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function cancelOvr()\n"
   "{\n"
   "  setVar('override',0)\n"
   "}\n"
   "\n"
   "function setFanTime()\n"
   "{\n"
   "  setVar('fantime',t2s(a.fantime.value))\n"
   "}\n"
   "\n"
   "function setAwayTime()\n"
   "{\n"
   "  setVar('awaytime',t2s(a.awaytime.value))\n"
   "}\n"
   "\n"
   "function setAwayTemp()\n"
   "{\n"
   "  setVar('awaydelta',(+a.awaytemp.value*10).toFixed())\n"
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
   "\n"
   "function s2t(elap)\n"
   "{\n"
   "  m=Math.floor(elap/60)\n"
   "  s=elap-(m*60)\n"
   "  if(m==0) return s\n"
   "  if(s<10) s='0'+s\n"
   "  return m+':'+s\n"
   "}\n"
   "\n"
   "function t2s(v)\n"
   "{\n"
   "  if(typeof v == 'string') v = (+v.substr(0, v.indexOf(':'))*60) + (+v.substr(v.indexOf(':')+1))\n"
   "  return v\n"
   "}\n"
   "//--></script>\n"
   "</head>\n"
   "<body onload=\"{\n"
   " myStorage1 = localStorage.getItem('myStoredText1')\n"
   " if(myStorage1  != null){\n"
   "  a.myToken.style.visibility = 'hidden'\n"
   "  a.hide.value='Show'\n"
   "  document.getElementById('myToken').value=myStorage1\n"
   " }\n"
   " readSettings()\n"
   "  \n"
   " myStorage3 = localStorage.getItem('myStoredText3')\n"
   " if(myStorage3  != null){\n"
   "  document.getElementById('ovrtemp').value=myStorage3\n"
   " }\n"
   " startEvents()\n"
   "}\">\n"
   "<strong><em>CuriousTech HVAC Remote</em></strong><br>\n"
   "<font size=4>\n"
   "<p><table style=\"width: 350px; height: 22px;\" cellspacing=\"0\">\n"
   "<tr>\n"
   "<td>In</td><td><div id=\"intemp\" class=\"style2\">in</div></td><td>&deg</td><td> &gt;</td>\n"
   "<td><div id=\"target\" class=\"style2\">trg</div></td><td>&deg</td>\n"
   "<td><div id=\"rh\" class=\"style2\">rh</div></td><td>%</td>\n"
   "<td>Out</td><td><div id=\"outtemp\" class=\"style2\">out</div></td><td>&deg</td>\n"
   "</tr>\n"
   "</table>\n"
   "</font></p>\n"
   "<table style=\"width: 350px\" cellspacing=\"0\" cellpadding=\"0\">\n"
   "<tr>\n"
   "<td id=\"fanCell\"><div id=\"fan\">Fan Off</div></td>\n"
   "<td align=\"right\"><input type=\"button\" value=\"Auto\" name=\"fAuto\" onClick=\"{setfan(0)}\"></td>\n"
   "<td width=\"40\"><input type=\"button\" value=\" On \" name=\"fOn\" onClick=\"{setfan(1)}\"><input type=\"button\" value=\"Cycl\" name=\"fS\" onClick=\"{setfan(2)}\"></td>\n"
   "<td width=300 align=\"right\"><input type=\"submit\" value=\"Settings\" onClick=\"window.location='/settings';\"></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td id=\"runCell\"><div id=\"run\">Cooling</div></td>\n"
   "<td align=\"right\"><input type=\"button\" value=\" Off \" name=\"mOff\" onClick=\"{setMode(0)}\"></td>\n"
   "<td><input type=\"button\" value=\"Cool\" name=\"mCool\" onClick=\"{setMode(1)}\"><input type=\"button\" value=\"Heat\" name=\"mHeat\" onClick=\"{setMode(2)}\"></td>\n"
   "<td>\n"
   "<input type=\"button\" value=\"Auto\" name=\"mAuto\" onClick=\"{setMode(3)}\"></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>&nbsp</td>\n"
   "<td></td>\n"
   "<td></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Cool Hi</td>\n"
   "<td><input type=text size=3 id=\"coolh\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setCoolHi()}\"><input type=\"button\" value=\"+1\" onClick=\"{incCool(1)}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td style=\"width: 81px\">Cool Lo</td>\n"
   "<td style=\"width: 44px\"><input type=text size=3 id=\"cooll\"></td>\n"
   "<td style=\"width: 200px\"><input type=\"button\" value=\"Set\" onClick=\"{setCoolLo()}\"><input type=\"button\" value=\" -1\" onClick=\"{incCool(-1)}\"></td>\n"
   "<td><input type=\"button\" value=\" HP \" name=\"hHP\" onClick=\"{setHeatMode(0)}\"><input type=\"button\" value=\"Gas \" name=\"hGas\" onClick=\"{setHeatMode(1)}\"><input type=\"button\" value=\"Auto\" name=\"hAuto\" onClick=\"{setHeatMode(2)}\"></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Heat Hi</td>\n"
   "<td><input type=text size=3 id=\"heath\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setHeatHi()}\"><input type=\"button\" value=\"+1\" onClick=\"{incHeat(1)}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Heat Lo</td>\n"
   "<td><input type=text size=3 id=\"heatl\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setHeatLo()}\"><input type=\"button\" value=\" -1\" onClick=\"{incHeat(-1)}\"></td>\n"
   "<td id=\"hmCell\"><div id=\"hm\">Humidifier Off</div></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td></td>\n"
   "<td></td>\n"
   "<td></td>\n"
   "<td>\n"
   "<input type=\"button\" value=\" Off \" name=\"hmOff\" onClick=\"{setHumidMode(0)}\"><input type=\"button\" value=\"Fan\" name=\"hmFan\" onClick=\"{setHumidMode(1)}\"></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>ovr Time</td>\n"
   "<td><input type=text size=3 id=\"ovrtime\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setOvrTime()}\"></td>\n"
   "<td>\n"
   "<input type=\"button\" value=\"Run\" name=\"hmRun\" onClick=\"{setHumidMode(2)}\"></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td id=\"ovrCell\">Overrd &Delta;</td>\n"
   "<td><input type=text size=3 id=\"ovrtemp\"></td>\n"
   "<td><input type=\"button\" value=\"Go \" onClick=\"{localStorage.setItem('myStoredText3', a.ovrtemp.value);setOvrTemp()}\"><input type=\"button\" value=\"Stop\" onClick=\"{cancelOvr()}\">\n"
   "</td>\n"
   "<td>\n"
   "<input type=\"button\" value=\" A1 \" name=\"hmAuto1\" onClick=\"{setHumidMode(3)}\"><input type=\"button\" value=\" A2 \" name=\"hmAuto2\" onClick=\"{setHumidMode(4)}\"></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Freshen</td>\n"
   "<td><input type=text size=3 id=\"fantime\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setFanTime()}\"><input type=\"button\" value=\" Go  \" onClick=\"{setfan(3)}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Away &Delta;</td>\n"
   "<td><input type=text size=3 id=\"awaytemp\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setAwayTemp()}\"></td>\n"
   "<td><input type=\"button\" value=\"Away\" name=\"away\" onClick=\"{setAway()}\"></td>\n"
   "</tr>\n"
   "</table><br/>\n"
   "<table style=\"width: 350px\">\n"
   "<tr>\n"
   "<td>Cycle</td><td><div id=\"cyctimer\" style=\"width: 70px\">0</div></td>\n"
   "<td>Total</td><td><input type=\"button\" id =\"runtotal\" value=\"0\" onClick=\"{rstTot()}\"></td>\n"
   "<td>Filter</td><td><input type=\"button\" id =\"filter\" value=\"0\" onClick=\"{rstFlt()}\"></td>\n"
   "</tr>\n"
   "</table>\n"
   "<p>\n"
   "<table style=\"width: 350px\">\n"
   "<tr>\n"
   "<td>Password</td>\n"
   "<td>\n"
   "<input id=\"myToken\" name=\"access_token\" type=text size=40 placeholder=\"e6bba7456a7c9\" style=\"width: 150px\">\n"
   "</td><td>\n"
   "<input type=\"button\" value=\"Save\" onClick=\"{\n"
   " localStorage.setItem('myStoredText1', a.myToken.value)\n"
   " alert(a.myToken.value+' Has been stored')\n"
   "}\">\n"
   "<input type=\"button\" value=\"Hide\" name=\"hide\" onClick=\"{\n"
   "if(a.myToken.style.visibility == 'hidden'){\n"
   " a.myToken.style.visibility = 'visible'\n"
   " a.hide.value='Hide'\n"
   " }else{\n"
   " a.myToken.style.visibility = 'hidden'\n"
   " a.hide.value='Show'\n"
   "}\n"
   "}\">\n"
   "</td>\n"
   "</tr>\n"
   "</table></p>\n"
   "<small>Copyright &copy 2016 CuriousTech.net</small>\n"
   "</body>\n"
   "</html>\n";

const char page2[] PROGMEM = 
   "<!DOCTYPE html>\n"
   "<html>\n"
   "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n"
   "<head>\n"
   "\n"
   "<title>ESP-HVAC</title>\n"
   "<style type=\"text/css\">\n"
   "table,input{\n"
   "border-radius: 5px;\n"
   "box-shadow: 2px 2px 12px #000000;\n"
   "background-image: -moz-linear-gradient(top, #ffffff, #50a0ff);\n"
   "background-image: -ms-linear-gradient(top, #ffffff, #50a0ff);\n"
   "background-image: -o-linear-gradient(top, #ffffff, #50a0ff);\n"
   "background-image: -webkit-linear-gradient(top, #efffff, #50a0ff);\n"
   "background-image: linear-gradient(top, #ffffff, #50a0ff);\n"
   "background-clip: padding-box;\n"
   "}\n"
   ".style1 {border-width: 0;}\n"
   ".style2 {text-align: right;}\n"
   ".style5 {\n"
   "border-radius: 5px;\n"
   "box-shadow: 2px 2px 12px #000000;\n"
   "background-image: -moz-linear-gradient(top, #ff00ff, #ffa0ff);\n"
   "background-image: -ms-linear-gradient(top, #ff00ff, #ffa0ff);\n"
   "background-image: -o-linear-gradient(top, #ff00ff, #ffa0ff);\n"
   "background-image: -webkit-linear-gradient(top, #ff0000, #ffa0a0);\n"
   "background-image: linear-gradient(top, #ff00ff, #ffa0ff);\n"
   "}\n"
   "body{width:340px;display:block;font-family: Arial, Helvetica, sans-serif;}\n"
   "</style>\n"
   "\n"
   "<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/1.3.2/jquery.min.js\" type=\"text/javascript\" charset=\"utf-8\"></script>\n"
   "\n"
   "<script type=\"text/javascript\">\n"
   "<!--\n"
   "\n"
   "var Json,ovrActive,away,rmtMode\n"
   "var a=document.all\n"
   "var states = new Array('Idle','Cooling','HP Heat','NG Heat')\n"
   "\n"
   "function startEvents()\n"
   "{\n"
   "ev= new EventSource(\"events?i=30&p=1\")\n"
   "ev.addEventListener('open',function(e){},false)\n"
   "ev.addEventListener('error',function(e){},false)\n"
   "ev.addEventListener('state',function(e){\n"
   "console.log(e)\n"
   "console.log(e.data)\n"
   "Json=JSON.parse(e.data)\n"
   "away=+Json.aw\n"
   "setAtt()\n"
   "},false)\n"
   "ev.addEventListener('alert',function(e){\n"
   "alert(e.data)\n"
   "},false)\n"
   "}\n"
   "\n"
   "function readSettings()\n"
   "{\n"
   "$.getJSON(\"json\", function(Json){\n"
   "  setAtt()\n"
   "  a.humidl.value= +Json.rh0/10\n"
   "  a.humidh.value= +Json.rh1/10\n"
   "  a.idlemin.value= s2t(+Json.im)\n"
   "  a.cycmin.value= s2t(+Json.cn)\n"
   "  a.cycmax.value= s2t(+Json.cx)\n"
   "  a.thresh.value= +Json.ct/10\n"
   "  a.fandelay.value= s2t(+Json.fd)\n"
   "  a.fanpre.value= s2t(+Json.fp)\n"
   "  a.awaytime.value= s2t(+Json.at)\n"
   "  a.heatthr.value= +Json.ht/10\n"
   "  rmtMode=+Json.ar\n"
   "});\n"
   "}\n"
   "\n"
   "function setVar(varName, value)\n"
   "{\n"
   "$.get('s?key='+a.myToken.value+'&'+varName+'='+value)\n"
   "}\n"
   "\n"
   "function setAway()\n"
   "{\n"
   "away=!away\n"
   "setVar('away',away?1:0)\n"
   "setAtt()\n"
   "}\n"
   "\n"
   "function setAtt()\n"
   "{\n"
   "a.rmth1.setAttribute('class',(rmtMode&10)==8?'style5':'')\n"
   "a.rmth2.setAttribute('class',(rmtMode&10)==10?'style5':'')\n"
   "a.rmth3.setAttribute('class',(rmtMode&10)==2?'style5':'')\n"
   "a.rmtl1.setAttribute('class',(rmtMode&5)==4?'style5':'')\n"
   "a.rmtl2.setAttribute('class',(rmtMode&5)==5?'style5':'')\n"
   "a.rmtl3.setAttribute('class',(rmtMode&5)==1?'style5':'')\n"
   "}\n"
   "\n"
   "function setHT()\n"
   "{\n"
   "setVar('eheatthresh',(+a.heatthr.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setHumidHi()\n"
   "{\n"
   "setVar('humidh',(+a.humidh.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setHumidLo()\n"
   "{\n"
   "setVar('humidl',(+a.humidl.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function incHumid(n)\n"
   "{\n"
   "a.humidh.value= +a.humidh.value+n\n"
   "a.humidl.value= +a.humidl.value+n\n"
   "\n"
   "setVar('humidh',(+a.humidh.value*10).toFixed())\n"
   "setVar('humidl',(+a.humidl.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setThresh()\n"
   "{\n"
   "setVar('cyclethresh',(+a.thresh.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setFanDelay()\n"
   "{\n"
   "setVar('fanpostdelay',t2s(a.fandelay.value))\n"
   "}\n"
   "\n"
   "function setFanPre()\n"
   "{\n"
   "setVar('fanpretime',t2s(a.fanpre.value))\n"
   "}\n"
   "\n"
   "function setIdleMin()\n"
   "{\n"
   "setVar('idlemin',t2s(a.idlemin.value))\n"
   "}\n"
   "\n"
   "function setCycMin()\n"
   "{\n"
   "setVar('cyclemin',t2s(a.cycmin.value))\n"
   "}\n"
   "\n"
   "function setCycMax()\n"
   "{\n"
   "setVar('cyclemax',t2s(a.cycmax.value))\n"
   "}\n"
   "\n"
   "function setOvrTime()\n"
   "{\n"
   "  setVar('overridetime',t2s(a.ovrtime.value))\n"
   "}\n"
   "\n"
   "function setOvrTemp()\n"
   "{\n"
   "  setVar('override',(+a.ovrtemp.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function cancelOvr()\n"
   "{\n"
   "  setVar('override',0)\n"
   "}\n"
   "\n"
   "function setFanTime()\n"
   "{\n"
   "  setVar('fantime',t2s(a.fantime.value))\n"
   "}\n"
   "\n"
   "function setAwayTime()\n"
   "{\n"
   "  setVar('awaytime',t2s(a.awaytime.value))\n"
   "}\n"
   "\n"
   "function setAwayTemp()\n"
   "{\n"
   "  setVar('awaydelta',(+a.awaytemp.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setRmt(v)\n"
   "{\n"
   "  switch(v)\n"
   "  {\n"
   "    case 1: rmtMode&=0xFD;rmtMode|=8;break;\n"
   "    case 2: rmtMode|=10;break;\n"
   "    case 3: rmtMode&=0xF7;rmtMode|=2;break;\n"
   "    case 4: rmtMode&=0xFE;rmtMode|=4;break;\n"
   "    case 5: rmtMode|=5;break;\n"
   "    case 6: rmtMode&=0xFB;rmtMode|=1;break;\n"
   "  }\n"
   "  setVar('rmtflgs',rmtMode)\n"
   "  setAtt()\n"
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
   "\n"
   "function s2t(elap)\n"
   "{\n"
   "  m=Math.floor(elap/60)\n"
   "  s=elap-(m*60)\n"
   "  if(m==0) return s\n"
   "  if(s<10) s='0'+s\n"
   "  return m+':'+s\n"
   "}\n"
   "\n"
   "function t2s(v)\n"
   "{\n"
   "  if(typeof v == 'string') v = (+v.substr(0, v.indexOf(':'))*60) + (+v.substr(v.indexOf(':')+1))\n"
   "  return v\n"
   "}\n"
   "//--></script>\n"
   "</head>\n"
   "<body onload=\"{\n"
   " myStorage1 = localStorage.getItem('myStoredText1')\n"
   " if(myStorage1  != null){\n"
   "  a.myToken.style.visibility = 'hidden'\n"
   "  a.hide.value='Show'\n"
   "  document.getElementById('myToken').value=myStorage1\n"
   " }\n"
   " readSettings()\n"
   "  \n"
   " myStorage3 = localStorage.getItem('myStoredText3')\n"
   " if(myStorage3  != null){\n"
   "  document.getElementById('ovrtemp').value=myStorage3\n"
   " }\n"
   " startEvents()\n"
   "}\">\n"
   "<strong><em>CuriousTech HVAC Remot Settings</em></strong><br><br>\n"
   "<table style=\"width: 350px\" cellspacing=\"0\" cellpadding=\"0\">\n"
   "<tr>\n"
   "<td style=\"width: 81px\">Threshold</td>\n"
   "<td style=\"width: 44px\"><input type=text size=4 id=\"thresh\"></td>\n"
   "<td style=\"width: 140px\"><input type=\"button\" value=\"Set\" onClick=\"{setThresh()}\"></td>\n"
   "<td>\n"
   "<input type=\"submit\" value=\"Main\" onClick=\"window.location='/';\">\n"
   "</td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Heat Thr</td>\n"
   "<td><input type=text size=4 id=\"heatthr\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setHT()}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Humid Hi</td>\n"
   "<td><input type=text size=4 id=\"humidh\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setHumidHi()}\"><input type=\"button\" value=\"+1\" onClick=\"{incHumid(1)}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Humid Lo</td>\n"
   "<td><input type=text size=4 id=\"humidl\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setHumidLo()}\"><input type=\"button\" value=\" -1\" onClick=\"{incHumid(-1)}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Pre Fan</td>\n"
   "<td><input type=text size=4 id=\"fanpre\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setFanPre()}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Post Fan</td>\n"
   "<td><input type=text size=4 id=\"fandelay\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setFanDelay()}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Idle Min</td>\n"
   "<td><input type=text size=4 id=\"idlemin\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setIdleMin()}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>cycle Min</td>\n"
   "<td><input type=text size=4 id=\"cycmin\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setCycMin()}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>cycle Max</td>\n"
   "<td><input type=text size=4 id=\"cycmax\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setCycMax()}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Away Lmt</td>\n"
   "<td><input type=text size=4 id=\"awaytime\"></td>\n"
   "<td><input type=\"button\" value=\"Set\" onClick=\"{setAwayTime()}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td></td>\n"
   "<td></td>\n"
   "<td></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Remote Hi</td>\n"
   "<td><input type=\"button\" value=\"Remote\" name=\"rmth1\" onClick=\"{setRmt(1)}\"></td>\n"
   "<td><input type=\"button\" value=\"Avg\" name=\"rmth2\" onClick=\"{setRmt(2)}\"><input type=\"button\" value=\"Main\" name=\"rmth3\" onClick=\"{setRmt(3)}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Remote Lo</td>\n"
   "<td><input type=\"button\" value=\"Remote\" name=\"rmtl1\" onClick=\"{setRmt(4)}\"></td>\n"
   "<td><input type=\"button\" value=\"Avg\" name=\"rmtl2\" onClick=\"{setRmt(5)}\"><input type=\"button\" value=\"Main\" name=\"rmtl3\" onClick=\"{setRmt(6)}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "</table>\n"
   "<p>\n"
   "<table style=\"width: 350px\">\n"
   "<tr>\n"
   "<td>Password</td>\n"
   "<td>\n"
   "<input id=\"myToken\" name=\"access_token\" type=text size=40 placeholder=\"e6bba7456a7c9\" style=\"width: 150px\">\n"
   "<input type=\"button\" value=\"Save\" onClick=\"{\n"
   " localStorage.setItem('myStoredText1', a.myToken.value)\n"
   " alert(a.myToken.value+' Has been stored')\n"
   "}\">\n"
   "<input type=\"button\" value=\"Hide\" name=\"hide\" onClick=\"{\n"
   "if(a.myToken.style.visibility == 'hidden'){\n"
   " a.myToken.style.visibility = 'visible'\n"
   " a.hide.value='Hide'\n"
   " }else{\n"
   " a.myToken.style.visibility = 'hidden'\n"
   " a.hide.value='Show'\n"
   "}\n"
   "}\">\n"
   "</td>\n"
   "</tr>\n"
   "</table></p>\n"
   "<small>Copyright &copy 2016 CuriousTech.net</small>\n"
   "</body>\n"
   "</html>\n";

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
