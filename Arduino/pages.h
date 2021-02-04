const char page_index[] PROGMEM = 
   "<!DOCTYPE html>\n"
   "<html>\n"
   "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n"
   "<head>\n"
   "\n"
   "<title>ESP-HVAC</title>\n"
   "<style type=\"text/css\">\n"
   "table{\n"
   "border-radius: 3px;\n"
   "box-shadow: 2px 2px 8px #000000;\n"
   "background-image: -moz-linear-gradient(top, #efbfbf, #50a0ff);\n"
   "background-image: -ms-linear-gradient(top, #efbfbf, #50a0ff);\n"
   "background-image: -o-linear-gradient(top, #efbfbf, #50a0ff);\n"
   "background-image: -webkit-linear-gradient(top, #efbfbf, #50a0ff);\n"
   "background-image: linear-gradient(top, #efbfbf, #50a0ff);\n"
   "background-clip: padding-box;\n"
   "}\n"
   "input{\n"
   "border-radius: 5px;\n"
   "box-shadow: 3px 3px 10px #000000;\n"
   "background-image: -moz-linear-gradient(top, #00ffff, #50a0ff);\n"
   "background-image: -ms-linear-gradient(top, #00ffff, #50a0ff);\n"
   "background-image: -o-linear-gradient(top, #00ffff, #50a0ff);\n"
   "background-image: -webkit-linear-gradient(top, #00ffff, #50a0ff);\n"
   "background-image: linear-gradient(top, #00ffff, #50a0ff);\n"
   "background-clip: padding-box;\n"
   "}\n"
   ".style1{border-width: 0;}\n"
   ".style2{text-align: right;}\n"
   ".style5{\n"
   "border-radius: 4px;\n"
   "box-shadow: 3px 3px 10px #000000;\n"
   "background-image: -moz-linear-gradient(top, #ff0000, #ffa0a0);\n"
   "background-image: -ms-linear-gradient(top, #ff0000, #ffa0a0);\n"
   "background-image: -o-linear-gradient(top, #ff0000, #ffa0a0);\n"
   "background-image: -webkit-linear-gradient(top, #ff0000, #ffa0a0);\n"
   "background-image: linear-gradient(top, #ff0000, #ffa0a0);\n"
   "}\n"
   "body{width:340px;display:block;font-family: Arial, Helvetica, sans-serif;}\n"
   "</style>\n"
   "\n"
   "<script type=\"text/javascript\"><!--\n"
   "var Json,mode,autoMode,heatMode,fanMode,running,fan,humidMode,ovrActive,away,rh\n"
   "var a=document.all\n"
   "var states = new Array('Idle','Cooling','HP Heat','NG Heat')\n"
   "var ws\n"
   "var myToken = localStorage.getItem('myStoredText1')\n"
   "function startEvents()\n"
   "{\n"
   "ws = new WebSocket(\"ws://\"+window.location.host+\"/ws\")\n"
   "//ws = new WebSocket(\"ws://192.168.31.125/ws\")\n"
   "ws.onopen = function(evt) { }\n"
   "ws.onclose = function(evt) { alert(\"Connection closed.\"); }\n"
   "\n"
   "ws.onmessage = function(evt) {\n"
   " lines = evt.data.split(';')\n"
   " event=lines[0]\n"
   " data=lines[1]\n"
   " if(event == 'settings')\n"
   " {\n"
   "Json=JSON.parse(data)\n"
   "   mode= +Json.m\n"
   "autoMode= +Json.am\n"
   "heatMode= +Json.hm\n"
   "fanMode= +Json.fm\n"
   "humidMode= +Json.rhm\n"
   "ovrActive= +Json.ot\n"
   "setAtt()\n"
   "a.cooll.value= +Json.c0/10\n"
   "a.coolh.value= +Json.c1/10\n"
   "a.heatl.value= +Json.h0/10\n"
   "a.heath.value= +Json.h1/10\n"
   "a.ovrtime.value= s2t(+Json.ov)\n"
   "a.fantime.value= s2t(+Json.fct)\n"
   "a.awaytemp.value= +Json.ad/10\n"
   "  if( +a.ovrtemp.value==0)\n"
   " a.ovrtemp.value= -2.0\n"
   " }\n"
   " else if(event == 'state')\n"
   " {\n"
   "Json=JSON.parse(data)\n"
   "running= +Json.r\n"
   "fan= +Json.fr\n"
   "rh= +Json.h\n"
   "away=+Json.aw\n"
   "a.time.innerHTML=(new Date(+Json.t*1000)).toLocaleTimeString()\n"
   "a.intemp.innerHTML= (+Json.it/10).toFixed(1)\n"
   "a.rh.innerHTML= (+Json.rh/10).toFixed(1)\n"
   "a.target.innerHTML= (+Json.tt/10).toFixed(1)\n"
   "a.outtemp.innerHTML= (+Json.ot/10).toFixed(1)\n"
   "a.cyctimer.innerHTML=secsToTime(+Json.ct)\n"
   "a.runtotal.value=secsToTime(+Json.rt)\n"
   "a.filter.value=s2t(+Json.fm)\n"
   "a.fan.innerHTML=fan?\"Fan On\":\"Fan Off\"\n"
   "a.run.innerHTML=states[+Json.s]\n"
   "a.hm.innerHTML=rh?\"Humidifier On\":\"Humidifier Off\"\n"
   "setAtt()\n"
   " }\n"
   " else if(event == 'alert')\n"
   " {\n"
   "alert(data)\n"
   " }\n"
   "}\n"
   "}\n"
   "\n"
   "function setVar(varName, value)\n"
   "{\n"
   " ws.send('cmd;{\"key\":\"'+myToken+'\",\"'+varName+'\":'+value+'}')\n"
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
   "ovrActive=false\n"
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
   "a.ovrCell.setAttribute('class',away?'style1':(ovrActive?'style5':'style1'))\n"
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
   "function incCool(n)\n"
   "{\n"
   "a.coolh.value= +a.coolh.value+n\n"
   "a.cooll.value= +a.cooll.value+n\n"
   "\n"
   "setVar('cooltemph',(+a.coolh.value*10).toFixed())\n"
   "setVar('cooltempl',(+a.cooll.value*10).toFixed())\n"
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
   "function setOvrTemp()\n"
   "{\n"
   "setVar('override',(+a.ovrtemp.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function setOvrTemp()\n"
   "{\n"
   "setVar('override',(+a.ovrtemp.value*10).toFixed())\n"
   "}\n"
   "\n"
   "function cancelOvr()\n"
   "{\n"
   "setVar('override',0)\n"
   "}\n"
   "\n"
   "function setVars()\n"
   "{\n"
   " s='cmd;{\"key\":\"'+myToken+'\"'\n"
   " s+=',\"cooltemph\":'+(+a.coolh.value*10).toFixed()\n"
   " s+=',\"cooltempl\":'+(+a.cooll.value*10).toFixed()\n"
   " s+=',\"heattemph\":'+(+a.heath.value*10).toFixed()\n"
   " s+=',\"heattempl\":'+(+a.heatl.value*10).toFixed()\n"
   " s+=',\"overridetime\":'+t2s(a.ovrtime.value)\n"
   " s+=',\"fancycletime\":'+t2s(a.fantime.value)\n"
   " s+=',\"awaydelta\":'+(+a.awaytemp.value*10).toFixed()\n"
   " s+='}'\n"
   " ws.send(s)\n"
   "}\n"
   "\n"
   "function secsToTime( elap )\n"
   "{\n"
   "d=0\n"
   "m=0\n"
   "h=Math.floor(elap/3600)\n"
   "if(h >23)\n"
   "{\n"
   "d=Math.floor(h/24)\n"
   "h-=(d*24)\n"
   "}\n"
   "else\n"
   "{\n"
   "m=Math.floor((elap-(h*3600))/60)\n"
   "s=elap-(h*3600)-(m*60)\n"
   "if(s<10) s='0'+s\n"
   "if(h==0)\n"
   "{\n"
   "if( m < 10) m='  '+m\n"
   "return '    '+m +':'+s\n"
   "}\n"
   "}\n"
   "if(m<10) m='0'+m\n"
   "if(h<10) h='  '+h\n"
   "if(d) return d+'d '+h+'h'\n"
   "return h+':'+m+':'+s\n"
   "}\n"
   "\n"
   "function s2t(elap)\n"
   "{\n"
   "m=Math.floor(elap/60)\n"
   "s=elap-(m*60)\n"
   "if(m==0) return s\n"
   "if(s<10) s='0'+s\n"
   "return m+':'+s\n"
   "}\n"
   "\n"
   "function t2s(v)\n"
   "{\n"
   "if(typeof v == 'string') v = (+v.substr(0, v.indexOf(':'))*60) + (+v.substr(v.indexOf(':')+1))\n"
   "return v\n"
   "}\n"
   "//--></script>\n"
   "</head>\n"
   "<body onload=\"{\n"
   " myStorage3 = localStorage.getItem('myStoredText3')\n"
   " if(myStorage3  != null)\n"
   "document.getElementById('ovrtemp').value=myStorage3\n"
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
   "<td width=\"40\"><input type=\"button\" value=\" On \" name=\"fOn\" onClick=\"{setfan(1)}\"><input type=\"button\" value=\"Cycle\" name=\"fS\" onClick=\"{setfan(2)}\"></td>\n"
   "<td width=300 align=\"right\"><input type=\"submit\" value=\"Settings\" onClick=\"window.location='/settings';\"></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td id=\"runCell\"><div id=\"run\">Cooling</div></td>\n"
   "<td align=\"right\"><input type=\"button\" value=\" Off \" name=\"mOff\" onClick=\"{setMode(0)}\"></td>\n"
   "<td><input type=\"button\" value=\"Cool\" name=\"mCool\" onClick=\"{setMode(1)}\"><input type=\"button\" value=\"Heat \" name=\"mHeat\" onClick=\"{setMode(2)}\"></td>\n"
   "<td><input type=\"button\" value=\"Auto\" name=\"mAuto\" onClick=\"{setMode(3)}\"> &nbsp &nbsp &nbsp<input type=\"submit\" value=\"  Chart  \" align=\"right\" onClick=\"window.location='/chart.html';\">\n"
   "</td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>&nbsp</td><td></td><td></td><td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Cool Hi</td><td><input type=text size=3 id=\"coolh\" onChange=\"{setVars()}\"></td><td><input type=\"button\" value=\"+1\" onClick=\"{incCool(1)}\"></td><td><div id=\"time\"></div></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td style=\"width: 81px\">Cool Lo</td>\n"
   "<td style=\"width: 44px\"><input type=text size=3 id=\"cooll\" onChange=\"{setVars()}\"></td>\n"
   "<td style=\"width: 200px\"><input type=\"button\" value=\" -1\" onClick=\"{incCool(-1)}\"></td>\n"
   "<td><input type=\"button\" value=\" HP \" name=\"hHP\" onClick=\"{setHeatMode(0)}\"><input type=\"button\" value=\"Gas \" name=\"hGas\" onClick=\"{setHeatMode(1)}\"><input type=\"button\" value=\"Auto\" name=\"hAuto\" onClick=\"{setHeatMode(2)}\"></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Heat Hi</td>\n"
   "<td><input type=text size=3 id=\"heath\" onChange=\"{setVars()}\"></td>\n"
   "<td><input type=\"button\" value=\"+1\" onClick=\"{incHeat(1)}\"></td>\n"
   "<td></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Heat Lo</td>\n"
   "<td><input type=text size=3 id=\"heatl\" onChange=\"{setVars()}\"></td>\n"
   "<td><input type=\"button\" value=\" -1\" onClick=\"{incHeat(-1)}\"></td>\n"
   "<td id=\"hmCell\"><div id=\"hm\">Humidifier Off</div></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td></td>\n"
   "<td></td>\n"
   "<td></td>\n"
   "<td>\n"
   "<input type=\"button\" value=\" Off \" name=\"hmOff\" onClick=\"{setHumidMode(0)}\"></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>ovr Time</td><td><input type=text size=3 id=\"ovrtime\"></td><td><input type=\"button\" value=\"  Go  \" onClick=\"{localStorage.setItem('myStoredText3', a.ovrtemp.value);setOvrTemp()}\"></td>\n"
   "<td>\n"
   "<input type=\"button\" value=\"Fan\" name=\"hmFan\" onClick=\"{setHumidMode(1)}\"></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td id=\"ovrCell\">Ovrrd &Delta;</td>\n"
   "<td><input type=text size=3 id=\"ovrtemp\" onChange=\"{setVars()}\"></td>\n"
   "<td><input type=\"button\" value=\" Stop \" onClick=\"{cancelOvr()}\">\n"
   "</td>\n"
   "<td><input type=\"button\" value=\"Run\" name=\"hmRun\" onClick=\"{setHumidMode(2)}\"></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Freshen</td>\n"
   "<td><input type=text size=3 id=\"fantime\" onChange=\"{setVars()}\"></td>\n"
   "<td><input type=\"button\" style=\"margin-left:200\" value=\"  Go  \" onClick=\"{setfan(3)}\"></td>\n"
   "<td><input type=\"button\" value=\" A1 \" name=\"hmAuto1\" onClick=\"{setHumidMode(3)}\"></td>\n"
   "</tr>\n"
   "<tr>\n"
   "<td>Away &Delta;</td><td><input type=text size=3 id=\"awaytemp\" onChange=\"{setVars()}\"></td><td><input type=\"button\" value=\"Away\" name=\"away\" onClick=\"{setAway()}\"></td>\n"
   "<td><input type=\"button\" value=\" A2 \" name=\"hmAuto2\" onClick=\"{setHumidMode(4)}\"></td>\n"
   "</tr>\n"
   "</table><br/>\n"
   "<table style=\"width: 350px\">\n"
   "<tr>\n"
   "<td>Cycle</td><td><div id=\"cyctimer\" style=\"width: 70px\">0</div></td>\n"
   "<td>Total</td><td><input type=\"button\" id =\"runtotal\" value=\"0\" onClick=\"{rstTot()}\"></td>\n"
   "<td>Filter</td><td><input type=\"button\" id =\"filter\" value=\"0\" onClick=\"{rstFlt()}\"></td>\n"
   "</tr>\n"
   "</table>\n"
   "<small>Copyright &copy 2016 CuriousTech.net</small>\n"
   "</body>\n"
   "</html>\n";

const char page_settings[] PROGMEM = 
   "<!DOCTYPE html>\n"
   "<html>\n"
   "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n"
   "<head>\n"
   "\n"
   "<title>ESP-HVAC</title>\n"
   "<style type=\"text/css\">\n"
   "table{\n"
   "border-radius: 5px;\n"
   "box-shadow: 2px 2px 12px #000000;\n"
   "background-image: -moz-linear-gradient(top, #ffffff, #50a0ff);\n"
   "background-image: -ms-linear-gradient(top, #ffffff, #50a0ff);\n"
   "background-image: -o-linear-gradient(top, #ffffff, #50a0ff);\n"
   "background-image: -webkit-linear-gradient(top, #efffff, #50a0ff);\n"
   "background-image: linear-gradient(top, #ffffff, #50a0ff);\n"
   "background-clip: padding-box;\n"
   "}\n"
   "input{\n"
   "border-radius: 4px;\n"
   "box-shadow: 3px 3px 10px #000000;\n"
   "background-image: -moz-linear-gradient(top, #00ffff, #50a0ff);\n"
   "background-image: -ms-linear-gradient(top, #00ffff, #50a0ff);\n"
   "background-image: -o-linear-gradient(top, #00ffff, #50a0ff);\n"
   "background-image: -webkit-linear-gradient(top, #00ffff, #50a0ff);\n"
   "background-image: linear-gradient(top, #00ffff, #50a0ff);\n"
   "background-clip: padding-box;\n"
   "text-align: right;\n"
   "}\n"
   ".style1{border-width: 0;}\n"
   ".style2{text-align: right;}\n"
   ".style5{\n"
   "border-radius: 5px;\n"
   "box-shadow: 2px 2px 12px #000000;\n"
   "background-image: -moz-linear-gradient(top, #ff00ff, #ffa0a0);\n"
   "background-image: -ms-linear-gradient(top, #ff00ff, #ffa0a0);\n"
   "background-image: -o-linear-gradient(top, #ff00ff, #ffa0a0);\n"
   "background-image: -webkit-linear-gradient(top, #ff0000, #ffa0a0);\n"
   "background-image: linear-gradient(top, #ff00ff, #ffa0a0);\n"
   "}\n"
   "body{width:340px;display:block;font-family: Arial, Helvetica, sans-serif;}\n"
   "</style>\n"
   "\n"
   "<script type=\"text/javascript\">\n"
   "<!--\n"
   "\n"
   "var Json,ovrActive,away,rmtMode\n"
   "var a=document.all\n"
   "var states = new Array('Idle','Cooling','HP Heat','NG Heat')\n"
   "var ws\n"
   "function startEvents()\n"
   "{\n"
   "ws = new WebSocket(\"ws://\"+window.location.host+\"/ws\")\n"
   "//ws = new WebSocket(\"ws://192.168.31.125/ws\")\n"
   "ws.onopen = function(evt) { }\n"
   "ws.onclose = function(evt) { alert(\"Connection closed.\"); }\n"
   "\n"
   "ws.onmessage = function(evt) {\n"
   "// console.log(evt.data)\n"
   " lines = evt.data.split(';')\n"
   " event=lines[0]\n"
   " data=lines[1]\n"
   " Json=JSON.parse(data)\n"
   " if(event == 'settings')\n"
   " {\n"
   "a.humidl.value= +Json.rh0/10\n"
   "a.humidh.value= +Json.rh1/10\n"
   "a.idlemin.value= s2t(+Json.im)\n"
   "a.cycmin.value= s2t(+Json.cn)\n"
   "a.cycmax.value= s2t(+Json.cx)\n"
   "a.thresh.value= +Json.ct/10\n"
   "a.fandelay.value= s2t(+Json.fd)\n"
   "a.fanpre.value= s2t(+Json.fp)\n"
   "a.awaytime.value= s2t(+Json.at)\n"
   "a.heatthr.value= +Json.ht\n"
   "a.ppkwh.value= +Json.ppk/1000\n"
   "a.ccf.value= +Json.ccf/1000\n"
   "a.cfm.value= +Json.cfm/1000\n"
   "a.fcr.value= +Json.fcr\n"
   "a.fcd.value= +Json.fcd\n"
   "a.fco.value= +Json.fco\n"
   "a.acth.value= +Json.dl/10\n"
   "rmtMode=+Json.ar\n"
   "setAtt()\n"
   " }\n"
   " else if(event == 'state')\n"
   " {\n"
   "away=+Json.aw\n"
   "setAtt()\n"
   " }\n"
   " else if(event == 'alert')\n"
   " {\n"
   "alert(data)\n"
   " }\n"
   "}\n"
   "}\n"
   "\n"
   "function setVar(varName, value)\n"
   "{\n"
   " ws.send('cmd;{\"key\":\"'+a.myToken.value+'\",\"'+varName+'\":'+value+'}')\n"
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
   "function setRmt(v)\n"
   "{\n"
   "switch(v)\n"
   "{\n"
   "case 1: rmtMode&=0xFD;rmtMode|=8;break;\n"
   "case 2: rmtMode|=10;break;\n"
   "case 3: rmtMode&=0xF7;rmtMode|=2;break;\n"
   "case 4: rmtMode&=0xFE;rmtMode|=4;break;\n"
   "case 5: rmtMode|=5;break;\n"
   "case 6: rmtMode&=0xFB;rmtMode|=1;break;\n"
   "}\n"
   "setVar('rmtflgs',rmtMode)\n"
   "setAtt()\n"
   "}\n"
   "\n"
   "function secsToTime( elap )\n"
   "{\n"
   "d=0\n"
   "m=0\n"
   "h=Math.floor(elap/3600)\n"
   "if(h >23)\n"
   "{\n"
   "d=Math.floor(h/24)\n"
   "h-=(d*24)\n"
   "}\n"
   "else\n"
   "{\n"
   "m=Math.floor((elap-(h*3600))/60)\n"
   "s=elap-(h*3600)-(m*60)\n"
   "if(s<10) s='0'+s\n"
   "if(h==0)\n"
   "{\n"
   "if( m < 10) m='  '+m\n"
   "return '    '+m +':'+s\n"
   "}\n"
   "}\n"
   "if(m<10) m='0'+m\n"
   "if(h<10) h='  '+h\n"
   "if(d) return d+'d '+h+'h'\n"
   "return h+':'+m+':'+s\n"
   "}\n"
   "\n"
   "function s2t(elap)\n"
   "{\n"
   "m=Math.floor(elap/60)\n"
   "s=elap-(m*60)\n"
   "if(m==0) return s\n"
   "if(s<10) s='0'+s\n"
   "return m+':'+s\n"
   "}\n"
   "\n"
   "function t2s(v)\n"
   "{\n"
   "if(typeof v == 'string') v = (+v.substr(0, v.indexOf(':'))*60) + (+v.substr(v.indexOf(':')+1))\n"
   "return v\n"
   "}\n"
   "//--></script>\n"
   "</head>\n"
   "<body onload=\"{\n"
   " myStorage1 = localStorage.getItem('myStoredText1')\n"
   " if(myStorage1  != null){\n"
   "document.getElementById('myToken').value=myStorage1\n"
   " }\n"
   " startEvents()\n"
   "}\" align=\"center\">\n"
   "<strong><em>CuriousTech HVAC Settings</em></strong><br><br>\n"
   "<table style=\"width: 240px\" cellspacing=0 cellpadding=0>\n"
   "<tr>\n"
   "<td style=\"width: 81px\">Threshold</td>\n"
   "<td style=\"width: 44px\"><input type=text size=4 id=\"thresh\" onchange=\"{setVar('cyclethresh',(+this.value*10).toFixed())}\"></td>\n"
   "<td style=\"width: 20px\"></td>\n"
   "<td>\n"
   "<input type=\"submit\" value=\" Home \" onClick=\"window.location='/iot';\">\n"
   "</td>\n"
   "</tr>\n"
   "<tr><td>Heat Thrsh</td><td><input type=text size=4 id=\"heatthr\" onchange=\"{setVar('eheatthresh',+this.value)}\"></td><td></td><td></td></tr>\n"
   "<tr><td>AC &#x2202 Limit</td><td><input type=text size=4 id=\"acth\" onchange=\"{setVar('dl',(+this.value*10).toFixed())}\"></td><td></td><td></td></tr>\n"
   "<tr><td>Rh Low</td><td><input type=text size=4 id=\"humidl\" onchange=\"{setVar('humidl',(+this.value*10).toFixed())}\"></td><td>High</td><td><input type=text size=3 id=\"humidh\" onchange=\"{setVar('humidh',(+this.value*10).toFixed())}\"></td></tr>\n"
   "<tr><td>Fan Pre</td><td><input type=text size=4 id=\"fanpre\" onchange=\"{setVar('fanpretime',t2s(this.value))}\"></td><td>Post</td><td><input type=text size=3 id=\"fandelay\" onchange=\"{setVar('fanpostdelay',t2s(this.value))}\"></td></tr>\n"
   "<tr><td>cycle Min</td><td><input type=text size=4 id=\"cycmin\" onchange=\"{setVar('cyclemin',t2s(this.value))}\"></td><td>Max</td><td><input type=text size=3 id=\"cycmax\" onchange=\"{setVar('cyclemax',t2s(this.value))}\"></td></tr>\n"
   "<tr><td>Idle Min</td><td><input type=text size=4 id=\"idlemin\" onchange=\"{setVar('idlemin',t2s(this.value))}\"></td><td>PKW</td><td><input type=text size=3 id=\"ppkwh\" onchange=\"{setVar('ppk',(+this.value*1000).toFixed())}\"></td></tr>\n"
   "<tr><td>Away Lmt</td><td><input type=text size=4 id=\"awaytime\" onchange=\"{setVar('awaytime',t2s(this.value))}\"></td><td>CFM</td><td><input type=text size=3 id=\"cfm\" onchange=\"{setVar('cfm',(+this.value*1000).toFixed())}\"></td></tr>\n"
   "<tr><td>FC Shift</td><td><input type=text size=4 id=\"fco\" onchange=\"{setVar('fco',this.value)}\"></td><td>CCF</td><td><input type=text size=3 id=\"ccf\" onchange=\"{setVar('ccf',(+this.value*1000).toFixed())}\"></td></tr>\n"
   "<tr><td>Lookahead</td><td><input type=text size=4 id=\"fcr\" onchange=\"{setVar('fcrange',this.value)}\"></td><td>Disp</td><td><input type=text size=3 id=\"fcd\" onchange=\"{setVar('fcdisp',this.value)}\"></td></tr>\n"
   "<tr><td>Remote Hi</td><td><input type=\"button\" value=\"Remote\" name=\"rmth1\" onClick=\"{setRmt(1)}\"></td>\n"
   "<td><input type=\"button\" value=\"Avg\" name=\"rmth2\" onClick=\"{setRmt(2)}\"></td><td><input type=\"button\" value=\" Main  \" name=\"rmth3\" onClick=\"{setRmt(3)}\">\n"
   "</td></tr>\n"
   "<tr><td>Remote Lo</td><td><input type=\"button\" value=\"Remote\" name=\"rmtl1\" onClick=\"{setRmt(4)}\"></td>\n"
   "<td><input type=\"button\" value=\"Avg\" name=\"rmtl2\" onClick=\"{setRmt(5)}\"></td><td><input type=\"button\" value=\" Main  \" name=\"rmtl3\" onClick=\"{setRmt(6)}\">\n"
   "</td></tr>\n"
   "</table>\n"
   "<p>\n"
   "<table style=\"width: 240px\">\n"
   "<tr><td>Password</td><td><input id=\"myToken\" name=\"access_token\" type=text size=40 placeholder=\"e6bba7456a7c9\" style=\"width: 98px\"\n"
   " onChange=\"{\n"
   " localStorage.setItem('myStoredText1', a.myToken.value)\n"
   " alert(a.myToken.value+' Has been stored')\n"
   "}\">\n"
   "</td>\n"
   "</tr>\n"
   "</table></p>\n"
   "<small>Copyright &copy 2016 CuriousTech.net</small>\n"
   "</body>\n"
   "</html>\n";


const char page_chart[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>HVAC Chart</title>
<style type="text/css">
div,table,input{
border-radius: 5px;
margin-bottom: 5px;
box-shadow: 2px 2px 12px #000000;
background-image: -moz-linear-gradient(top, #ffffff, #a0a0a0);
background-image: -ms-linear-gradient(top, #ffffff, #a0a0a0);
background-image: -o-linear-gradient(top, #ffffff, #a0a0a0);
background-image: -webkit-linear-gradient(top, #efffff, #a0a0a0);
background-image: linear-gradient(top, #ffffff, #a0a0a0);
background-clip: padding-box;
}
.style3 {
border-radius: 5px;
margin-bottom: 5px;
box-shadow: 2px 2px 12px #000000;
background-image: -moz-linear-gradient(top, #4f4f4f, #50a0a0);
background-image: -ms-linear-gradient(top, #4f4f4f, #50a0a0);
background-image: -o-linear-gradient(top, #4f4f4f, #50a0a0);
background-image: -webkit-linear-gradient(top, #4f4f4f, #50a0a0);
background-image: linear-gradient(top, #4f4f4f, #50a0a0);
background-clip: padding-box;
}
.style4 {
border-radius: 5px;
margin-bottom: 5px;
box-shadow: 2px 2px 12px #000000;
background-image: -moz-linear-gradient(top, #4f4f4f, #50a0ff);
background-image: -ms-linear-gradient(top, #4f4f4f, #50a0ff);
background-image: -o-linear-gradient(top, #4f4f4f, #50a0ff);
background-image: -webkit-linear-gradient(top, #4f4f4f, #50a0ff);
background-image: linear-gradient(top, #4f4f4f, #50a0ff);
background-clip: padding-box;
}
.style5 {
border-radius: 5px;
box-shadow: 2px 2px 12px #000000;
background-image: -moz-linear-gradient(top, #ff00ff, #ffa0ff);
background-image: -ms-linear-gradient(top, #ff00ff, #ffa0ff);
background-image: -o-linear-gradient(top, #ff00ff, #ffa0ff);
background-image: -webkit-linear-gradient(top, #f0a0e0, #d0a0a0);
background-image: linear-gradient(top, #ff00ff, #ffa0ff);
}
body{background:silver;width:700px;display:block;text-align:center;font-family: Arial, Helvetica, sans-serif;}}
</style>
<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.6.1/jquery.min.js" type="text/javascript" charset="utf-8"></script>
<script type="text/javascript" src="forecast"></script>
<script type="text/javascript">
var graph;
xPadding=30
yPadding=50
drawOut=false
var yRange
var Json
var a=document.all
var ws
added=false
$(document).ready(function()
{
 myStorage1 = localStorage.getItem('myStoredText1')
 if(myStorage1  != null) myToken=myStorage1
 ws = new WebSocket("ws://"+window.location.host+"/ws")
// ws = new WebSocket("ws://192.168.31.125/ws")
 ws.onopen=function(evt){ws.send('cmd;{sum:0}')}
 ws.onclose=function(evt){alert("Connection closed.")}
 ws.onmessage = function(evt){
  console.log(evt.data)
  lines = evt.data.split(';')
  event=lines[0]
  data=lines[1]
  Json=JSON.parse(data)
  switch(event)
  {
    case 'settings':
      ppkwh=+Json.ppk/1000
      ccf=+Json.ccf/1000
      cfm=+Json.cfm/1000
      cw=+Json.cw
      fw=+Json.fw
      frnw=+Json.frnw
      md=+Json.m
      dl=+Json.dl
      a.fco.value=fco=+Json.fco
      a.fcr.value=fcr=+Json.fcr
      ct=+Json.ct
      c0=+Json.c0
      c1=+Json.c1
      h0=+Json.h0
      h1=+Json.h1
      a.lo.value=(iMin=(md==2)?h0:c0)/10
      a.hi.value=(iMax=(md==2)?h1:c1)/10
      a.ct.value=ct/10
      drawFC()
      break
    case 'state':
      sJson=Json
      cyc=secsToTime(+Json.ct)
      draw()
      break
    case 'alert':
      alert(data)
      break
    case 'print':
      break
    case 'sum':
      ws.send('cmd;{data:0}')
      dys=Json.day
      mns=Json.mon
      draw_bars()
      break
    case 'update':
      switch(Json.type)
      {
        case 'day':
          dys[Json.e][0]=+Json.d0
          dys[Json.e][1]=+Json.d1
          dys[Json.e][2]=+Json.d2
          break
      }
      draw_bars()
      break
    case 'ref':
      tb=Json.tb
      th=Json.th
      tm=Json.tm
      lm=Json.lm
      rm=Json.rm
      om=Json.om
      arr=new Array()
      break
    case 'data':
      for(i=0;i<Json.d.length;i++){
        Json.d[i][0]=(tb-Json.d[i][0]*10)*1000
        Json.d[i][1]+=tm
        Json.d[i][2]+=rm
        Json.d[i][3]+=lm
        Json.d[i][5]+=om
      }
      arr=arr.concat(Json.d)
      draw()
      break
    case 'data2':
      for(i=0;i<Json.d.length;i++)
        Json.d[i][0]*=1000
      arr=Json.d.concat(arr)
    case 'draw':
      draw()
      break
  }
 }
 setInterval(function(){
  s=0
  if(arr.length) s=(arr[0][0]/1000).toFixed()
  ws.send('cmd;{data:'+s+'}'); }, 60000);
});

function draw(){
  graph = $('#graph')
  c=graph[0].getContext('2d')

  tipCanvas=document.getElementById("tip")
  tipCtx=tipCanvas.getContext("2d")
  tipDiv=document.getElementById("popup")

  c.fillStyle='black'
  c.strokeStyle='black'
  c.clearRect(0, 0, graph.width(), graph.height())
  canvasOffset=graph.offset()
  offsetX=canvasOffset.left
  offsetY=canvasOffset.top

  c.lineWidth=2
  c.font='italic 8pt sans-serif'
  c.textAlign="left"

  c.beginPath() // borders
  c.moveTo(xPadding,0)
  c.lineTo(xPadding,graph.height()-yPadding)
  c.lineTo(graph.width()-xPadding, graph.height()-yPadding)
  c.lineTo(graph.width()-xPadding, 0)
  c.stroke()

  c.lineWidth = 1
  // dates
  step = Math.floor(arr.length / 15)
  if(step == 0) step = 1
  for(var i=0; i<arr.length-1; i+=step){
  c.save()
  c.translate(getXPixel(i), graph.height()-yPadding+5)
  c.rotate(0.9)
  date = new Date(arr[i][0])
  c.fillText(date.toLocaleTimeString(),0,0)
  c.restore()
  }

  yRange = getMaxY() - getMinY()
  // value range
  c.textAlign = "right"
  c.textBaseline = "middle"

  for(var i = getMinY(); i < getMaxY(); i += (yRange/8) )
    c.fillText((i/10).toFixed(1), graph.width()-6, getYPixel(i))

  c.fillText('Temp', graph.width()-6, 6)
  c.fillStyle = +sJson.r?(md==2?"red":"blue"):(+sJson.fr?"green":"gray")
  c.fillText((+sJson.it/10).toFixed(1), graph.width()-6, getYPixel(+sJson.it) )
 // cycle
  c.fillText(cyc,graph.width()-xPadding-7,graph.height()-yPadding-8)

  c.fillStyle="green"
  c.fillText('Rh', xPadding-6, 6)

  // rh scale
  for(i=0;i<10;i++){
    pos=graph.height()-8-(((graph.height()-yPadding)/10)*i)-yPadding
    c.fillText(i*10,xPadding-4,pos)
  }

  // in-out diff
  grd=c.createLinearGradient(0,yPadding,0,graph.height()-yPadding)
  grd.addColorStop(0,'rgba(255,100,0,0.2)')
  grd.addColorStop(1,'rgba(150,150,200,0.2)')

// Fill with gradient
  c.fillStyle = grd
  c.beginPath()
  c.moveTo(graph.width()-xPadding, graph.height()-yPadding)
  for(i=0;i<arr.length;i++){
  switch(md){
    default: diff=0; break
    case 1: diff=(arr[i][5]-200-arr[i][1])*10; break // 20~30=0~100%
    case 2: diff=(arr[i][1]-380-arr[i][5])*3; break
  }
  if(diff<0) diff=0
  if(diff>1000) diff=1000
  c.lineTo(getXPixel(i),getRHPixel(diff))
  }
  c.lineTo(getXPixel(i),graph.height()-yPadding)
  c.closePath()
  c.fill()

  //threshold
  c.fillStyle = 'rgba(100,100,180,0.25)'
  c.beginPath()
  c.moveTo(getXPixel(0),getYPixel(arr[0][3]+th))

  for(i=1;i<arr.length-1;i++)
    c.lineTo(getXPixel(i),getYPixel(arr[i][3]+th))
  for(i=arr.length-2;i>=0;i--)
    c.lineTo(getXPixel(i),getYPixel(arr[i][3]))
  c.closePath()
  c.fill()

  // temp lines
  date = new Date(arr[0][0])
  dt = date.getDate()
  for(i = 1; i < arr.length; i++){
  c.strokeStyle = stateColor(arr[i][4])
  c.beginPath()
  c.moveTo(getXPixel(i), getYPixel(arr[i][1]))
  c.lineTo(getXPixel(i-1), getYPixel(arr[i-1][1]))
  c.stroke()
  date = new Date(arr[i][0])
  if(dt != date.getDate())
  {
    dt = date.getDate()
    c.strokeStyle = '#000'
    c.beginPath() // borders
    c.moveTo(getXPixel(i),0)
    c.lineTo(getXPixel(i),graph.height()-yPadding)
    c.stroke()
  }
  }
  // out temp
  c.strokeStyle = '#fa0'
  if(drawOut) for(i=1;i<arr.length;i++){
  c.beginPath()
  c.moveTo(getXPixel(i),getYPixel(arr[i][5]))
  c.lineTo(getXPixel(i-1),getYPixel(arr[i-1][5]))
  c.stroke()
  }

  // rh lines
  c.strokeStyle = '#0f0'
  c.beginPath()
  c.moveTo(getXPixel(0), getRHPixel(arr[0][2]))
  for(var i=1;i<arr.length-1;i ++)
  c.lineTo(getXPixel(i), getRHPixel(arr[i][2]))
  c.stroke()

  var dots = []
  for(i = 0; i < arr.length; i ++) {
    date = new Date(arr[i][0])
    dots.push({
      x: getXPixel(i),
      y: getYPixel(arr[i][1]),
      r: 4,
      rXr: 16,
      color: "red",
      tip: date.toLocaleTimeString()+' ',
      tip2: arr[i][1]/10,
      tip3: arr[i][2]/10,
      tip4: arr[i][5]/10
    })
  }

  // request mousemove events
  graph.mousemove(function(e){handleMouseMove(e);})

  // show tooltip when mouse hovers over dot
  function handleMouseMove(e){
    mouseX=parseInt(e.clientX-offsetX)
    mouseY=parseInt(e.clientY-offsetY)
    
    // Put your mousemove stuff here
    var hit = false
    for (i = 0; i < dots.length; i++) {
      dot = dots[i]
      dx = mouseX - dot.x
      dy = mouseY - dot.y
      if (dx * dx + dy * dy < dot.rXr) {
        tipCtx.clearRect(0, 0, tipCanvas.width, tipCanvas.height)
        tipCtx.lineWidth = 2
        tipCtx.fillStyle = "#000000"
        tipCtx.strokeStyle = '#333'
        tipCtx.font = 'italic 8pt sans-serif'
        tipCtx.textAlign = "left"

        tipCtx.fillText( dot.tip, 4, 15)
        tipCtx.fillText( dot.tip2+'°F', 4, 29)
        tipCtx.fillText( dot.tip3+'%', 4, 44)
        tipCtx.fillText( dot.tip4 + '°F', 4, 58)
        hit = true
        popup = document.getElementById("popup")
        popup.style.top = dot.y + "px"
        popup.style.left = (dot.x-60) + "px"
      }
    }
    if (!hit) { popup.style.left = "-200px" }
  }

  mousePos={x:0,y:0}
  lastPos=mousePos
  if(added==false)
  {
    graph[0].addEventListener("mousedown",function(e){
      lastPos=getMousePos(graph[0],e)
      drawOut=!drawOut
      draw()
    },false)
    added=true
  }
  function getMousePos(cDom, mEv){
    rect = cDom.getBoundingClientRect();
    return{
     x: mEv.clientX-rect.left,
     y: mEv.clientY-rect.top
    }
  }
}

function getMaxY(){
  var max = 0
  
  for(i=0; i<arr.length-1; i++)
  {
    if(arr[i][1] > max)
      max=arr[i][1]
    if(arr[i][3]+th>max)
      max=arr[i][3]+th
    if(drawOut&&arr[i][5]>max)
      max=arr[i][5]
  }
  return Math.ceil(max)
}

function getMinY(){
  var min = 1500

  for(i=0; i<arr.length; i++)
  {
    if(arr[i][1]<min)
      min=arr[i][1]
    if(arr[i][3]<min)
      min=arr[i][3]
    if(drawOut&&arr[i][5]<min)
      min=arr[i][5]
  }
  return Math.floor(min)
}
 
function getXPixel(val){
  x=(graph.width()-xPadding)-((graph.width()-26-xPadding)/arr.length)*val
  return x.toFixed()
}

function getYPixel(val) {
  y=graph.height()-( ((graph.height()-yPadding)/yRange)*(val-getMinY()))-yPadding
  return y.toFixed()
}

function getRHPixel(val) {
  return graph.height()-(((graph.height()-yPadding)/1000)*val)-yPadding
}

function stateColor(s)
{
  sts=Array('gray','blue','red','red')
  if(s==1) return 'cyan'
  return sts[s>>1]
}

function setVar(varName, value)
{
 ws.send('cmd;{"key":"'+myToken+'","'+varName+'":'+value+'}')
}

function secsToTime(elap)
{
  dy=0
  m=0
  h=Math.floor(elap/3600)
  if(h>23)
  {
    dy=Math.floor(h/24)
    h-=(dy*24)
    elap-=dy*3600*24
  }
  
  m=Math.floor((elap-(h*3600))/60)
  s=elap-(h*3600)-(m*60)
  if(s<10) s='0'+s
  if(h==0&&dy==0)
  {
    if(m<10) m='  '+m
    return '    '+m +':'+s
  }
  if(m<10) m='0'+m
  if(h<10) h='  '+h
  if(dy) return dy+'d '+h+':'+m+'m'
  return h+':'+m+':'+s
}

function draw_bars()
{
    graph = $('#chart')
  var c=document.getElementById('chart')
  rect=c.getBoundingClientRect()
  canvasX=rect.x
  canvasY=rect.y

    tipCanvas=document.getElementById("tip")
    tipCtx=tipCanvas.getContext("2d")
    tipDiv=document.getElementById("popup")

  ctx=c.getContext("2d")
  ht=c.height/2
  ctx.fillStyle="#FFF"
  ctx.font="10px sans-serif"

    dots2=[]
    date=new Date()
  ctx.lineWidth=6
  draw_scale(dys,c.width-4,ht,2,1,date.getDate()-1)
  ctx.lineWidth=14
  draw_scale(mns,c.width-4,ht-2,ht+2,1,date.getMonth())

  // request mousemove events
  graph.mousemove(function(e){handleMouseMove(e);})

  // show tooltip when mouse hovers over dot
  function handleMouseMove(e){
    rect=c.getBoundingClientRect()
    mouseX=e.clientX-rect.x
    mouseY=e.clientY-rect.y
    var hit = false
    for(i=0;i<dots2.length;i++){
      dot=dots2[i]
      if(mouseX>=dot.x && mouseX<=dot.x2 && mouseY>=dot.y && mouseY<=dot.y2){
        tipCtx.clearRect(0, 0, tipCanvas.width, tipCanvas.height)
        tipCtx.fillStyle = "#000000"
        tipCtx.strokeStyle = '#333'
        tipCtx.font = 'italic 8pt sans-serif'
        tipCtx.textAlign = "left"
        tipCtx.fillText(dot.tip, 4,15)
        tipCtx.fillText(dot.tip2,4,29)
        tipCtx.fillText(dot.tip3,4,44)
        tipCtx.fillText(dot.tip4,4,59)
        tipCtx.fillText(dot.tip5,4,75)
        hit = true
        popup = document.getElementById("popup")
        popup.style.top =(dot.y+rect.y+window.pageYOffset)+"px"
        x=dot.x+rect.x-60
        if(x<10)x=10
        popup.style.left=x+"px"
      }
    }
    if(!hit){popup.style.left="-200px"}
  }

  function getMousePos(cDom, mEv){
    rect = cDom.getBoundingClientRect();
    return{
     x: mEv.clientX-rect.left,
     y: mEv.clientY-rect.top
    }
  }
}

function draw_scale(ar,w,h,o,p,ct)
{
  ctx.fillStyle="#336"
  ctx.fillRect(2,o,w,h-3)
  ctx.fillStyle="#FFF"
  max=[0,0,0]
  tot=[0,0,0,0,0]
  for(i=0;i<ar.length;i++)
  {
    if(ar[i][0]>max[0]) max[0]=ar[i][0]
    if(ar[i][1]>max[1]) max[1]=ar[i][1]
    if(ar[i][2]>max[0]) max[0]=ar[i][2]
    tot[0]+=ar[i][0]
    tot[1]+=ar[i][1]
    tot[2]+=ar[i][2]
  }
  max[2]=max[0]
  ctx.textAlign="center"
  lw=ctx.lineWidth
  clr=['#55F','#F55','#5F5']
  mbh=0
  for(i=0;i<ar.length;i++)
  {
    x=i*((w-40)/ar.length)+10
    for(j=0;j<3;j++)
    {
      ctx.strokeStyle=clr[j]
        bh=ar[i][j]*(h-20)/max[j]
        if(mbh<bh) mbh=bh
        y=(o+h-20)-bh
      ctx.beginPath()
        ctx.moveTo(x,o+h-20)
        ctx.lineTo(x,y)
      ctx.stroke()
      x+=lw
    }
    ctx.strokeStyle="#FFF"
    ctx.fillText(i+p,x-lw*2,o+h-7)

    if(i==ct)
    {
      ctx.strokeStyle="#fff"
      ctx.lineWidth=1
      ctx.beginPath()
        ctx.moveTo(x-1,o+h-20)
        ctx.lineTo(x-1,o)
      ctx.stroke()
      ctx.lineWidth=lw
    }
    if(mbh<25) mbh=25
    costE=+(ppkwh*ar[i][0]*(cw/3600000))+(ppkwh*ar[i][2]*(fw/3600000))+(ppkwh*ar[i][1]*(frnw/3600000))
    costG=+(ccf*ar[i][1]*cfm)/3600
    tot[3]+=costE
    tot[4]+=costG
    if(ar[i][0]||ar[i][1]||ar[i][2])
      dots2.push({
      x: x-lw*3,
      y: (o+h-20)-mbh,
      y2: (o+h),
      x2: x+ctx.lineWidth*1.5,
      tip: 'AC'+secsToTime(ar[i][0]),
      tip2: 'NG\t\t'+secsToTime(ar[i][1]),
      tip3: 'FAN\t\t'+secsToTime(ar[i][2]),
      tip4: 'Elec   $'+costE.toFixed(2),
      tip5: 'NG     $'+costG.toFixed(2)
    })
  }
  ctx.textAlign="right"
  ctx.fillText(secsToTime(tot[0]),w-1,o+10)
  ctx.fillText(secsToTime(tot[1]),w-1,o+21)
  ctx.fillText('$'+tot[3].toFixed(2),w-1,o+32)
  ctx.fillText('$'+tot[4].toFixed(2),w-1,o+43)
}

function drawFC(){
  graph2 = $('#graph2')
  c=graph2[0].getContext('2d')

  c.fillStyle='black'
  c.strokeStyle='black'
  c.clearRect(0, 0, graph2.width(), graph2.height())
  canvasOffset=graph2.offset()
  offsetX=canvasOffset.left
  offsetY=canvasOffset.top

  c.lineWidth=2
  c.font='italic 8pt sans-serif'
  c.textAlign="left"

  c.beginPath() // borders
  c.moveTo(xPadding,0)
  c.lineTo(xPadding,graph2.height()-18)
  c.lineTo(graph2.width()-xPadding, graph2.height()-18)
  c.lineTo(graph2.width()-xPadding, 0)
  c.stroke()

  c.lineWidth = 1
  min=150
  max=-30
  cnt=0
  for(i=0;i<fc.length;i++)
  {
    if(fc[i][0]){
      if(min>fc[i][1]) min=fc[i][1]
      if(max<fc[i][1]) max=fc[i][1]
      cnt++
    }
  }
  max++
  yRange=max-min
  min2=150
  max2=-30
  cnt2=0
  for(i=fco;i<fcr;i++)
  {
    if(fc[i][0]){
      if(min2>fc[i][1]) min2=fc[i][1]
      if(max2<fc[i][1]) max2=fc[i][1]
      cnt2++
    }
  }
  yRange2=max2-min2

  // value range
  c.textAlign = "right"
  c.textBaseline = "middle"
  c.fillStyle='black'

  for(i = min; i<max; i+=(yRange/8) )
    c.fillText(i.toFixed(1), graph2.width()-6, getYPixel2(i))
  c.fillText('Out', graph2.width()-6, 6)

  c.textAlign = "left"
//  iMax+=ct;
//  iMin-=ct+400;
  iRng=iMax-iMin
  c.fillText(iMax/10, 6, getYPixel3(iMax))
  c.fillText(iMin/10, 6, getYPixel3(iMin))

  c.fillStyle='#40404050'
  w=graph2.width()-xPadding*2
  c.fillRect(xPadding,getYPixel3(iMax),w,getYPixel3(iMin)-getYPixel3(iMax))

  // temp lines
  c.fillStyle = "red"
  date = new Date(fc[0][0]*1000)
  dt = date.getDate()
  for(i=1; i<cnt; i++){
  c.strokeStyle = fc[i][1]<32?"blue":"red"
  c.beginPath()
  c.moveTo(getXPixel2(i), getYPixel2(fc[i][1]))
  c.lineTo(getXPixel2(i-1), getYPixel2(fc[i-1][1]))
  c.stroke()
  date = new Date(fc[i][0]*1000)
  if(dt != date.getDate())
  {
    dt = date.getDate()
    c.strokeStyle = '#555'
    c.beginPath() // borders
    c.moveTo(getXPixel2(i),0)
    c.lineTo(getXPixel2(i),graph2.height()-18)
    c.stroke()

    c.fillStyle = '#000'
    c.textAlign = "left"
    date = new Date(fc[i][0]*1000)
    c.fillText(date.toLocaleString().substr(0,8),getXPixel2(i),graph2.height()-8)
  }
  }
  c.fillStyle = "#9040F080"
  c.beginPath()
  c.moveTo(getXPixel2(0-(fco/3)), getTT(0,fco,0))
  for(i=fco; i<=fcr+fco; i++)
  {
    idx=fco+Math.floor(i/3)
  c.lineTo(getXPixel2(i-(fco/3)), getTT(i,idx,0))
  }
  for(i=fcr+fco; i>=fco; i--)
  {
    idx=fco+Math.floor(i/3)
  c.lineTo(getXPixel2(i-(fco/3)), getTT(i,idx,(md==2)?ct:-ct))
  }
  c.closePath()
  c.fill()
}
function getXPixel2(val){
  x=xPadding+((graph2.width()-xPadding*2)/cnt)*val
  return x.toFixed()
}

function getYPixel2(val) {
  y=graph2.height()-( ((graph2.height()-18)/yRange)*(val-min))-18
  return y.toFixed()
}
function getYPixel3(val) {
  y=graph2.height()/2-( (graph2.height()/2/iRng)*(val-iMin))
  return y+30
}
function getTT(i,o,th)
{
/*  min2=150
  max2=-30
  for(j=o;j<o+fcr;j++)
  {
    if(j<fc.length&&fc[j][0]){
      if(min2>fc[j][1]) min2=fc[j][1]
      if(max2<fc[j][1]) max2=fc[j][1]
    }
  }*/
  tt=(fc[i][1]-min2)*iRng/(max2-min2)+iMin+th/10
  return graph2.height()/2-(graph2.height()/2/iRng*(tt-iMin))+30
}
</script>
<style type="text/css">
#wrapper {
  width: 100%;
  height: 400px;
  position: relative;
}
#graph {
  width: 100%;
  height: 100%;
  position: absolute;
  top: 0;
  left: 0;
}
#popup {
  position: absolute;
  top: 150px;
  left: -150px;
  z-index: 10;
}
#wrapper2{
  width: 100%;
  height: 200px;
  position: relative;
}
#chart{
  width: 100%;
  height: 100%;
  position: absolute;
  top: 0;
  left: 0;
}
#wrapper3 {
  width: 100%;
  height: 170px;
  position: relative;
}
#graph2 {
  width: 100%;
  height: 100%;
  position: absolute;
  top: 0;
  left: 0;
}
.style1 {
  border-style: solid;
  border-width: 1px;
}
</style>
</head>
<body>
<div id="wrapper">
<canvas id="graph" width="700" height="400"></canvas>
<div id="popup"><canvas id="tip" width="90" height="78"></canvas></div>
</div>
<div id="wrapper2">
<canvas id="chart" width="700" height="200"></canvas>
</div>
<table><tr>
<td>Offset:<input type=text size=1 id="fco" onchange="{fco=+this.value;drawFC()}"> Range:<input type=text size=1 id="fcr" onchange="{fcr=+this.value;drawFC()}">
 Low:<input type=text size=1 id="lo" onchange="{iMin=(+this.value)*10;drawFC()}">
 High:<input type=text size=1 id="hi" onchange="{iMax=(+this.value)*10;drawFC()}">
 Thresh:<input type=text size=1 id="ct" onchange="{ct=(+this.value)*10;drawFC()}">
</td>
<td></td>
</tr></table>
<div id="wrapper3">
<canvas id="graph2" width="700" height="170"></canvas>
</div>
</body>
</html>
)rawliteral";
