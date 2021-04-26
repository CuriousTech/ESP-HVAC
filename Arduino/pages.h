const char page_index[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<head>
<title>ESP-HVAC</title>
<link rel="stylesheet" type="text/css" href="styles.css">
<style type="text/css">
body{width:340px;display:block;font-family: Arial, Helvetica, sans-serif;}
</style>

<script type="text/javascript"><!--
var Json,mode,autoMode,heatMode,fanMode,running,fan,humidMode,ovrActive,away,rh
var a=document.all
var states = new Array('Idle','Cooling','HP Heat','NG Heat')
var ws
var myToken = localStorage.getItem('myStoredText1')
function startEvents()
{
ws = new WebSocket("ws://"+window.location.host+"/ws")
//ws = new WebSocket("ws://192.168.31.125/ws")
ws.onopen = function(evt){}
ws.onclose = function(evt){alert("Connection closed.");}

ws.onmessage = function(evt){
 lines = evt.data.split(';')
 event=lines[0]
 data=lines[1]
 if(event == 'settings')
 {
  Json=JSON.parse(data)
    mode= +Json.m
  autoMode= +Json.am
  heatMode= +Json.hm
  fanMode= +Json.fm
  humidMode= +Json.rhm
  ovrActive= +Json.ot
  setAtt()
  a.cooll.value= +Json.c0/10
  a.coolh.value= +Json.c1/10
  a.heatl.value= +Json.h0/10
  a.heath.value= +Json.h1/10
  a.humidl.value= +Json.rh0/10
  a.humidh.value= +Json.rh1/10
  a.ovrtime.value= s2t(+Json.ov)
  a.fantime.value= s2t(+Json.fct)
  a.awaytemp.value= +Json.ad/10
  if( +a.ovrtemp.value==0)
   a.ovrtemp.value= -2.0
 }
 else if(event == 'state')
 {
  Json=JSON.parse(data)
  running= +Json.r
  fan= +Json.fr
  rh= +Json.rh
  away=+Json.aw
  a.time.innerHTML=(new Date(+Json.t*1000)).toLocaleTimeString()
  a.intemp.innerHTML= (+Json.it/10).toFixed(1)
  a.rh.value= (+Json.rh/10).toFixed(1)+'%'
  a.target.innerHTML= (+Json.tt/10).toFixed(1)
  a.outtemp.innerHTML= (+Json.ot/10).toFixed(1)
  a.cyctimer.innerHTML=secsToTime(+Json.ct)
  a.runtotal.value=secsToTime(+Json.rt)
  a.filter.value=s2t(+Json.fm)
  a.fan.innerHTML=fan?"Fan On":"Fan Off"
  a.run.innerHTML=states[+Json.s]
  hon=+Json.h
  a.hm.innerHTML=hon?"Humidifier On":"Humidifier Off"
  a.hmCell.setAttribute('class',hon?'style5':'style1')
  setAtt()
 }
 else if(event == 'alert')
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
setVar('humidmode',humidMode=m)
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
a.fan.innerHTML = "Fan "+((fanMode==1)?"On":(fan?"On":"Off"))
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

a.hmOff.setAttribute('class',humidMode==0?'style5':'')
a.hmFan.setAttribute('class',humidMode==1?'style5':'')
a.hmRun.setAttribute('class',humidMode==2?'style5':'')
a.hmAuto1.setAttribute('class',humidMode==3?'style5':'')
a.hmAuto2.setAttribute('class',humidMode==4?'style5':'')
a.away.setAttribute('class',away?'style5':'')
}

function incCool(n)
{
a.coolh.value= +a.coolh.value+n
a.cooll.value= +a.cooll.value+n
setVars()
}

function incHeat(n)
{
a.heath.value= +a.heath.value+n
a.heatl.value= +a.heatl.value+n
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

function secsToTime( elap )
{
  d=0
  m=0
  h=Math.floor(elap/3600)
  if(h >23)
  {
    d=Math.floor(h/24)
    h-=(d*24)
  }
  else
  {
    m=Math.floor((elap-(h*3600))/60)
    s=elap-(h*3600)-(m*60)
    if(s<10) s='0'+s
    if(h==0)
    {
      if( m < 10) m='  '+m
      return '    '+m +':'+s
    }
  }
  if(m<10) m='0'+m
  if(h<10) h='  '+h
  if(d) return d+'d '+h+'h'
  return h+':'+m+':'+s
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
 myStorage3 = localStorage.getItem('myStoredText3')
 if(myStorage3  != null)
  document.getElementById('ovrtemp').value=myStorage3
 startEvents()
}">
<strong><em>CuriousTech HVAC Remote</em></strong><br>
<font size=4>
<p><table style="width: 350px; height: 22px;" cellspacing="0">
<tr>
<td>In</td><td><div id="intemp" class="style2">in</div></td><td>&deg</td><td> &gt;</td>
<td><div id="target" class="style2">trg</div></td><td>&deg &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp </td>
<td>Out</td><td><div id="outtemp" class="style2">out</div></td><td>&deg &nbsp &nbsp </td>
<td> &nbsp &nbsp &nbsp </td><td></td>
</tr>
</table>
</font></p>
<table style="width: 350px" cellspacing="0" cellpadding="0">
<tr>
<td id="fanCell"><div id="fan">Fan Off</div></td>
<td align="right"><input type="button" value="Auto" name="fAuto" onClick="{setfan(0)}"></td>
<td width="40"><input type="button" value=" On " name="fOn" onClick="{setfan(1)}"></td>
<td width=300 align="right"><input type="button" value="Cycle" name="fCyc" onClick="{setMode(4)}"> &nbsp &nbsp <input type="submit" value="Settings" onClick="window.location='/settings';"></td>
</tr>
<tr>
<td id="runCell"><div id="run">Cooling</div></td>
<td align="right"><input type="button" value=" Off " name="mOff" onClick="{setMode(0)}"></td>
<td><input type="button" value="Cool" name="mCool" onClick="{setMode(1)}"><input type="button" value="Heat " name="mHeat" onClick="{setMode(2)}"></td>
<td><input type="button" value="Auto" name="mAuto" onClick="{setMode(3)}"> &nbsp &nbsp &nbsp<input type="submit" value="  Chart  " align="right" onClick="window.location='/chart.html';">
</td>
</tr>
<tr>
<td>&nbsp</td><td></td><td></td><td></td>
</tr>
<tr>
<td>Cool Hi</td><td><input type=text size=3 id="coolh" onChange="{setVars()}"></td><td><input type="button" value="+1" onClick="{incCool(1)}"></td><td><div id="time"></div></td>
</tr>
<tr>
<td style="width: 81px">Cool Lo</td>
<td style="width: 44px"><input type=text size=3 id="cooll" onChange="{setVars()}"></td>
<td style="width: 200px"><input type="button" value=" -1" onClick="{incCool(-1)}"></td>
<td><input type="button" value=" HP " name="hHP" onClick="{setHeatMode(0)}"><input type="button" value="Gas " name="hGas" onClick="{setHeatMode(1)}"><input type="button" value="Auto" name="hAuto" onClick="{setHeatMode(2)}"></td>
</tr>
<tr>
<td>Heat Hi</td>
<td><input type=text size=3 id="heath" onChange="{setVars()}"></td>
<td><input type="button" value="+1" onClick="{incHeat(1)}"></td>
<td></td>
</tr>
<tr>
<td>Heat Lo</td>
<td><input type=text size=3 id="heatl" onChange="{setVars()}"></td>
<td><input type="button" value=" -1" onClick="{incHeat(-1)}"></td>
<td id="hmCell"><div id="hm">Humidifier Off</div></td>
</tr>
<tr>
<td></td>
<td></td>
<td></td>
<td>
<input type="button" value=" Off " name="hmOff" onClick="{setHumidMode(0)}">
</td>
</tr>
<tr>
<td>ovr Time</td><td><input type=text size=3 id="ovrtime"></td><td><input type="button" value="  Go  " onClick="{localStorage.setItem('myStoredText3', a.ovrtemp.value);setOvrTemp()}"></td>
<td>
<input type="button" value="Fan" name="hmFan" onClick="{setHumidMode(1)}"> Hi <input type=text size=2 id="humidh" onchange="{setVar('humidh',(+this.value*10).toFixed())}"></td>
</tr>
<tr>
<td id="ovrCell">Ovrrd &Delta;</td>
<td><input type=text size=3 id="ovrtemp" onChange="{setVars()}"></td>
<td><input type="button" value=" Stop " onClick="{cancelOvr()}">
</td>
<td><input type="button" value="Run" name="hmRun" onClick="{setHumidMode(2)}">&nbsp; &nbsp; &nbsp;<input id="rh" size="2" disabled></td>
</tr>
<tr>
<td>Freshen</td>
<td><input type=text size=3 id="fantime" onChange="{setVars()}"></td>
<td><input type="button" style="margin-left:200" value="  Go  " onClick="{setfan(3)}"></td>
<td><input type="button" value=" A1 " name="hmAuto1" onClick="{setHumidMode(3)}"> Lo <input type=text size=2 id="humidl" onchange="{setVar('humidl',(+this.value*10).toFixed())}">
</td>
</tr>
<tr>
<td>Away &Delta;</td><td><input type=text size=3 id="awaytemp" onChange="{setVars()}"></td><td><input type="button" value="Away" name="away" onClick="{setAway()}"></td>
<td><input type="button" value=" A2 " name="hmAuto2" onClick="{setHumidMode(4)}"></td>
</tr>
</table><br/>
<table style="width: 350px">
<tr>
<td>Cycle</td><td><div id="cyctimer" style="width: 70px">0</div></td>
<td>Total</td><td><input type="button" id ="runtotal" value="0" onClick="{rstTot()}"></td>
<td>Filter</td><td><input type="button" id ="filter" value="0" onClick="{rstFlt()}"></td>
</tr>
</table>
<small>&copy 2016 CuriousTech.net</small>
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
var states = new Array('Idle','Cooling','HP Heat','NG Heat')
snd=new Array()
var ws
function startEvents()
{
ws=new WebSocket("ws://"+window.location.host+"/ws")
//ws=new WebSocket("ws://192.168.31.125/ws")
ws.onopen=function(evt){}
ws.onclose=function(evt){alert("Connection closed.");}

ws.onmessage = function(evt){
// console.log(evt.data)
 lines=evt.data.split(';')
 event=lines[0]
 data=lines[1]
 Json=JSON.parse(data)
 if(event == 'settings')
 {
  a.idlemin.value= s2t(+Json.im)
  a.cycmin.value= s2t(+Json.cn)
  a.cycmax.value= s2t(+Json.cx)
  a.thresh.value= +Json.ct/10
  a.fandelay.value= s2t(+Json.fd)
  a.fanpre.value= s2t(+Json.fp)
  a.awaytime.value= s2t(+Json.at)
  a.heatthr.value= +Json.ht
  a.ppkwh.value= +Json.ppk/1000
  a.ccf.value= +Json.ccf/1000
  a.cfm.value= +Json.cfm/1000
  a.fcr.value= +Json.fcr
  a.fcd.value= +Json.fcd
  a.fco.value= +Json.fco
  a.acth.value= +Json.dl/10
  a.fim.value=s2t(+Json.fim)
  a.far.value=s2t(+Json.far)
 }
 else if(event == 'state')
 {
  a.it0.innerHTML= (+Json.it/10).toFixed(1)+' '+(+Json.rh/10).toFixed(1)+'%'
  a.loc.innerHTML= (+Json.lt/10).toFixed(1)+' '+(+Json.lh/10).toFixed(1)+'%'
  snd=Json.snd
  if(snd) setSenders()
 }
 else if(event == 'alert')
 {
  alert(data)
 }
 else if(event == 'print')
 {
//  a.console.value += data
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
  document.getElementById('s'+i).innerHTML=(snd[i][4].length)?snd[i][4]:snd[i][0]
  document.getElementById('sndpri'+i).setAttribute('class',snd[i][3]&1?'style5':'')
  document.getElementById('snda'+i).setAttribute('class',snd[i][3]&2?'style5':'')
  document.getElementById('rt'+i).innerHTML=(snd[i][1])?(snd[i][1]/10).toFixed(1)+' '+(snd[i][2]/10).toFixed(1)+'%':''
 }
}

function setVar(varName, value)
{
 ws.send('cmd;{"key":"'+a.myToken.value+'","'+varName+'":'+value+'}')
}

function setSnd(n,v)
{
  snd[n][3]^=1<<v;
  setVar('rmtid',snd[n][0])
  setVar('rmtflg',snd[n][3])
  setSenders()
}

function secsToTime( elap )
{
  d=0
  m=0
  h=Math.floor(elap/3600)
  if(h >23)
  {
    d=Math.floor(h/24)
    h-=(d*24)
  }
  else
  {
    m=Math.floor((elap-(h*3600))/60)
    s=elap-(h*3600)-(m*60)
    if(s<10) s='0'+s
    if(h==0)
    {
      if( m < 10) m='  '+m
      return '    '+m +':'+s
    }
  }
  if(m<10) m='0'+m
  if(h<10) h='  '+h
  if(d) return d+'d '+h+'h'
  return h+':'+m+':'+s
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
 myStorage1 = localStorage.getItem('myStoredText1')
 if(myStorage1  != null){
  document.getElementById('myToken').value=myStorage1
 }
 startEvents()
}" align="center">
<strong><em>CuriousTech HVAC Settings</em></strong><br><br>
<table style="width: 290px" cellspacing=0 cellpadding=0>
<tr>
<td style="width: 100px">Threshold</td>
<td style="width: 90px"><input type=text size=4 id="thresh" onchange="{setVar('cyclethresh',(+this.value*10).toFixed())}"></td>
<td style="width: 20px"></td>
<td>
<input type="submit" value=" Home " onClick="window.location='/iot';">
</td>
</tr>
<tr><td>Heat Thresh</td><td><input type=text size=4 id="heatthr" onchange="{setVar('eheatthresh',+this.value)}"></td><td></td><td></td></tr>
<tr><td>AC &#x2202 Limit</td><td><input type=text size=4 id="acth" onchange="{setVar('dl',(+this.value*10).toFixed())}"></td><td></td><td></td></tr>
<tr><td>Fan Pre</td><td><input type=text size=4 id="fanpre" onchange="{setVar('fanpretime',t2s(this.value))}"></td><td>Post</td><td><input type=text size=3 id="fandelay" onchange="{setVar('fanpostdelay',t2s(this.value))}"></td></tr>
<tr><td>cycle Min</td><td><input type=text size=4 id="cycmin" onchange="{setVar('cyclemin',t2s(this.value))}"></td><td>Max</td><td><input type=text size=3 id="cycmax" onchange="{setVar('cyclemax',t2s(this.value))}"></td></tr>
<tr><td>Idle Min</td><td><input type=text size=4 id="idlemin" onchange="{setVar('idlemin',t2s(this.value))}"></td><td>PKW</td><td><input type=text size=3 id="ppkwh" onchange="{setVar('ppk',(+this.value*1000).toFixed())}"></td></tr>
<tr><td>Away Limit</td><td><input type=text size=4 id="awaytime" onchange="{setVar('awaytime',t2s(this.value))}"></td><td>CFM</td><td><input type=text size=3 id="cfm" onchange="{setVar('cfm',(+this.value*1000).toFixed())}"></td></tr>
<tr><td>FC Shift</td><td><input type=text size=4 id="fco" onchange="{setVar('fco',this.value)}"></td><td>CCF</td><td><input type=text size=3 id="ccf" onchange="{setVar('ccf',(+this.value*1000).toFixed())}"></td></tr>
<tr><td>Lookahead</td><td><input type=text size=4 id="fcr" onchange="{setVar('fcrange',this.value)}"></td><td>Disp</td><td><input type=text size=3 id="fcd" onchange="{setVar('fcdisp',this.value)}"></td></tr>
<tr><td>Fan Auto Run</td><td><input type=text size=4 id="fim" onchange="{setVar('fim',t2s(this.value))}"></td><td>Run</td><td><input type=text size=3 id="far" onchange="{setVar('far',t2s(this.value))}"></td></tr>
<tr id="int" style="visibility:collapse"><td>Internal</td><td id="it0"></td><td id="loc" colspan=2></td><td></td></tr>
<tr id="snd0" style="visibility:collapse"><td id="s0"></td><td><input type="button" value="Pri" id="sndpri0" onClick="{setSnd(0,0)}"><input type="button" value="En" id="snda0" onClick="{setSnd(0,1)}"></td><td id="rt0" colspan=2></td><td></td></tr>
<tr id="snd1" style="visibility:collapse"><td id="s1"></td><td><input type="button" value="Pri" id="sndpri1" onClick="{setSnd(1,0)}"><input type="button" value="En" id="snda1" onClick="{setSnd(1,1)}"></td><td id="rt1" colspan=2></td><td></td></tr>
<tr id="snd2" style="visibility:collapse"><td id="s2"></td><td><input type="button" value="Pri" id="sndpri2" onClick="{setSnd(2,0)}"><input type="button" value="En" id="snda2" onClick="{setSnd(2,1)}"></td><td id="rt2" colspan=2></td><td></td></tr>
<tr id="snd3" style="visibility:collapse"><td id="s3"></td><td><input type="button" value="Pri" id="sndpri3" onClick="{setSnd(3,0)}"><input type="button" value="En" id="snda3" onClick="{setSnd(3,1)}"></td><td id="rt3" colspan=2></td><td></td></tr>
<tr id="snd4" style="visibility:collapse"><td id="s4"></td><td><input type="button" value="Pri" id="sndpri4" onClick="{setSnd(4,0)}"><input type="button" value="En" id="snda4" onClick="{setSnd(4,1)}"></td><td id="rt4" colspan=2></td><td></td></tr>
</table>
<p>
<table style="width: 290px">
<tr><td>Password</td><td><input id="myToken" name="access_token" type=text size=40 placeholder="e6bba7456a7c9" style="width: 98px"
 onChange="{
 localStorage.setItem('myStoredText1', a.myToken.value)
 alert(a.myToken.value+' Has been stored')
}">
</td>
</tr>
</table></p>
<small>&copy 2016 CuriousTech.net</small>
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
body{background:silver;width:700px;display:block;text-align:center;font-family: Arial, Helvetica, sans-serif;}}
</style>
<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.6.1/jquery.min.js" type="text/javascript" charset="utf-8"></script>
<script type="text/javascript" src="forecast"></script>
<script type="text/javascript">
var graph;
xPadding=30
yPadding=56
drawMode=3
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
  step = Math.floor(arr.length / 15)
  if(step == 0) step = 1
  for(var i=0;i<arr.length-1;i+=step){
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

  c.fillText('Temp', graph.width()-6,6)
  c.fillStyle = +sJson.r?(md==2?"red":"blue"):(+sJson.fr?"green":"slategray")
  c.fillText((+sJson.it/10).toFixed(1), graph.width()-6, getYPixel(+sJson.it) )
 // cycle
  c.fillText(cyc,graph.width()-xPadding-7,h-8)

  c.fillStyle="green"
  c.fillText('Rh', xPadding-6, 6)

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
    c.lineTo(getXPixel(i),h)
    c.stroke()
   }
   }
  }

  if(arr[0].length>6)
  {
  if(drawMode&4) doLines('rgba(0,0,255,0.5)',6)
  if(drawMode&8) doLines('rgba(200,180,0,0.7)',7)
  if(drawMode&16) doLines('rgba(100,100,50,0.6)',8)
  }
  c.textAlign="left"
  y=graph.height()
  c.fillStyle='#000'
  c.fillText('Temp',2,y-=10)
  c.fillStyle='#0f0'
  c.fillText('Rh',2,y-=10)
  c.fillStyle='rgba(0,0,255)'
  c.fillText('Internal',2,y-=10)
  c.fillStyle='rgba(200,180,0)'
  y-=10
  if(snd[0]) c.fillText(snd[0][4],2,y)
  c.fillStyle='rgba(100,100,50)'
  y-=10
  if(snd[1]) c.fillText(snd[1][4],2,y)
  c.fillStyle='#fa0'
  c.fillText('Out',2,y-=10)

  // out temp
  if(drawMode&32) doLines('#fa0',5,'Out')
  if(drawMode&2){
  c.strokeStyle = '#0f0'
  c.beginPath()
  c.moveTo(getXPixel(0), getRHPixel(arr[0][2]))
  for(var i=1;i<arr.length;i ++)
  c.lineTo(getXPixel(i), getRHPixel(arr[i][2]))
  c.stroke()
  }
  var dots=[]
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
    var hit = false
    for(i=0;i<dots.length;i++){
      dot = dots[i]
      dx = mouseX - dot.x
      dy = mouseY - dot.y
      if(dx*dx + dy*dy < dot.rXr) {
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
  function getMousePos(cDom, mEv){
    rect = cDom.getBoundingClientRect();
    return{
     x: mEv.clientX-rect.left,
     y: mEv.clientY-rect.top
    }
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
  var max = 0
  
  for(i=0; i<arr.length-1; i++)
  {
    if(arr[i][1] > max)
      max=arr[i][1]
    if(arr[i][3]+th>max)
      max=arr[i][3]+th
    if(drawMode&32&&arr[i][5]>max)
      max=arr[i][5]
    for(j=6;j<arr[i].length;j++)
      if(arr[i][j]>max)
      max=arr[i][j]
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
    if(drawMode&32&&arr[i][5]<min)
      min=arr[i][5]
    for(j=6;j<arr[i].length;j++)
      if(arr[i][j]<min)
      min=arr[i][j]
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
//  c.clearRect(0, 0, graph2.width(), graph2.height())
  canvasOffset=graph2.offset()
  offsetX=canvasOffset.left
  offsetY=canvasOffset.top
  yPad=18
  if(fcr>fc.length) fcr=fc.length
  c.lineWidth=2
  c.font='italic 8pt sans-serif'
  c.textAlign="left"

  doBorder(graph2)

  c.lineWidth = 1
  min=150
  max=-30
  for(i=0;i<fc.length;i++)
  {
    if(min>fc[i][1]) min=fc[i][1]
    if(max<fc[i][1]) max=fc[i][1]
  }
  max++
  yRange=max-min
  c.textAlign = "right"
  c.textBaseline = "middle"
  c.fillStyle='black'

  // right legend
  for(i = min; i<max; i+=(yRange/8) )
    c.fillText(i.toFixed(1), graph2.width()-6, getYPixel2(i))
  c.fillText('Out', graph2.width()-6, 6)

  c.textAlign = "left"
  iRng=iMax-iMin
  c.fillText(iMax/10, 6, getYPixel3(iMax))
  c.fillText(iMin/10, 6, getYPixel3(iMin))

  c.fillStyle='#40404050'
  w=graph2.width()-xPadding*2
  c.fillRect(xPadding,getYPixel3(iMax),w,getYPixel3(iMin)-getYPixel3(iMax))

  // temp lines
  c.fillStyle="red"
  date = new Date(fc[0][0]*1000)
  dt=date.getDate()
  fl=(fc[fc.length-1][0]-fc[0][0])/60
  cPos=0
  for(i=1; i<fc.length; i++){
  c.strokeStyle=(fc[i][1]<32)?"blue":"red"
  c.beginPath()
  c.moveTo(getXPixel2(i), getYPixel2(fc[i][1]))
  c.lineTo(getXPixel2(i-1), getYPixel2(fc[i-1][1]))
  c.stroke()
  date = new Date(fc[i][0]*1000)
  if(cPos==0&&date.valueOf()>=(new Date()).valueOf())
  {
    dif=(date.valueOf()-(new Date().valueOf()))/60000
    xOff=w/fl*dif
    cPos=i;
    c.strokeStyle='#fff'
    c.beginPath()
    c.moveTo(getXPixel2(cPos)-xOff,getYPixel3(iMax))
    c.lineTo(getXPixel2(cPos)-xOff,getYPixel3(iMin))
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
  c.moveTo(getXPixel2(strt)-xOff, getTT(strt,0))
  for(i=strt+1; i<=cPos+fcr; i++)
    c.lineTo(getXPixel2(i)-xOff, getTT(i,0))
  for(i=cPos+fcr; i>=strt; i--)
    c.lineTo(getXPixel2(i)-xOff, getTT(i,(md==2)?ct:-ct))
  c.closePath()
  c.fill()
}
function getXPixel2(val){
  x=xPadding+((graph2.width()-xPadding*2)/fc.length)*val
  return +x.toFixed()
}

function getYPixel2(val) {
  y=graph2.height()-( ((graph2.height()-18)/yRange)*(val-min))-18
  return y.toFixed()
}
function getYPixel3(val) {
  y=graph2.height()/2-( (graph2.height()/2/iRng)*(val-iMin))
  return y+30
}
function getTT(i,th)
{
  min2=150
  max2=-30
  strt1=i-fcr
  if(strt1<0) strt1=0
  for(j=strt1;j<i+fcr;j++)
  {
    if(j<fc.length){
      if(min2>fc[j][1]) min2=fc[j][1]
      if(max2<fc[j][1]) max2=fc[j][1]
    }
  }
  tt=(fc[i][1]-min2)*iRng/(max2-min2)+iMin+th/10
  h=graph2.height()/2
  return h-(h/iRng*(tt-iMin))+30
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
<td>Shift:<input type=text size=1 id="fco" onchange="{fco=+this.value;drawFC()}"> Range:<input type=text size=1 id="fcr" onchange="{fcr=+this.value;drawFC()}">
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

const char page_styles[] PROGMEM = R"rawliteral(
table{
border-radius: 5px;
margin-bottom: 5px;
box-shadow: 2px 2px 12px #000000;
background-image: -moz-linear-gradient(top, #efffff, #a0a0a0);
background-image: -ms-linear-gradient(top, #efffff, #a0a0a0);
background-image: -o-linear-gradient(top, #efffff, #a0a0a0);
background-image: -webkit-linear-gradient(top, #efffff, #a0a0a0);
background-image: linear-gradient(top, #efffff, #a0a0a0);
background-clip: padding-box;
}
input{
border-radius: 5px;
margin-bottom: 5px;
box-shadow: 2px 2px 12px #000000;
background-image: -moz-linear-gradient(top, #efffff, #a0a0a0);
background-image: -ms-linear-gradient(top, #efffff, #a0a0a0);
background-image: -o-linear-gradient(top, #efffff, #a0a0a0);
background-image: -webkit-linear-gradient(top, #efffff, #a0a0a0);
background-image: linear-gradient(top, #efffff, #a0a0a0);
background-clip: padding-box;
}
.style1{border-width: 0;}
.style2{text-align: left;}
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
)rawliteral";

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
