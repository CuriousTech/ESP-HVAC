const char page_index[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<head>
<title>ESP-HVAC</title>
<link rel="stylesheet" type="text/css" href="styles.css">
<style type="text/css">
body{width:360px;display:block;font-family: Arial, Helvetica, sans-serif;}
</style>

<script type="text/javascript"><!--
var Json,mode,autoMode,heatMode,fanMode,running,fan,rhm,ovrActive,away,rh
var a=document.all
var states=new Array('IDLE','COOLING','HP HEAT','NG HEAT')
var ws
var myToken=localStorage.getItem('myStoredText1')
function startEvents()
{
ws=new WebSocket("ws://"+window.location.host+"/ws")
//ws=new WebSocket("ws://192.168.31.46/ws")
ws.onopen=function(evt){}
ws.onclose=function(evt){alert("Connection closed.");}

ws.onmessage=function(evt){
 console.log(evt.data)
 lines=evt.data.split(';')
 event=lines[0]
 data=lines[1]
 if(event=='settings')
 {
  Json=JSON.parse(data)
    mode=+Json.m
  autoMode=+Json.am
  heatMode=+Json.hm
  fanMode=+Json.fm
  rhm=+Json.rhm
  ovrActive=+Json.ot
  setAtt()
  a.cooll.value=+Json.c0/10
  a.coolh.value=+Json.c1/10
  a.heatl.value=+Json.h0/10
  a.heath.value=+Json.h1/10
  a.humidl.value=+Json.rh0/10
  a.humidh.value=+Json.rh1/10
  a.ovrtime.value=s2t(+Json.ov)
  a.fantime.value=s2t(+Json.fct)
  a.awaytemp.value=+Json.ad/10
  if( +a.ovrtemp.value==0)
   a.ovrtemp.value= -2.0
 }
 else if(event == 'state')
 {
  Json=JSON.parse(data)
  running=+Json.r
  fan=+Json.fr
  rh=+Json.rh
  away=+Json.aw
  a.time.innerHTML=(new Date(+Json.t*1000)).toLocaleTimeString()
  a.intemp.innerHTML=(+Json.it/10).toFixed(1)
  a.rh.value=(+Json.rh/10).toFixed(1)+'%'
  a.target.innerHTML=(+Json.tt/10).toFixed(1)
  a.outtemp.innerHTML=(+Json.ot/10).toFixed(1)
  a.cyctimer.innerHTML=secsToTime(+Json.ct)
  a.runtotal.value=secsToTime(+Json.rt)
  a.filter.value=s2t(+Json.fm)
  a.fan.innerHTML=fan?"FAN ON":"FAN OFF"
  a.run.innerHTML=states[+Json.s]
  hon=+Json.h
  a.hm.innerHTML=hon?"HUMIDIFIER ON":"HUMIDIFIER OFF"
  a.hmCell.setAttribute('class',hon?'style5':'style1')
  setAtt()
 }
 else if(event=='alert')
 {
  alert(data)
 }
}
}

function setVar(varName, value)
{
 ws.send('cmd;{"key":"'+myToken+'","'+varName+'":'+value+'}')
}

function setfan(n)
{
if(n<3) fanMode=n
setVar('fanmode',n)
setAtt()
}

function setMode(m)
{
setVar('mode',mode=m)
setAtt()
}

function setHeatMode(m)
{
setVar('heatmode',heatMode=m)
setAtt()
}

function setHumidMode(m)
{
setVar('humidmode',rhm=m)
setAtt()
}

function setAway()
{
away=!away
ovrActive=false
setVar('away',away?1:0)
setAtt()
}

function rstFlt()
{
setVar('resetfilter',0)
}

function rstTot()
{
setVar('resettotal',0)
}

function setAtt()
{
a.runCell.setAttribute('class',running?'style5':'style1')
a.fAuto.setAttribute('class',fanMode==0?'style5':'')
a.fOn.setAttribute('class',fanMode==1?'style5':'')
a.fan.innerHTML="FAN "+((fanMode==1)?"ON":(fan?"ON":"OFF"))
a.fanCell.setAttribute('class',fan?'style5' : 'style1')
a.ovrCell.setAttribute('class',away?'style1':(ovrActive?'style5':'style1'))

a.mOff.setAttribute('class',mode==0?'style5':'')
a.mCool.setAttribute('class',mode==1?'style5':'')
a.mHeat.setAttribute('class',mode==2?'style5':'')
a.mAuto.setAttribute('class',mode==3?'style5':'')
a.fCyc.setAttribute('class',mode==4?'style5':'')

a.hHP.setAttribute('class',heatMode==0?'style5':'')
a.hGas.setAttribute('class',heatMode==1?'style5':'')
a.hAuto.setAttribute('class',heatMode==2?'style5':'')

a.hmOff.setAttribute('class',rhm==0?'style5':'')
a.hmFan.setAttribute('class',rhm==1?'style5':'')
a.hmRun.setAttribute('class',rhm==2?'style5':'')
a.hmAuto1.setAttribute('class',rhm==3?'style5':'')
a.hmAuto2.setAttribute('class',rhm==4?'style5':'')
a.away.setAttribute('class',away?'style5':'')
}

function incCool(n)
{
a.coolh.value=+a.coolh.value+n
a.cooll.value=+a.cooll.value+n
setVars()
}

function incHeat(n)
{
a.heath.value=+a.heath.value+n
a.heatl.value=+a.heatl.value+n
setVars()
}

function setOvrTemp()
{
setVar('override',(+a.ovrtemp.value*10).toFixed())
}

function setOvrTemp()
{
setVar('override',(+a.ovrtemp.value*10).toFixed())
}

function cancelOvr()
{
  setVar('override',0)
}

function setVars()
{
 s='cmd;{"key":"'+myToken+'"'
 s+=',"cooltemph":'+(+a.coolh.value*10).toFixed()+',"cooltempl":'+(+a.cooll.value*10).toFixed()
 s+=',"heattemph":'+(+a.heath.value*10).toFixed()+',"heattempl":'+(+a.heatl.value*10).toFixed()
 s+=',"overridetime":'+t2s(a.ovrtime.value)+',"fancycletime":'+t2s(a.fantime.value)
 s+=',"awaydelta":'+(+a.awaytemp.value*10).toFixed()+'}'
 ws.send(s)
}

function secsToTime(sec)
{
 date=new Date(1970,0,1)
 date.setSeconds(sec)
 d=date.getDate()-1
 d=d?d+'d ':''
 return d+date.toTimeString().replace(/.*(\d:\d{2}:\d{2}).*/, "$1")
}

function s2t(elap)
{
  m=Math.floor(elap/60)
  s=elap-(m*60)
  if(m==0) return s
  if(s<10) s='0'+s
  return m+':'+s
}

function t2s(v)
{
  if(typeof v=='string') v=(+v.substr(0, v.indexOf(':'))*60)+(+v.substr(v.indexOf(':')+1))
  return v
}
//--></script>
</head>
<body onload="{
 myStorage3=localStorage.getItem('myStoredText3')
 if(myStorage3!=null)
  document.getElementById('ovrtemp').value=myStorage3
 startEvents()
}">
<strong><em>CuriousTech HVAC Remote</em></strong><br>
<font size=4>
<table style="width: 418px; height: 22px;" cellspacing="0">
<tr>
<td>IN</td><td><div id="intemp" class="style2">in</div></td><td>&deg;</td><td> &gt;</td>
<td><div id="target" class="style2">trg</div></td><td>&deg; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; </td>
<td>OUT</td><td><div id="outtemp" class="style2">out</div></td><td>&deg; &nbsp; &nbsp; </td>
<td> &nbsp; &nbsp; &nbsp; </td>
</tr>
</table>
</font>
<table style="width: 418px" cellspacing="0" cellpadding="0">
<tr>
<td id="fanCell"><div id="fan">FAN OFF</div></td>
<td align="right"><input type="button" value="AUTO" name="fAuto" onClick="{setfan(0)}"></td>
<td width="40"><input type="button" value=" ON " name="fOn" onClick="{setfan(1)}"></td>
<td width=300><input type="button" value="CYCLE" name="fCyc" onClick="{setMode(4)}"> &nbsp; <input type="submit" value="SETTINGS" onClick="window.location='/settings';"></td>
</tr>
<tr>
<td id="runCell"><div id="run">Cooling</div></td>
<td align="right"><input type="button" value=" OFF " name="mOff" onClick="{setMode(0)}"></td>
<td><input type="button" value="COOL" name="mCool" onClick="{setMode(1)}"><input type="button" value="HEAT " name="mHeat" onClick="{setMode(2)}"></td>
<td><input type="button" value=" AUTO " name="mAuto" onClick="{setMode(3)}"> &nbsp; &nbsp;<input type="submit" value="  CHART  " align="right" onClick="window.location='/chart.html';">
</td>
</tr>
<tr>
<td>&nbsp;</td><td></td><td></td><td></td>
</tr>
<tr>
<td>COOL HI</td><td><input type=text size=3 id="coolh" onChange="{setVars()}"></td><td><input type="button" value="+1" onClick="{incCool(1)}"></td><td><div id="time"></div></td>
</tr>
<tr>
<td style="width: 81px">COOL LO</td>
<td style="width: 44px"><input type=text size=3 id="cooll" onChange="{setVars()}"></td>
<td style="width: 200px"><input type="button" value=" -1" onClick="{incCool(-1)}"></td>
<td><input type="button" value=" HP " name="hHP" onClick="{setHeatMode(0)}"><input type="button" value="GAS " name="hGas" onClick="{setHeatMode(1)}"><input type="button" value="AUTO" name="hAuto" onClick="{setHeatMode(2)}"></td>
</tr>
<tr>
<td>HEAT HI</td>
<td><input type=text size=3 id="heath" onChange="{setVars()}"></td>
<td><input type="button" value="+1" onClick="{incHeat(1)}"></td>
<td></td>
</tr>
<tr>
<td>HEAT LO</td>
<td><input type=text size=3 id="heatl" onChange="{setVars()}"></td>
<td><input type="button" value=" -1" onClick="{incHeat(-1)}"></td>
<td id="hmCell"><div id="hm"></div></td>
</tr>
<tr>
<td> &nbsp;</td>
<td></td>
<td></td>
<td>
<input type="button" value="OFF " name="hmOff" onClick="{setHumidMode(0)}">
</td>
</tr>
<tr>
<td>OVR TIME</td><td><input type=text size=3 id="ovrtime"></td><td><input type="button" value="  GO  " onClick="{localStorage.setItem('myStoredText3', a.ovrtemp.value);setOvrTemp()}"></td>
<td>
<input type="button" value="FAN " name="hmFan" onClick="{setHumidMode(1)}"> &nbsp; HI <input type=text size=2 id="humidh" onchange="{setVar('humidh',(+this.value*10).toFixed())}"></td>
</tr>
<tr>
<td id="ovrCell">OVRRD &Delta;</td>
<td><input type=text size=3 id="ovrtemp" onChange="{setVars()}"></td>
<td><input type="button" value="STOP" onClick="{cancelOvr()}">
</td>
<td><input type="button" value="RUN" name="hmRun" onClick="{setHumidMode(2)}">&nbsp; &nbsp; &nbsp; &nbsp; <input id="rh" size="2" disabled></td>
</tr>
<tr>
<td>FRESHEN</td>
<td><input type=text size=3 id="fantime" onChange="{setVars()}"></td>
<td><input type="button" style="margin-left:200" value="  GO  " onClick="{setfan(3)}"></td>
<td><input type="button" value="  A1 " name="hmAuto1" onClick="{setHumidMode(3)}"> &nbsp; LO <input type=text size=2 id="humidl" onchange="{setVar('humidl',(+this.value*10).toFixed())}">
</td>
</tr>
<tr>
<td>AWAY &Delta;</td><td><input type=text size=3 id="awaytemp" onChange="{setVars()}"></td><td><input type="button" value="AWAY" name="away" onClick="{setAway()}"></td>
<td><input type="button" value="  A2 " name="hmAuto2" onClick="{setHumidMode(4)}"></td>
</tr>
</table>
<table style="width: 418px">
<tr>
<td>CYCLE</td><td><div id="cyctimer" style="width: 70px">0</div></td>
<td>TOTAL</td><td><input type="button" id ="runtotal" value="0" onClick="{rstTot()}"></td>
<td>&nbsp; FILTER</td><td><input type="button" id ="filter" value="0" onClick="{rstFlt()}"></td>
</tr>
</table>
<small>&copy; 2016 CuriousTech.net</small>
</body>
</html>
)rawliteral";

///////////////////////
const char page_settings[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<head>
<title>ESP-HVAC</title>
<link rel="stylesheet" type="text/css" href="styles.css">
<style type="text/css">
body{width:340px;display:block;font-family: Arial, Helvetica, sans-serif;}
</style>
<script type="text/javascript">
<!--

var Json,ovrActive,rmtMode
var a=document.all
snd=new Array()
var ws
function startEvents()
{
ws=new WebSocket("ws://"+window.location.host+"/ws")
//ws=new WebSocket("ws://192.168.31.46/ws")
ws.onopen=function(evt){}
ws.onclose=function(evt){alert("Connection closed.");}

ws.onmessage = function(evt){
 console.log(evt.data)
 lines=evt.data.split(';')
 event=lines[0]
 data=lines[1]
 Json=JSON.parse(data)
 if(event == 'settings')
 {
  a.idlemin.value=s2t(+Json.im)
  a.cycmin.value=s2t(+Json.cn)
  a.cycmax.value=s2t(+Json.cx)
  a.thresh.value=+Json.ct/10
  a.fandelay.value=s2t(+Json.fd)
  a.fanpre.value=s2t(+Json.fp)
  a.awaytime.value=s2t(+Json.at)
  a.heatthr.value=+Json.ht
  a.ppkwh.value=+Json.ppk/1000
  a.ccf.value=+Json.ccf/1000
  a.cfm.value=+Json.cfm/1000
  a.fcr.value=+Json.fcr
  a.fcd.value=+Json.fcd
  a.fco.value=+Json.fco
  a.acth.value=+Json.dl/10
  a.fim.value=s2t(+Json.fim)
  a.far.value=s2t(+Json.far)
 }
 else if(event == 'state')
 {
  a.loc.innerHTML=(+Json.it/10).toFixed(1)+' '+(+Json.rh/10).toFixed(1)+'%'
  snd=Json.snd
  if(snd) setSenders()
 }
 else if(event == 'alert')
 {
  alert(data)
 }
}
}

function setSenders()
{
 for(i=0;i<5;i++)
 {
  item=document.getElementById('snd'+i)
  item.setAttribute('style',i<snd.length?'':'visibility:collapse')
 }
 item=document.getElementById('int')
 item.setAttribute('style',snd.length?'':'visibility:collapse')

 for(i=0;i<snd.length;i++)
 {
  document.getElementById('shr'+i).value=snd[i][4]
  document.getElementById('sndpri'+i).setAttribute('class',snd[i][3]&1?'style5':'')
  document.getElementById('snda'+i).setAttribute('class',snd[i][3]&2?'style5':'')
  document.getElementById('rt'+i).innerHTML=(snd[i][1])?(snd[i][1]/10).toFixed(1)+' '+(snd[i][2]/10).toFixed(1)+'%':''
 }
}

function jmp(v)
{
  window.location.assign('http://'+snd[+v.charAt(3)][0])
}

function setVar(varName, value)
{
  ws.send('cmd;{"key":"'+a.myToken.value+'","'+varName+'":'+value+'}')
}

function setSnd(n,v)
{
  setVar('rmtid',snd[n][0].split('.')[3])
  flg=(snd[n][3]&1<<v)?0x100:0;
  flg|=1<<v
  snd[n][3]^=1<<v
  setVar('rmtflg',flg)
  setSenders()
}

function secsToTime(sec)
{
 date=new Date(1970,0,1)
 date.setSeconds(sec)
 d=date.getDate()-1
 d=d?d+'d ':''
 return d+date.toTimeString().replace(/.*(\d:\d{2}:\d{2}).*/, "$1")
}

function s2t(elap)
{
  m=Math.floor(elap/60)
  s=elap-(m*60)
  if(m==0) return s
  if(s<10) s='0'+s
  return m+':'+s
}

function t2s(v)
{
  if(typeof v == 'string') v = (+v.substr(0, v.indexOf(':'))*60) + (+v.substr(v.indexOf(':')+1))
  return v
}
//--></script>
</head>
<body onload="{
 myStorage1=localStorage.getItem('myStoredText1')
 if(myStorage1!= null){
  document.getElementById('myToken').value=myStorage1
 }
 startEvents()
}" align="center">
<strong><em>CuriousTech HVAC Settings</em></strong><br><br>
<table style="width: 290px" cellspacing=0 cellpadding=0>
<tr>
<td style="width: 100px">Threshold</td>
<td style="width: 90px"><input type=text size=4 id="thresh" onchange="{setVar('cyclethresh',(+this.value*10).toFixed())}"></td>
<td style="width: 20px"></td><td><input type="submit" value=" Home " onClick="window.location='/iot';"></td></tr>
<tr><td>Heat Thresh</td><td><input type=text size=4 id="heatthr" onchange="{setVar('eheatthresh',+this.value)}"></td><td></td><td><input type="submit" value=" Chart " onClick="window.location='/chart.html';"></td></tr>
<tr><td>AC &#x2202; Limit</td><td><input type=text size=4 id="acth" onchange="{setVar('dl',(+this.value*10).toFixed())}"></td><td></td><td></td></tr>
<tr><td>Fan Pre</td><td><input type=text size=4 id="fanpre" onchange="{setVar('fanpretime',t2s(this.value))}"></td><td>Post</td><td><input type=text size=3 id="fandelay" onchange="{setVar('fanpostdelay',t2s(this.value))}"></td></tr>
<tr><td>cycle Min</td><td><input type=text size=4 id="cycmin" onchange="{setVar('cyclemin',t2s(this.value))}"></td><td>Max</td><td><input type=text size=3 id="cycmax" onchange="{setVar('cyclemax',t2s(this.value))}"></td></tr>
<tr><td>Idle Min</td><td><input type=text size=4 id="idlemin" onchange="{setVar('idlemin',t2s(this.value))}"></td><td>PKW</td><td><input type=text size=3 id="ppkwh" onchange="{setVar('ppk',(+this.value*1000).toFixed())}"></td></tr>
<tr><td>Away Limit</td><td><input type=text size=4 id="awaytime" onchange="{setVar('awaytime',t2s(this.value))}"></td><td>CFM</td><td><input type=text size=3 id="cfm" onchange="{setVar('cfm',(+this.value*1000).toFixed())}"></td></tr>
<tr><td>FC Shift</td><td><input type=text size=4 id="fco" onchange="{setVar('fco',this.value)}"></td><td>CCF</td><td><input type=text size=3 id="ccf" onchange="{setVar('ccf',(+this.value*1000).toFixed())}"></td></tr>
<tr><td>Lookahead</td><td><input type=text size=4 id="fcr" onchange="{setVar('fcrange',this.value)}"></td><td>Disp</td><td><input type=text size=3 id="fcd" onchange="{setVar('fcdisp',this.value)}"></td></tr>
<tr><td>Fan Auto Run</td><td><input type=text size=4 id="fim" onchange="{setVar('fim',t2s(this.value))}"></td><td>Run</td><td><input type=text size=3 id="far" onchange="{setVar('far',t2s(this.value))}"></td></tr>
<tr id="int" style="visibility:collapse"><td></td><td>Effective</td><td id="loc" colspan=2></td><td></td></tr>
<tr id="snd0" style="visibility:collapse"><td id="s0"><input type="submit" ID="shr0"></td><td><input type="button" value="Pri" id="sndpri0" onClick="{setSnd(0,0)}"><input type="button" value="En" id="snda0" onClick="{setSnd(0,1)}"></td><td id="rt0" colspan=2></td><td></td></tr>
<tr id="snd1" style="visibility:collapse"><td id="s1"><input type="submit" ID="shr1" onClick="{jmp(this.id)}"></td><td><input type="button" value="Pri" id="sndpri1" onClick="{setSnd(1,0)}"><input type="button" value="En" id="snda1" onClick="{setSnd(1,1)}"></td><td id="rt1" colspan=2></td><td></td></tr>
<tr id="snd2" style="visibility:collapse"><td id="s2"><input type="submit" ID="shr2" onClick="{jmp(this.id)}"></td><td><input type="button" value="Pri" id="sndpri2" onClick="{setSnd(2,0)}"><input type="button" value="En" id="snda2" onClick="{setSnd(2,1)}"></td><td id="rt2" colspan=2></td><td></td></tr>
<tr id="snd3" style="visibility:collapse"><td id="s3"><input type="submit" ID="shr3" onClick="{jmp(this.id)}"></td><td><input type="button" value="Pri" id="sndpri3" onClick="{setSnd(3,0)}"><input type="button" value="En" id="snda3" onClick="{setSnd(3,1)}"></td><td id="rt3" colspan=2></td><td></td></tr>
<tr id="snd4" style="visibility:collapse"><td id="s4"><input type="submit" ID="shr4" onClick="{jmp(this.id)}"></td><td><input type="button" value="Pri" id="sndpri4" onClick="{setSnd(4,0)}"><input type="button" value="En" id="snda4" onClick="{setSnd(4,1)}"></td><td id="rt4" colspan=2></td><td></td></tr>
<tr id="snd5" style="visibility:collapse"><td id="s5"><input type="submit" ID="shr5" onClick="{jmp(this.id)}"></td><td><input type="button" value="Pri" id="sndpri5" onClick="{setSnd(5,0)}"><input type="button" value="En" id="snda5" onClick="{setSnd(5,1)}"></td><td id="rt5" colspan=2></td><td></td></tr>
<tr id="snd6" style="visibility:collapse"><td id="s6"><input type="submit" ID="shr6" onClick="{jmp(this.id)}"></td><td><input type="button" value="Pri" id="sndpri6" onClick="{setSnd(6,0)}"><input type="button" value="En" id="snda6" onClick="{setSnd(6,1)}"></td><td id="rt6" colspan=2></td><td></td></tr>
</table>
<table style="width: 290px">
<tr><td>Password</td><td><input id="myToken" name="access_token" type=text size=40 placeholder="password" style="width: 98px"
 onChange="{
 localStorage.setItem('myStoredText1', a.myToken.value)
 alert(a.myToken.value+' Has been stored')
}">
</td>
</tr>
</table>
<small>&copy; 2016 CuriousTech.net</small>
</body>
</html>
)rawliteral";

//////////////////////
const char page_chart[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>HVAC Chart</title>
<link rel="stylesheet" type="text/css" href="styles.css">
<style type="text/css">
div{
border-radius: 1px;
margin-bottom: 1px;
box-shadow: 2px 2px 12px #000000;
background: rgb(160,160,160);
background: linear-gradient(0deg, rgba(160,160,160,1) 0%, rgba(176,176,176,1) 100%);
background-clip: padding-box;
}
.dropdown{
 position: relative;
 display: inline-block;
}
.btn{
 background-color: #a0a0a0;
 padding: 1px;
 font-size: 12px;
 min-width: 50px;
 border: none;
}
.dropdown-content{
 display: none;
 position: absolute;
 min-width: 40px;
 min-height: 1px;
 z-index: 1;
}
.dropdown:hover .dropdown-content{display: block;}
body{background:silver;width:700px;display:block;text-align:center;font-family: Arial, Helvetica, sans-serif;}}
</style>
<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.6.1/jquery.min.js" type="text/javascript" charset="utf-8"></script>
<script type="text/javascript">
var graph;
xPadding=30
yPadding=56
drawMode=3
schedMode=1
var yRange
var Json
var a=document.all
var ws
added=false
$(document).ready(function()
{
 myStorage1 = localStorage.getItem('myStoredText1')
 if(myStorage1  != null) myToken=myStorage1
 ws=new WebSocket("ws://"+window.location.host+"/ws")
// ws=new WebSocket("ws://192.168.31.46/ws")
 ws.onopen=function(evt){ws.send('cmd;{sum:0}')}
 ws.onclose=function(evt){alert("Connection closed.")}
 ws.onmessage = function(evt){
  console.log(evt.data)
  lines=evt.data.split(';')
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
      schedMode=+Json.sm
      fco=+Json.fco
      so=+Json.so
      if(schedMode==0) a.fco.value=fco
      else a.fco.value=so
      a.fcr.value=fcr=+Json.fcr
      ct=+Json.ct
      c0=+Json.c0
      c1=+Json.c1
      h0=+Json.h0
      h1=+Json.h1
      a.lo.value=(iMin=(md==2)?h0:c0)/10
      a.hi.value=(iMax=(md==2)?h1:c1)/10
      a.ct.value=ct/10
      break
    case 'state':
      sJson=Json
      cyc=secsToTime(+Json.ct)
      snd=Json.snd
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
      fc=Json.fc
      fcDate=Json.fcDate
      fcFreq=Json.fcFreq
      draw_bars()
      drawFC()
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
        n=Json.d[i][0]      // time, temp, rh, thrsh, state outtemp
        Json.d[i][0]=tb*1000
        tb-=n
        Json.d[i][1]+=tm
        Json.d[i][2]+=rm
        Json.d[i][3]+=lm
        Json.d[i][5]+=om
        for(j=6;j<Json.d[i].length;j++)
         Json.d[i][j]+=tm
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
  ws.send('cmd;{data:'+s+'}'); },60000)
});

function draw(){
  graph = $('#graph')
  c=graph[0].getContext('2d')

  tipCanvas=document.getElementById("tip")
  tipCtx=tipCanvas.getContext("2d")
  tipDiv=document.getElementById("popup")

  c.fillStyle='black'
  c.strokeStyle='black'
  canvasOffset=graph.offset()
  offsetX=canvasOffset.left
  offsetY=canvasOffset.top
  h=graph.height()-yPadding
  c.lineWidth=2
  c.font='italic 8pt sans-serif'
  c.textAlign="left"
  yPad=yPadding
  doBorder(graph)

  c.lineWidth = 1
  // dates
  if(typeof(arr)=="undefined") return
  step=Math.floor(arr.length/15)
  if(step==0) step=1
  for(i=0;i<arr.length-1;i+=step){
  c.save()
  c.translate(getXPixel(i),h+5)
  c.rotate(0.9)
  date = new Date(arr[i][0])
  c.fillText(date.toLocaleTimeString(),0,0)
  c.restore()
  }

  yRange=getMaxY()-getMinY()
  // value range
  c.textAlign="right"
  c.textBaseline="middle"

  for(var i=getMinY();i<getMaxY();i+=(yRange/8))
  c.fillText((i/10).toFixed(1),graph.width()-6,getYPixel(i))

  c.fillText('TEMP', graph.width(),6)
  c.fillStyle = +sJson.r?(md==2?"red":"blue"):(+sJson.fr?"green":"slategray")
  c.fillText((+sJson.it/10).toFixed(1),graph.width()-6,getYPixel(+sJson.it) )
 // cycle
  c.fillText(cyc,graph.width()-xPadding-7,h-8)

  c.fillStyle="green"
  c.fillText('RH',xPadding-6,6)

  // rh scale
  for(i=0;i<10;i++){
  pos=h-8-((h/10)*i)
  c.fillText(i*10,xPadding-4,pos)
  }

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
  if(drawMode&1)
  {
   date=new Date(arr[0][0])
   dt=date.getDate()
   for(i=1;i<arr.length;i++){
  c.strokeStyle=stateColor(arr[i][4])
  c.beginPath()
  c.moveTo(getXPixel(i),getYPixel(arr[i][1]))
  c.lineTo(getXPixel(i-1),getYPixel(arr[i-1][1]))
  c.stroke()
  date=new Date(arr[i][0])
  if(dt!=date.getDate())
  {
    dt = date.getDate()
    c.strokeStyle = '#000'
    c.beginPath() // borders
    c.moveTo(getXPixel(i),0)
    c.lineTo(getXPixel(i),h)
    c.stroke()
  }
   }
  }

  if(arr[0].length>6)
  {
  if(drawMode&4) doLines('rgba(0,0,255,0.5)',6)
  if(drawMode&8) doLines('rgba(255,255,255,0.7)',7)
  if(drawMode&16) doLines('rgba(0,0,50,0.6)',8)
  }
  c.textAlign="left"
  y=graph.height()
  c.fillStyle='#000'
  c.fillText('TEMP',2,y-=10)
  c.fillStyle='#0f0'
  c.fillText('RH',2,y-=10)
  c.fillStyle='rgba(0,0,255)'
  c.fillText(snd[0][4],2,y-=10)
  c.fillStyle='rgba(255,255,255)'
  y-=10
  if(snd[1]) c.fillText(snd[1][4],2,y)
  c.fillStyle='rgba(0,0,50)'
  y-=10
  if(snd[2]) c.fillText(snd[2][4],2,y)
  c.fillStyle='#fa0'
  c.fillText('OUT',2,y-=10)

  // out temp
  if(drawMode&32) doLines('#fa0',5,'OUT')
  if(drawMode&2){
  c.strokeStyle = '#0f0'
  c.beginPath()
  c.moveTo(getXPixel(0),getRHPixel(arr[0][2]))
  for(var i=1;i<arr.length;i ++)
  c.lineTo(getXPixel(i),getRHPixel(arr[i][2]))
  c.stroke()
  }
  dots=[]
  for(i=0;i<arr.length;i++){
    dots.push({
      x: getXPixel(i),
      y: getYPixel(arr[i][1]),
      r: 4,
      rXr: 16,
      color: "red",
      tip: (new Date(arr[i][0])).toLocaleTimeString()+' ',
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
    var hit=false
    for(i=0;i<dots.length;i++){
      dot=dots[i]
      dx=mouseX-dot.x
      dy=mouseY-dot.y
      if(dx*dx+dy*dy<dot.rXr){
        tipCtx.clearRect(0,0,tipCanvas.width,tipCanvas.height)
        tipCtx.lineWidth=2
        tipCtx.fillStyle="#000000"
        tipCtx.strokeStyle='#333'
        tipCtx.font='italic 8pt sans-serif'
        tipCtx.textAlign="left"

        tipCtx.fillText(dot.tip,4,15)
        tipCtx.fillText(dot.tip2+'°F',4,29)
        tipCtx.fillText(dot.tip3+'%',4,44)
        tipCtx.fillText(dot.tip4 + '°F',4,58)
        hit=true
        popup=document.getElementById("popup")
        popup.style.top=dot.y+"px"
        popup.style.left=(dot.x-60)+"px"
      }
    }
    if(!hit){popup.style.left="-200px"}
  }

  mousePos={x:0,y:0}
  if(added==false)
  {
    graph[0].addEventListener("mousedown",function(e){
      mouseX=parseInt(e.clientX-offsetX)
      mouseY=parseInt(e.clientY-offsetY)
      drawMode^=1<<((graph[0].height-10-mouseY)/10).toFixed()
      draw()
    },false)
    added=true
  }
  function doLines(ss,os)
  {
    c.strokeStyle=ss
    c.beginPath()
    c.moveTo(getXPixel(0),getYPixel(arr[0][os]))
    for(i=1;i<arr.length;i++)
      c.lineTo(getXPixel(i),getYPixel(arr[i][os]))
    c.stroke()
  }
}

function getMaxY(){
  max=0
  for(i=0; i<arr.length-1; i++)
  {
    if(arr[i][1] > max)
      max=arr[i][1]
    if(arr[i][3]+th>max)
      max=arr[i][3]+th
    if(drawMode&32&&arr[i][5]>max)
      max=arr[i][5]
    for(j=6;j<arr[i].length;j++)
      if(drawMode&1<<(j-4)&&arr[i][j]>max)
      max=arr[i][j]
  }
  return Math.ceil(max)
}

function getMinY(){
  min=1500
  for(i=0; i<arr.length; i++)
  {
    if(arr[i][1]<min)
      min=arr[i][1]
    if(arr[i][3]<min)
      min=arr[i][3]
    if(drawMode&32&&arr[i][5]<min)
      min=arr[i][5]
    for(j=6;j<arr[i].length;j++)
      if(drawMode&1<<(j-4)&&arr[i][j]<min)
      min=arr[i][j]
  }
  return Math.floor(min)
}

function getXPixel(val){
  x=(graph.width()-xPadding)-((graph.width()-26-xPadding)/arr.length)*val
  return x.toFixed()
}

function getYPixel(val){
  y=graph.height()-(((graph.height()-yPadding)/yRange)*(val-getMinY()))-yPadding
  return y.toFixed()
}

function getRHPixel(val){
  return graph.height()-(((graph.height()-yPadding)/1000)*val)-yPadding
}

function stateColor(s)
{
  sts=Array('gray','blue','yellow','red')
  if(s==1) return 'cyan'
  return sts[s>>1]
}

function setVar(varName,value)
{
  ws.send('cmd;{"key":"'+myToken+'","'+varName+'":'+value+'}')
}

function secsToTime(sec)
{
  dt=new Date(1970,0,1)
  dt.setSeconds(sec)
  d=dt.getDate()-1
  d=d?d+'d ':''
  return d+dt.toTimeString().replace(/.*(\d:\d{2}:\d{2}).*/, "$1")
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
        tipCtx.clearRect(0,0,tipCanvas.width, tipCanvas.height)
        tipCtx.fillStyle="#000000"
        tipCtx.strokeStyle='#333'
        tipCtx.font='italic 8pt sans-serif'
        tipCtx.textAlign="left"
        tipCtx.fillText(dot.tip, 4,15)
        tipCtx.fillText(dot.tip2,4,29)
        tipCtx.fillText(dot.tip3,4,44)
        tipCtx.fillText(dot.tip4,4,59)
        tipCtx.fillText(dot.tip5,4,75)
        hit=true
        popup=document.getElementById("popup")
        popup.style.top=(dot.y+rect.y+window.pageYOffset)+"px"
        x=dot.x+rect.x-60
        if(x<10)x=10
        popup.style.left=x+"px"
      }
    }
    if(!hit){popup.style.left="-200px"}
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
      tip: 'AC\t\t'+secsToTime(ar[i][0]),
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

function doBorder(g)
{
  c.clearRect(0, 0, g.width(), g.height())
  c.beginPath()
  c.moveTo(xPadding,0)
  c.lineTo(xPadding,g.height()-yPad)
  c.lineTo(g.width()-xPadding, g.height()-yPad)
  c.lineTo(g.width()-xPadding, 0)
  c.stroke()
}

function drawFC(){
  graph2 = $('#graph2')
  c=graph2[0].getContext('2d')

  c.fillStyle='black'
  c.strokeStyle='black'
  canvasOffset=graph2.offset()
  offsetX=canvasOffset.left
  offsetY=canvasOffset.top
  yPad=18
  if(fcr>fc.length) fcr=fc.length
  if(fcr==0) fcr=23
  c.lineWidth=2
  c.font='italic 8pt sans-serif'
  c.textAlign="left"

  doBorder(graph2)

  c.lineWidth=1
  min=150
  max=-30
  for(i=0;i<fc.length;i++)
  {
    if(min>fc[i]) min=fc[i]
    if(max<fc[i]) max=fc[i]
  }
  max++
  yRange=max-min
  c.textAlign="right"
  c.textBaseline="middle"
  c.fillStyle='black'
  
  // right legend
  for(i=min;i<max;i+=(yRange/8))
    c.fillText(i.toFixed(1),graph2.width()-6,getYPixel2(i))
  c.fillText('OUT',graph2.width()-6,6)

  c.textAlign="left"
  if(iMax==iMin) iMax+=1
  iRng=iMax-iMin
  c.fillText(iMax/10,6,getYPixel3(iMax))
  c.fillText(iMin/10,6,getYPixel3(iMin))

  c.fillStyle='#40404050'
  w=graph2.width()-xPadding*2
  c.fillRect(xPadding,getYPixel3(iMax),w,getYPixel3(iMin)-getYPixel3(iMax))

  // temp lines
  c.fillStyle="red"
  fcl=fc.length
  if(fcl==0){
    fcl=58
    fcDate=(new Date()).valueOf()/1000-(60*60*24)
    fcFreq=10800
    fcr=23
  }
  cPos=0
  fl=(fcDate+fcFreq*(fcl-1))/60
  date=new Date(fcDate*1000)
  dt=date.getDate()
  grd=c.createLinearGradient(0,0,0,graph2.height()-18)
  if(max<=32) grd.addColorStop(0,"blue")
  else if(min<32)
  {
    fr=1-((32-min)/yRange)
    grd.addColorStop(fr,"red")
    grd.addColorStop(fr,"blue")
  }
  else{grd.addColorStop(0,"red");grd.addColorStop(1,"blue")}
  for(i=1;i<fcl;i++){
    if(fc.length){
      c.strokeStyle=grd
      c.beginPath()
      c.moveTo(getXPixel2(i),getYPixel2(fc[i]))
      c.lineTo(getXPixel2(i-1),getYPixel2(fc[i-1]))
      c.stroke()
    }
    date = new Date((fcDate+fcFreq*i)*1000)
    if(cPos==0&&date.valueOf()>=(new Date()).valueOf())
    {
      dif=(date.valueOf()-(new Date().valueOf()))/60000
      xOff=w/fl*dif
      cPos=i;
      c.strokeStyle='#fff'
      c.beginPath()
      c.moveTo(getXPixel2(cPos)-xOff,graph2.height()-18)
      c.lineTo(getXPixel2(cPos)-xOff,1)
      c.stroke()
      c.fillStyle='#000'
      c.textAlign="center"
      c.fillText("Now",getXPixel2(cPos)-xOff,getYPixel3(iMax)-8)
    }
    if(dt!=date.getDate()){
      dt=date.getDate()
      c.strokeStyle='#555'
      c.beginPath()
      c.moveTo(getXPixel2(i),0)
      c.lineTo(getXPixel2(i),graph2.height()-18)
      c.stroke()
      c.fillStyle='#000'
      c.textAlign="left"
      c.fillText(date.toLocaleDateString(),getXPixel2(i),graph2.height()-8)
    }
  }
  xOff=w/fl*fco
  c.fillStyle = "#9050F090"
  c.beginPath()
  strt=cPos-fcr
  if(strt<0) strt=0
  date=new Date(fcDate*1000)
  c.moveTo(getXPixel2(strt)-xOff,getTT(strt,0))
  for(i=strt+1;i<=cPos+fcr;i++)
  {
    date=new Date((fcDate+(fcFreq*i))*1000)
    c.lineTo(getXPixel2(i)-xOff,getTT(i,0))
  }
  for(i=cPos+fcr;i>=strt;i--)
  {
    date=new Date((fcDate+(fcFreq*i))*1000)
    c.lineTo(getXPixel2(i)-xOff,getTT(i,(md==2)?ct:-ct))
  }
  c.closePath()
  c.fill()
}
function getXPixel2(val){
  x=xPadding+((graph2.width()-xPadding*2)/fcl)*val
  return +x.toFixed()
}

function getYPixel2(val){
  h=graph2.height()-18
  y=h-((h/yRange)*(val-min))
  return y.toFixed()
}
function getYPixel3(val){
  h=graph2.height()-18
  o=70+ct*2
  return h-(o/2)-((h-o)/iRng*(val-iMin))
}
function getTT(i,th)
{
 if(schedMode==0)
 {
  min2=150
  max2=-30
  strt1=i-fcr
  if(strt1<0) strt1=0
  for(j=strt1;j<i+fcr;j++)
  {
    if(j<fc.length){
      if(min2>fc[j]) min2=fc[j]
      if(max2<fc[j]) max2=fc[j]
    }
  }
  tt=(fc[i]-min2)*iRng/(max2-min2)+iMin+th
 }else if(schedMode==1){
  m=((date.getHours()+14)*60+date.getMinutes()+so)/4
  r=(iRng/2)*Math.sin(Math.PI*(180-m)/180)
  tt=r+iMin+th
  if(md==2) tt+=iRng/2
  else tt-=iRng/2
  }else if(schedMode==2){
  tt=iMin+th
 }
 h=graph2.height()-18
 o=70+ct*2
 return h-o/2-(h-o)/iRng*(tt-iMin)
}
function setSched(n)
{
 schedMode=n
 setVar('sm',n)
 if(n==0) a.fco.value=fco
 else a.fco.value=so
 drawFC()
}
function setShift(n)
{
 if(schedMode==0) fco=n
 else so=n
 setVar('fco',n)
 drawFC()
}
</script>
<style type="text/css">
#wrapper{
  width: 100%;
  height: 400px;
  position: relative;
}
#graph{
  width: 100%;
  height: 100%;
  position: absolute;
  top: 0;
  left: 0;
}
#popup{
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
#wrapper3{
  width: 100%;
  height: 170px;
  position: relative;
}
#graph2{
  width: 100%;
  height: 100%;
  position: absolute;
  top: 0;
  left: 0;
}
.style1{
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
<table width=700><tr>
<td>SHIFT:<input type=text size=1 id="fco" onchange="setShift(+this.value)">&nbsp; RANGE:<input type=text size=1 id="fcr" onchange="{fcr=+this.value;drawFC()}">
&nbsp; LOW:<input type=text size=1 id="lo" onchange="{iMin=(+this.value)*10;drawFC()}">
&nbsp; HIGH:<input type=text size=1 id="hi" onchange="{iMax=(+this.value)*10;drawFC()}">
&nbsp; THRESH:<input type=text size=1 id="ct" onchange="{ct=(+this.value)*10;drawFC()}">
</td><td>&nbsp;
<div class="dropdown">
  <button class="dropbtn">MODE:</button>
  <div class="dropdown-content">
  <button class="btn" id="s0" onclick="setSched(0)">FORECAST</button>
  <button class="btn" id="s1" onclick="setSched(1)">SINE</button>
  <button class="btn" id="s2" onclick="setSched(2)">FLAT</button>
  </div>
</div>
</td>
<td></td>
</tr></table>
<div id="wrapper3">
<canvas id="graph2" width="700" height="170"></canvas>
</div>
</body>
</html>
)rawliteral";

//////////////////////
const char page_styles[] PROGMEM = R"rawliteral(
table{
border-radius: 2px;
margin-bottom: 2px;
box-shadow: 4px 4px 10px #000000;
background: rgb(160,160,160);
background: linear-gradient(0deg, rgba(94,94,94,1) 0%, rgba(160,160,160,1) 90%);
background-clip: padding-box;
}
input{
border-radius: 2px;
margin-bottom: 2px;
box-shadow: 4px 4px 10px #000000;
background: rgb(160,160,160);
background: linear-gradient(0deg, rgba(160,160,160,1) 0%, rgba(239,255,255,1) 100%);
background-clip: padding-box;
}
.style1{border-width: 0;}
.style2{text-align: left;}
.style3{
border-radius: 5px;
margin-bottom: 5px;
box-shadow: 2px 2px 10px #000000;
background: rgb(95,194,230);
background: linear-gradient(0deg, rgba(95,194,230,1) 0%, rgba(79,79,79,1) 100%);
background-clip: padding-box;
}
.style4{
border-radius: 5px;
margin-bottom: 5px;
box-shadow: 2px 2px 10px #000000;
background: rgb(80,160,255);
background: linear-gradient(0deg, rgba(80,160,255,1) 0%, rgba(79,79,79,1) 100%);
background-clip: padding-box;
}
.style5 {
border-radius: 1px;
box-shadow: 2px 2px 10px #000000;
background: rgb(0,160,224);
background: linear-gradient(0deg, rgba(0,160,224,1) 0%, rgba(0,224,224,1) 100%);
}})rawliteral";

const uint8_t favicon[] PROGMEM = {
  0x1F, 0x8B, 0x08, 0x08, 0x70, 0xC9, 0xE2, 0x59, 0x04, 0x00, 0x66, 0x61, 0x76, 0x69, 0x63, 0x6F, 
  0x6E, 0x2E, 0x69, 0x63, 0x6F, 0x00, 0xD5, 0x94, 0x31, 0x4B, 0xC3, 0x50, 0x14, 0x85, 0x4F, 0x6B, 
  0xC0, 0x52, 0x0A, 0x86, 0x22, 0x9D, 0xA4, 0x74, 0xC8, 0xE0, 0x28, 0x46, 0xC4, 0x41, 0xB0, 0x53, 
  0x7F, 0x87, 0x64, 0x72, 0x14, 0x71, 0xD7, 0xB5, 0x38, 0x38, 0xF9, 0x03, 0xFC, 0x05, 0x1D, 0xB3, 
  0x0A, 0x9D, 0x9D, 0xA4, 0x74, 0x15, 0x44, 0xC4, 0x4D, 0x07, 0x07, 0x89, 0xFA, 0x3C, 0x97, 0x9C, 
  0xE8, 0x1B, 0xDA, 0x92, 0x16, 0x3A, 0xF4, 0x86, 0x8F, 0x77, 0x73, 0xEF, 0x39, 0xEF, 0xBD, 0xBC, 
  0x90, 0x00, 0x15, 0x5E, 0x61, 0x68, 0x63, 0x07, 0x27, 0x01, 0xD0, 0x02, 0xB0, 0x4D, 0x58, 0x62, 
  0x25, 0xAF, 0x5B, 0x74, 0x03, 0xAC, 0x54, 0xC4, 0x71, 0xDC, 0x35, 0xB0, 0x40, 0xD0, 0xD7, 0x24, 
  0x99, 0x68, 0x62, 0xFE, 0xA8, 0xD2, 0x77, 0x6B, 0x58, 0x8E, 0x92, 0x41, 0xFD, 0x21, 0x79, 0x22, 
  0x89, 0x7C, 0x55, 0xCB, 0xC9, 0xB3, 0xF5, 0x4A, 0xF8, 0xF7, 0xC9, 0x27, 0x71, 0xE4, 0x55, 0x38, 
  0xD5, 0x0E, 0x66, 0xF8, 0x22, 0x72, 0x43, 0xDA, 0x64, 0x8F, 0xA4, 0xE4, 0x43, 0xA4, 0xAA, 0xB5, 
  0xA5, 0x89, 0x26, 0xF8, 0x13, 0x6F, 0xCD, 0x63, 0x96, 0x6A, 0x5E, 0xBB, 0x66, 0x35, 0x6F, 0x2F, 
  0x89, 0xE7, 0xAB, 0x93, 0x1E, 0xD3, 0x80, 0x63, 0x9F, 0x7C, 0x9B, 0x46, 0xEB, 0xDE, 0x1B, 0xCA, 
  0x9D, 0x7A, 0x7D, 0x69, 0x7B, 0xF2, 0x9E, 0xAB, 0x37, 0x20, 0x21, 0xD9, 0xB5, 0x33, 0x2F, 0xD6, 
  0x2A, 0xF6, 0xA4, 0xDA, 0x8E, 0x34, 0x03, 0xAB, 0xCB, 0xBB, 0x45, 0x46, 0xBA, 0x7F, 0x21, 0xA7, 
  0x64, 0x53, 0x7B, 0x6B, 0x18, 0xCA, 0x5B, 0xE4, 0xCC, 0x9B, 0xF7, 0xC1, 0xBC, 0x85, 0x4E, 0xE7, 
  0x92, 0x15, 0xFB, 0xD4, 0x9C, 0xA9, 0x18, 0x79, 0xCF, 0x95, 0x49, 0xDB, 0x98, 0xF2, 0x0E, 0xAE, 
  0xC8, 0xF8, 0x4F, 0xFF, 0x3F, 0xDF, 0x58, 0xBD, 0x08, 0x25, 0x42, 0x67, 0xD3, 0x11, 0x75, 0x2C, 
  0x29, 0x9C, 0xCB, 0xF9, 0xB9, 0x00, 0xBE, 0x8E, 0xF2, 0xF1, 0xFD, 0x1A, 0x78, 0xDB, 0x00, 0xEE, 
  0xD6, 0x80, 0xE1, 0x90, 0xFF, 0x90, 0x40, 0x1F, 0x04, 0xBF, 0xC4, 0xCB, 0x0A, 0xF0, 0xB8, 0x6E, 
  0xDA, 0xDC, 0xF7, 0x0B, 0xE9, 0xA4, 0xB1, 0xC3, 0x7E, 0x04, 0x00, 0x00, 
};
