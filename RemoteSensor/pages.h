const char page_index[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>WiFi Environmental Monitor</title>
<style>
div,table{border-radius: 3px;box-shadow: 2px 2px 12px #000000;
background-image: -moz-linear-gradient(top, #ffffff, #405050);
background-image: -ms-linear-gradient(top, #ffffff, #405050);
background-image: -o-linear-gradient(top, #ffffff, #405050);
background-image: -webkit-linear-gradient(top, #ffffff, #405050);
background-image: linear-gradient(top, #ffffff, #405050);
background-clip: padding-box;}
input{border-radius: 5px;box-shadow: 2px 2px 12px #000000;
background-image: -moz-linear-gradient(top, #ffffff, #207080);
background-image: -ms-linear-gradient(top, #ffffff, #207080);
background-image: -o-linear-gradient(top, #ffffff, #207080);
background-image: -webkit-linear-gradient(top, #a0c0ff, #207080);
background-image: linear-gradient(top, #ffffff, #207080);
background-clip: padding-box;}
body{width:490px;display:block;text-align:right;font-family: Arial, Helvetica, sans-serif;}
</style>
<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.6.1/jquery.min.js" type="text/javascript" charset="utf-8"></script>
<script type="text/javascript">
a=document.all
xPadding=30
yPadding=56
added=false
cf=0
showidx=0
nms=['OFF','ON ']
spri=['OFF','PRI','EN ']
$(document).ready(function()
{
  key=localStorage.getItem('key')
  if(key!=null) document.getElementById('myKey').value=key
  openSocket()
})

function openSocket(){
ws=new WebSocket("ws://"+window.location.host+"/ws")
//ws=new WebSocket("ws://192.168.31.194/ws")
ws.onopen=function(evt){setVar('hist',0)}
ws.onclose=function(evt){alert("Connection closed");}
ws.onmessage=function(evt){
console.log(evt.data)
 lines=evt.data.split(';')
 event=lines[0]
 data=lines[1]
 d=JSON.parse(data)
 switch(event)
 {
  case 'settings':
    oledon=d.o
    a.OLED.value=oledon?'ON ':'OFF'
    a.ID.value=hex_to_ascii(d.ID)
    a.PIR.value=d.pir?'ON ':'OFF'
    a.prisec.value=s2t(d.prisec)
    a.PRI.value=spri[d.pri]
    a.nm.value=d.name
    a.SRATE.value=s2t(d.srate)
    a.LRATE.value=s2t(d.lrate)
    a.TZ.value=d.tz
    a.LED1.value=nms[d.l1]
    a.LED1.setAttribute('style',d.l1?'color:blue':'')
    a.LED2.value=nms[d.l2]
    a.LED2.setAttribute('style',d.l2?'color:blue':'')
    cf=d.cf
    a.CF.value=cf?'F':'C'
    a.CH.value=nms[d.ch]
    a.CH.setAttribute('style',d.ch?'color:red':'')
    bSi=+d.si
  a.SIL.setAttribute('style',bSi?'color:red':'')
    break
  case 'state':
    dt=new Date(d.t*1000)
    DF=d.df
    a.time.innerHTML=dt.toLocaleTimeString()+' '+d.temp+'&deg'+ (cf?'F':'C') + ' '+d.rh+'%'
    a.rssi.innerHTML=d.rssi+'dB  &nbsp; '
    if(d.df & 4) a.co2.innerHTML = label[2]+': '+d.co2+'ppm'
    s=''
    if(d.df & 8) s += ' '+label[3]+': '+d.ch2o+'mg/m3'
    if(d.df & 16) s += ' &nbsp;'+label[4]+': '+d.voc+'ppm '
    a.extra.innerHTML=s+' &nbsp; '
    break
  case 'alert':
    alert(d.text)
    break
  case 'ref':
    tb=d.tb
    base=d.base
    decs=d.dec
    label=d.label
    arr=new Array()
    arrW=new Array()
    arrD=new Array()
  fl(0)
  s='Alert &nbsp; '
  for(i=0;i<label.length;i++)
    if(DF&(1<<i)) s+=' &nbsp; &nbsp;&nbsp; '+label[i]+' &nbsp; '
    else s+=' &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; '
  a.labels.innerHTML=s
    for(i=0;i<10;i++)
    {
     if(DF&(1<<(i>>1))){
     dec=1
     for(j=0;j<decs[i>>1];j++) dec*=10
     document.getElementById('al'+i).value=(d.alert[i]/dec)
     }
    }
    break
  case 'data':
    for(i=0;i<d.d.length;i++){     // time, temp, rh, thrsh, state outtemp
      n=d.d[i][0]; d.d[i][0]=tb*1000; tb-=n
      for(j=0;j<base.length;j++){
        n=d.d[i][j+1]; d.d[i][j+1]=base[j]+n; base[j]+=n
      }
    }
    arr=arr.concat(d.d)
    draw()
    draw2()
    break
  case 'data2':
    d.d[0][0]*=1000
    arr=d.d.concat(arr)
    draw()
    draw2()
    break
  case 'weekly':
    arrW=arrW.concat(d.d)
    draw4()
    break
  case 'daily':
    arrD=arrD.concat(d.d)
    draw3()
    break
 }
}
}

function setVar(varName, value)
{
  ws.send('cmd;{"key":"'+a.myKey.value+'","'+varName+'":'+value+'}')
}

function led1()
{
  if(a.LED1.value=='OFF') l=1
  else l=0
  setVar('led1',l)
  a.LED1.value=nms[l]
  a.LED1.setAttribute('style',l?'color:blue':'')
}
function led2()
{
  if(a.LED2.value=='OFF') l=1
  else l=0
  setVar('led2',l)
  a.LED2.value=nms[l]
  a.LED2.setAttribute('style',l?'color:blue':'')
}
function changeNm()
{
  setVar('name',a.nm.value)
}
function setPriSec()
{
  setVar('prisec', t2s(a.prisec.value))
}
function setSRate()
{
  setVar('srate', t2s(a.SRATE.value))
}
function setLRate()
{
  setVar('lrate', t2s(a.LRATE.value))
}
function oled(){
  oledon=!oledon
  setVar('oled', oledon)
  a.OLED.value=oledon?'ON ':'OFF'
}
function pir(){
  p=(a.PIR.value=='OFF')?true:false
  setVar('pir', p)
  a.PIR.value=p?'ON ':'OFF'
}
function pri(){
  p=0
  if(a.PRI.value=='OFF') p=1
  else if(a.PRI.value=='PRI') p=2
  setVar('pri', p)
  a.PRI.value=spri[p]
}

function fl(n)
{
 showidx=n
 for(i=0;i<label.length;i++)
 {
  flg=document.getElementById('FLG'+i)
  if(DF&(1<<i)){
  flg.setAttribute('style',(showidx==i)?'color:red':'')
  flg.value=label[i]
  }
  else flg.setAttribute('style','visibility:hidden')
 }
 draw3()
 draw4()
}

function t2hms(t)
{
  s=t%60
  t=Math.floor(t/60)
  if(t==0) return s
  if(s<10) s='0'+s
  m=t%60
  t=Math.floor(t/60)
  if(t==0) return m+':'+s
  if(m<10) m='0'+m
  h=t%24
  t=Math.floor(t/24)
  if(t==0) return h+':'+m+':'+s
  return t+'d '+h+':'+m+':'+s
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

function togCall()
{
  on=(a.CH.value=='OFF')?1:0
  a.CH.value=nms[on]
  a.CH.setAttribute('style',on?'color:red':'')
  if(on) setVar('hostip',80)
  else setVar('ch',0)
}

function setcf()
{
  cf=(a.CF.value=='C')?1:0
  a.CF.value=cf?'F':'C'
  setVar('cf',cf)
}

function setID()
{
  setVar('ID',ascii_to_hex(a.ID.value))
}

function setAL(n)
{
  setVar('alertidx',n)
  mul=1
  for(j=0;j<decs[n>>1];j++) mul*=10
  setVar('alertlevel',document.getElementById('al'+n).value*mul)
}

function setSilence()
{
  bSi^=1
  setVar('silence',bSi)
  a.SIL.setAttribute('style',bSi?'color:red':'')
}

function hex_to_ascii(val)
{
  hex=val.toString(16)
  str=''
  for(n=hex.length-2;n>=0;n-=2)
    str+=String.fromCharCode(parseInt(hex.substr(n,2),16))
  return str
}

function ascii_to_hex(str)
{
  v=0
  for(n=str.length-1;n>=0;n--)
  {
    v<<=8
    v+=str.charCodeAt(n)
  }
  return v
}

function draw(){
 graph = $('#chart')
 c=graph[0].getContext('2d')

 tipCanvas=document.getElementById("tip")
 tipCtx=tipCanvas.getContext("2d")
 tipDiv=document.getElementById("popup")

 c.fillStyle=c.strokeStyle='black'
 o=graph.offset()
 offsetX=o.left
 offsetY=o.top
 yPad=3
 c.lineWidth=2
 c.font='italic 8pt sans-serif'
 c.textAlign="left"
 c.clearRect(0, 0, graph.width(), graph.height())
 doBorder(graph)

 c.lineWidth = 1
 if(typeof(arr)=="undefined") return

 // value range
 c.textAlign="right"
 c.textBaseline="middle"

 // temp scale
 yRange=getMaxY(1)-getMinY(1)
 for(var i=getMinY(1);i<getMaxY(1);i+=(yRange/8))
  c.fillText((i/10).toFixed(1),graph.width()-6,getYPixel(i,getMinY(1)))

 // Temp
 c.strokeStyle=c.fillStyle='#f00'
 c.fillText('Temp', graph.width()-6,6)
 drawArray(1,getMinY(1))

 // rh scale
 c.fillStyle=c.strokeStyle='black'
 min=350
 if(getMinY(2)<min) min=getMinY(2)
 max=450
 if(getMaxY(2)>max) max=getMaxY(2)
 yRange=max-min
 for(var i=min;i<max;i+=(yRange/8))
  c.fillText((i/10).toFixed(),xPadding-4,getYPixel(i,min))

 // RH
 c.strokeStyle=c.fillStyle='#0f0'
 c.fillText('Rh', xPadding-6, 6)
 drawArray(2,min)

 // request mousemove events
 graph.mousemove(function(e){handleMouseMove(e);})

 // show tooltip when mouse hovers over anything
 function handleMouseMove(e){
  var c=document.getElementById('chart')
  rect=c.getBoundingClientRect()
  dx=e.clientX-rect.x-xPadding
  dy=e.clientY-rect.y

  if((DF&0x1C)==0) popup.style.height='58px'
  tipCtx.clearRect(0,0,tipCanvas.width,tipCanvas.height)
  tipCtx.lineWidth=2
  tipCtx.fillStyle="#000000"
  tipCtx.strokeStyle='#333'
  tipCtx.font='italic 8pt sans-serif'
  tipCtx.textAlign="left"

  idx=arr.length-(dx*arr.length/(rect.width-xPadding*2)).toFixed()
  if(idx<0||idx>=arr.length)
    popup.style.left="-200px"
  else
  {
    tipCtx.fillText((new Date(arr[idx][0])).toLocaleTimeString()+' ',4,15)
    tipCtx.fillText(label[0]+' '+(arr[idx][1]/10)+'\xB0'+(cf?'F':'C'),4,29)
    tipCtx.fillText(label[1]+'   '+(arr[idx][2]/10)+'%',4,43)
    if(DF&4) tipCtx.fillText(label[2]+'  '+arr[idx][3]+' mg',4,57)
    if(DF&8) tipCtx.fillText(label[3]+' '+arr[idx][4]+' ppm',4,71)
    if(DF&16) tipCtx.fillText(label[4]+'   '+arr[idx][5]+' ppm',4,85)
    popup=document.getElementById("popup")
    popup.style.top=(dy+rect.y+window.pageYOffset-90)+"px"
    popup.style.left=(dx+rect.x-40)+"px"
  }
 }
}

function draw2(){
  graph = $('#chart2')
  c=graph[0].getContext('2d')

  if(typeof(arr)=="undefined") return

  if(DF&0x1C && graph.height()!=200)
   graph[0].height=200

  c.fillStyle='black'
  c.strokeStyle='black'
  yPad=yPadding
  c.lineWidth=2
  c.font='italic 8pt sans-serif'
  c.textAlign="left"
  c.clearRect(0, 0, graph.width(), graph.height())

  if(DF&0x1C) doBorder(graph)

  c.lineWidth=1
  // dates
  step=Math.floor(arr.length/10)
  if(step == 0) step = 1
  h=graph.height()-yPad
  for(var i=0;i<arr.length-1;i+=step){
   c.save()
   c.translate(getXPixel(i),h+5)
   c.rotate(0.9)
   date = new Date(arr[i][0])
   c.fillText(date.toLocaleTimeString(),0,0)
   c.restore()
  }
  
  c.textAlign="right"
  c.textBaseline="middle"

  if((DF&0x1C)==0)
    return

  // CO2 scale
  min=getMinY(3)-20
  if(min<=0) min=0
  yRange=getMaxY(3)-min
  for(i=min;i<getMaxY(3);i+=(yRange/8))
  c.fillText((i.toFixed()),graph.width()-6,getYPixel(i,min))

  // CO2
  c.strokeStyle=c.fillStyle='#f0f'
  c.fillText('CO2', graph.width()-6,6)
  drawArray(3,min)

  // VOC/CH2O scale
  min=0
  c.fillStyle='black'
  yRange=getMaxY(4)-0
  yRange2=getMaxY(5)-0
  if(yRange2>yRange) yRange=yRange2
  yRange+=2
  for(i=0;i<yRange;i+=(yRange/4))
    c.fillText(i.toFixed(),xPadding-4,getYPixel(i,0) )

  // CH2O
  c.strokeStyle=c.fillStyle='#00f'
  c.fillText(label[3], xPadding-5, 17)
  drawArray(4,0)

  // VOC
  c.strokeStyle=c.fillStyle='#ff0'
  c.fillText(label[4], xPadding-6, 6)
  drawArray(5,0)
}

function drawArray(n,min)
{
  c.beginPath()
  c.moveTo(getXPixel(0),getYPixel(arr[0][n],min))
  for(var i=1;i<arr.length;i++)
   c.lineTo(getXPixel(i),getYPixel(arr[i][n],min))
  c.stroke()
}

function doBorder(g)
{
  c.beginPath()
  c.moveTo(xPadding,0)
  c.lineTo(xPadding,g.height()-yPad)
  c.lineTo(g.width()-xPadding, g.height()-yPad)
  c.lineTo(g.width()-xPadding, 0)
  c.stroke()
}

function getMaxY(v){
  var max = 0
  
  for(i=0; i<arr.length-1; i++)
  {
    if(arr[i][v] > max)
      max=arr[i][v]
  }
  return Math.ceil(max)
}

function getMinY(v){
  var min = 5000

  for(i=0; i<arr.length; i++)
  {
    if(arr[i][v]<min)
      min=arr[i][v]
  }
  return Math.floor(min)
}

function getXPixel(val){
  x=(graph.width()-xPadding)-((graph.width()-26-xPadding)/arr.length)*val
  return x.toFixed()
}

function getYPixel(val,min) {
  y=graph.height()-( ((graph.height()-yPad)/yRange)*(val-min))-yPad
  return y.toFixed()
}

function draw3(){ // daily for 1 week
  graph = $('#chart3')
  c=graph[0].getContext('2d')

  if(graph.height()!=200)
   graph[0].height=200

  c.fillStyle='black'
  c.strokeStyle='black'
  yPad=20
  c.lineWidth=2
  c.font='italic 8pt sans-serif'
  c.textAlign="left"
  c.clearRect(0, 0, graph.width(), graph.height())

  dys=['Sun','Mon','Tue','Wed','Thu','Fri','Sat']

  c.lineWidth=1
  // dates
  y=graph.height()-yPad+10
  for(var i=0;i<arrD.length;i++){
   x=(graph.width()-yPad)/arrD.length*i+10
   c.fillText(dys[i],x,y)
  }
  dots2=[]
  c.textAlign="right"
  c.textBaseline="middle"
  date=new Date()
  c.lineWidth=20
  draw_scale(arrD,graph.width()-20,graph.height()-yPad,2,date.getDay())
}

function draw4(){ // 52 weeks
  graph = $('#chart4')
  c=graph[0].getContext('2d')

  if(graph.height()!=200)
   graph[0].height=200

  c.fillStyle='black'
  c.strokeStyle='black'
  yPad=20
  c.textAlign="left"
  c.clearRect(0,0,graph.width(), graph.height())

  mon=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec']

  c.lineWidth=1
  // dates
  y=graph.height()-yPad+10
  for(var i=0;i<12;i++){
   x=(graph.width()-yPad-10)/12*i+10
   c.fillText(mon[i],x,y)
  }
  c.textAlign="right"
  c.textBaseline="middle"
  date=new Date()
  c.lineWidth=4
  
  cd=new Date()
  oneJan=new Date(cd.getFullYear(),0,1)
  days=Math.floor((cd-oneJan)/(24*60*60*1000))
  wks=Math.ceil((cd.getDay()+1+days)/7)-1
  draw_scale(arrW,graph.width()-40,graph.height()-yPad,1,wks)
}

function draw_scale(ar,w,h,o,ct)
{
  min=20000
  max=0
  idx=showidx*2
  for(i=0;i<ar.length;i++)
  {
    if(ar[i][idx]==0&&ar[i][idx+1]==0) continue
    if(ar[i][idx]<min) min=ar[i][idx]
    if(ar[i][idx+1]>max) max=ar[i][idx+1]
  }

  yRange=max-min
  div=1
  for(i=0;i<decs[showidx];i++) div*=10
  for(i=min;i<=max;i+=(yRange/8))
  {
    n=i/div
    c.fillText(n.toFixed(decs[showidx]),graph.width()-6,chartY(i,yRange))
  }
  c.textAlign="center"
  lw=c.lineWidth
  clr=['#F00','#0F0','#F0F','#FF0','#00F']
  for(i=0;i<ar.length;i++)
  {
    x=((i*(w/ar.length))+10+(lw/2))
    c.strokeStyle=clr[showidx]
    bt=ar[i][idx+1]*(h-10)/max
    if(min==0) bb=0
    else bb=ar[i][idx]*(h-10)/min
    c.beginPath()
    c.moveTo(x,chartY(ar[i][idx+1],max-min))
    c.lineTo(x,chartY(ar[i][idx],max-min))
    c.stroke()
    if(i==ct)
    {
      c.strokeStyle="#000"
      c.lineWidth=1
      c.beginPath()
    dw=((w/ar.length)/2)+(lw/2)-10
      c.moveTo(x+dw,o+h)
      c.lineTo(x+dw,o)
      c.stroke()
      c.lineWidth=lw
    }
  }
}

function chartY(n,rng)
{
  h=graph.height()-yPad-4
  return h-((h/rng)*(n-min))+4
}

</script>
<style type="text/css">
#popup {
  position: absolute;
  top: 150px;
  left: -150px;
  z-index: 10;
  border-style: solid;
  border-width: 1px;
}
</style>
</head>
<body bgcolor="silver">
<table align="right" width=480>
<tr><td>LED1 &nbsp; LED2 &nbsp; </td><td><input id="nm" type=text size=8 onchange="changeNm();"></td><td><div id="time"></div></td></tr>
<tr>
<td><input name="LED1" value="OFF" type='button' onclick="{led1()}">&nbsp; <input name="LED2" value="OFF" type='button' onclick="{led2()}"> &nbsp; </td>
<td>Report <input name="CH" value="OFF" type='button' onclick="{togCall()}"> </td><td>
 OLED <input type="button" value="ON" id="OLED" onClick="{oled()}">
 &nbsp; TZ<input name="TZ" type=text size=1 value='0' onchange="setVar('TZ', this.value);">
 &nbsp; <input name="CF" value="C" type='button' onclick="{setcf()}"></td></tr>
<tr>
<td>Upd Rate<input id='SRATE' type=text size=4 value='10' onchange="{setSRate()}"></td>
<td>Motion<input type="button" value="ON" id="PIR" onClick="{pir()}"></td>
<td> Mode:<input type="button" value="OFF" id="PRI" onClick="{pri()}">
 Timer: <input id='prisec' type=text size=4 value='60' onchange="{setPriSec()}"></td></tr>
<tr>
<td>Log Rate<input id='LRATE' type=text size=4 value='10' onchange="{setLRate()}"></td>
<td><div id="rssi">0db</div></td><td>
 <input value='Restart' type=button onclick="setVar('reset',0);"> &nbsp; 
 <input id="myKey" name="key" type=text size=50 placeholder="password" style="width: 128px" onChange="{localStorage.setItem('key', key = document.all.myKey.value)}">
</td></tr>

<tr><td colspan=3 id="labels"></tr>
<tr><td colspan=3><input name="SIL" value="Silence" type='button' onclick="{setSilence()}">&nbsp; &nbsp; High &nbsp; <input id='al1' type=text size=4 onchange="{setAL(1)}"><input id='al3' type=text size=4 onchange="{setAL(3)}"> <input id='al5' type=text size=4 onchange="{setAL(5)}"> <input id='al7' type=text size=4 onchange="{setAL(7)}"> <input id='al9' type=text size=4 onchange="{setAL(9)}"></td></tr>
<tr><td colspan=3>Low &nbsp; <input id='al0' type=text size=4 onchange="{setAL(0)}"><input id='al2' type=text size=4 onchange="{setAL(2)}"> <input id='al4' type=text size=4 onchange="{setAL(4)}"> <input id='al6' type=text size=4 onchange="{setAL(6)}"> <input id='al8' type=text size=4 onchange="{setAL(8)}"></td></tr>

<tr><td>ID: <input id='ID' type=text size=6 value='0' maxlength=4 onchange="{setID()}"></td><td><div id="co2"></div></td><td><div id="extra"></div></td>
</tr>
</table>
<table align="right" width=480>
<tr><td>
<div id="wrapper">
<canvas id="chart" width="474" height="300"></canvas>
<canvas id="chart2" width="474" height="56"></canvas>
<canvas id="chart3" width="474" height="56"></canvas>
<canvas id="chart4" width="474" height="56"></canvas>
<div id="popup"><canvas id="tip" width=70 height=94></canvas></div>
</div>
</td></tr>
<tr><td colspan=3>
<input id="FLG0" type='button' onclick="{fl(0)}">
<input id="FLG1" type='button' onclick="{fl(1)}">
<input id="FLG2" type='button' onclick="{fl(2)}">
<input id="FLG3" type='button' onclick="{fl(3)}">
<input id="FLG4" type='button' onclick="{fl(4)}">
 </td></tr>
</table></body>
</html>
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
