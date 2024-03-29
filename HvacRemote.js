//  HvacRemote script running on PngMagic  http://www.curioustech.net/pngmagic.html
// Device is set to a fixed IP in the router

	hvacUrl = 'ws://192.168.31.46/ws'
	password = 'password'

	kwh = 3600 // killowatt hours (compressor+fan)
	ppkwh = 0.153  // electric price per KWH  (price / KWH)
	ccfs	= 0.70 / (60*60) // NatGas cost per hour divided into seconds

	modes = new Array('Off', 'Cool', 'Heat', 'Auto','Cycle')

	fontSize  = 20

	if(Reg.overrideTemp == 0)
		Reg.overrideTemp = -1.2

	var coolTempH
	var cycleThresh
	var hvacJson
	var mode
	var last
	var last1, last2
	var cycleState = 0
	var lastCycleTimer = 0
	var cycleTotal = 0

	Pm.Window('HvacRemote')

	Gdi.Width = 300 // resize drawing area
	Gdi.Height = 340

	Http.Close()
	if(!Http.Connected)
		Http.Connect('HVAC', hvacUrl)

	Pm.SetTimer(1000)

// Handle published events
function OnCall(msg, event, data, d2)
{
	switch(msg)
	{
		case 'HTTPCONNECTED':
			Pm.Echo('HVAC Connected')
			break
		case 'HTTPSTATUS':
			switch(+event)
			{
				case 400: s = 'Bad request'; break
				case 408: s = 'Request timeout'; break
				case 12002: s = 'Timeout'; break
				case 12152: s =  'INVALID_SERVER_RESPONSE'; break
				default: s = ' '
			}
			Pm.Echo( 'HvacRemote error: ' + event + ' ' + s)
			break
		case 'HTTPDATA':
			timeout = new Date()
			if(data.length) procLine(data)
			break
		case 'HTTPCLOSE':
			switch(+data)
			{
				case 400: data += ' Bad request'; break
				case 408: data += ' Request timeout'; break
				case 12002: data += ' Timeout'; break
				case 12017: data += ' ERROR_INTERNET_OPERATION_CANCELLED'; break
			}
			Pm.Echo( 'HVAC WS closed ' + data)
			break

		case 'BUTTON':
			switch(event)
			{
				case 0:		// Override
					ovrActive = !ovrActive
					SetVar('override', ovrActive ? (Reg.overrideTemp * 10) : 0)
					break
				case 1:		// reset filter
					SetVar('resetfilter', 0)
					filterMins = 0
					break
				case 2:		// fan
					fanMode = (fanMode+1) % 2; SetVar('fanmode', fanMode)
					break
				case 3:		// Unused
					SetVar('fw', 300)
					break
				case 4:		// mode
					mode = (mode + 1) % 5; SetVar('mode', mode)
					break
				case 5:		// heat mode
					heatMode = (heatMode+1) % 3; SetVar('heatMode', heatMode)
					break
				case 6:		// cool H up
					setTemp(1, coolTempH + 0.1, 1); SetVar('cooltemph', (coolTempH * 10).toFixed())
					break
				case 7:		// cool H dn
					setTemp(1, coolTempH - 0.1, 1); SetVar('cooltemph', (coolTempH * 10).toFixed())
					break
				case 8:		// cool L up
					setTemp(1, coolTempL + 0.1, 0); SetVar('cooltempl', (coolTempL * 10).toFixed())
					break
				case 9:		// cool L dn
					setTemp(1, coolTempL - 0.1, 0); SetVar('cooltempl', (coolTempL * 10).toFixed())
					break
				case 10:		// heat H up
					setTemp(2, heatTempH + 0.2, 1); SetVar('heattemph', (heatTempH * 10).toFixed())
					break
				case 11:		// heat H dn
					setTemp(2, heatTempH - 0.2, 1); SetVar('heattemph', (heatTempH * 10).toFixed())
					break
				case 12:		// heat L up
					setTemp(2, heatTempL + 0.2, 0); SetVar('heattempl', (heatTempL * 10).toFixed())
					break
				case 13:		// heat L dn
					setTemp(2, heatTempL - 0.2, 0); SetVar('heattempl', (heatTempL * 10).toFixed())
					break
				case 14:		// thresh up
					if(cycleThresh < 6.3){ cycleThresh += 0.1; SetVar('cyclethresh', (cycleThresh * 10).toFixed()); }
					break
				case 15:		// thresh dn
					if(cycleThresh > 0.1){ cycleThresh -= 0.1; SetVar('cyclethresh', (cycleThresh * 10).toFixed()); }
					break
				case 16:		// override time up
					overrideTime += 60
					SetVar('overridetime', overrideTime)
					break
				case 17:		// override time dn
					overrideTime -= 10
					SetVar('overridetime', overrideTime)
					break	
				case 18:		// override temp up
					Reg.overrideTemp = +Reg.overrideTemp + 0.1
					break
				case 19:		// override temp dn
					Reg.overrideTemp = +Reg.overrideTemp - 0.1
					break
			}
			Draw()
			break

		default:
			Pm.Echo('HVAC Unrecognised ' + msg)
			break
	}
}

function OnTimer()
{
	if(Http.Connected)
		return
	Http.Connect('HVAC', hvacUrl)
}

function SetVar(v, val)
{
	Http.Send( '{key:' + password + ',' + v + ':' + val + '}'  )
}

function procLine(data)
{
	if(data.length < 2) return
//Pm.Echo(data)
	json = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
		data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')

	switch(json.cmd)
	{
		case 'settings':
			mode = +json.m
			autoMode = +json.am
			heatMode = +json.hm
			fanMode = +json.fm
			ovrActive = +json.ot
			eHeatThresh = +json.ht

			coolTempL = +json.c0 / 10
			coolTempH = +json.c1 / 10
			heatTempL = +json.h0 / 10
			heatTempH = +json.h1 / 10
			cycleThresh = +json.ct / 10
			Reg.cycleThresh = cycleThresh
			overrideTime = +json.ov
			remoteTimer = json.rm
			remoteTimeout = json.ro
			ppkwh = json.ppk / 10000
			kwh = json.cw + json.fw

			ccfs = json.ccf / 100000 // Nat gas = 1.243 CCF on bill / into CF
			cfm = json.cfm / 1000	// Cubic feet per minute into seconds

			Draw()
			break

		case 'state':
			hvacJson = json
			running = +json.r
			state = +json.s
			fan = +json.fr
			inTemp = +json.it / 10
			rh = (+json.rh / 10).toFixed(1)
			targetTemp = +json.tt / 10
			filterMins = +json.fm
			outTemp = +json.ot / 10
			outFL = +json.ol
			outFH = +json.oh
			cycleTimer = +json.ct
			fanTimer = +json.ft
			runTotal = +json.rt

//Pm.Echo('HV ' + inTemp + ' ' + rh)

			if(Pm.FindWindow( 'HvacHistory' ))
				Pm.History( 'REFRESH' )
			Draw()
			LogTemps(json.snd)
			Pm.Server('STATTEMP', inTemp + '° ' + rh + '% > ' + targetTemp + '° ')
			break
		case 'update':
			break
		case 'alert':
			date = new Date()
			Pm.Echo('HVAC Alert: ' + date.toLocaleTimeString() + ' ' + json.text)
			break
		case 'print':
			date = new Date()
			Pm.Echo( 'HVAC  '  + date.toLocaleTimeString() + ' '  + json.text)
			break
		default:
			Pm.Echo('HVAC Unknown event: ' + data)
			break	
	}
}

// mimic thermostat
function setTemp( mode, Temp, hl)
{
	if(mode == 3) // auto
	{
		mode = autoMode
	}

	switch(mode)
	{
		case 1:
			if(Temp < 65.0 || Temp > 92.0)    // ensure sane values
				break
			if(hl)
			{
				coolTempH = Temp
				coolTempL = Math.min(coolTempH, coolTempL)     // don't allow h/l to invert
			}
			else
			{
				coolTempL = Temp
				coolTempH = Math.max(coolTempL, coolTempH)
			}
			save = heatTempH - heatTempL
			heatTempH = Math.min(coolTempL - 2, heatTempH) // Keep 2.0 degree differencial for Auto mode
			heatTempL = heatTempH - save                      // shift heat low by original diff
			break
		case 2:
			if(Temp < 63.0 || Temp > 86.0)    // ensure sane values
				break
			if(hl)
			{
				heatTempH = Temp
				heatTempL = Math.min(heatTempH, heatTempL)
			}
			else
			{
				heatTempL = Temp;
				heatTempH = Math.max(heatTempL, heatTempH);
			}
			save = coolTempH - coolTempL;
			coolTempL = Math.max(heatTempH - 2, coolTempL);
			coolTempH = coolTempL + save;
			break
	}
}

function Draw()
{
	Gdi.Clear(0) // transaprent

	btnW = fontSize * 3
	btnX = Gdi.Width - btnW * 2 - 4
	btnY = fontSize * 2 + 14

	// rounded window
	Gdi.Brush( Gdi.Argb( 160, 0, 0, 0) )
	Gdi.FillRectangle(0, 0, Gdi.Width-1, Gdi.Height-1)
	Gdi.Pen( Gdi.Argb(255, 0, 0, 255), 1 )
	Gdi.Rectangle(0, 0, Gdi.Width-1, Gdi.Height-1)

	// Title
	Gdi.Font( 'Courier New', fontSize, 'BoldItalic')
	Gdi.Brush( Gdi.Argb(255, 255, 230, 25) )
	Gdi.Text( 'HVAC Remote', 5, 1 )

	color = Gdi.Argb(255, 255, 0, 0)
	Gdi.Brush( color )
	Gdi.Text( 'X', Gdi.Width-17, 1 )

	Gdi.Font( 'Arial' , fontSize  -1, 'Regular')
	Gdi.Brush( Gdi.Argb(255, 255, 255, 255) )

	date = new Date()
	Gdi.Text( date.toLocaleTimeString(), Gdi.Width - fontSize * 6.4, 2 )

	Gdi.Font( 'Arial' , fontSize, 'Regular')

	x = 5
	y = fontSize  + 4
	if(hvacJson == undefined || coolTempH == undefined)
		return

	Gdi.Text('In: ' + inTemp + '°', x, y)
	Gdi.Text( '>' + targetTemp + '°  ' + rh + '%', x + fontSize * 4, y)

	Gdi.Text('O:' + outTemp + '°', x + fontSize * 11, y)

	y = btnY
	Gdi.Text('Fan:', x, y); 	Gdi.Text(fan ? "On" : "Off", x + fontSize * 5, y, 'Right')
	y += fontSize + 4

	s = 'huh'
	switch(mode)
	{
		case 0: s = 'Off'; break
		case 1: s = 'Cooling'; break
		case 2: s = 'Heating'; break
		case 3: s = 'eHeating'; break
		case 4: s = 'Cycling'; break
	}

	bh = fontSize  + 3

	Gdi.Text('Run:', x, y)
	Gdi.Text(running ? s : "Off", x + fontSize * 7, y, 'Right')
	y += bh

	Gdi.Text('Cool Hi:', x, y); 	Gdi.Text(coolTempH.toFixed(1) + '°', x + fontSize * 8, y, 'Right')
	y += bh
	Gdi.Text('Cool Lo:', x, y); 	Gdi.Text(coolTempL.toFixed(1) + '°', x + fontSize * 8, y, 'Right')
	y += bh
	Gdi.Text('Heat Hi:', x, y); 	Gdi.Text(heatTempH.toFixed(1) + '°', x + fontSize * 8, y, 'Right')
	y += bh
	Gdi.Text('Heat Lo:', x, y); 	Gdi.Text(heatTempL.toFixed(1) + '°', x + fontSize * 8, y, 'Right')
	y += bh
	Gdi.Text('Threshold:', x, y); 	Gdi.Text(cycleThresh.toFixed(1) + '°', x + fontSize * 8, y, 'Right')
	y += bh
	Gdi.Text('ovr Time:', x, y); 	Gdi.Text(overrideTime , x + fontSize * 8, y, 'Time')
	y += bh
	a = +Reg.overrideTemp
	Gdi.Text('Override:', x, y);  Gdi.Text(a.toFixed(1) + '°' , x + fontSize * 8, y, 'Right')

	if(ovrActive)
		Gdi.Pen(Gdi.Argb(255,255,20,20), 2 )	// Button square
	else
		Gdi.Pen(Gdi.Argb(255,20,20,255), 2 )	// Button square
	Gdi.Rectangle(x, y, fontSize * 4, fontSize, 2)
	Pm.Button(x, y, fontSize * 4, fontSize)

	y = Gdi.Height - fontSize * 2.4

 	if(mode == 1 || (mode==2 && heatMode == 0))  // cool or HP
		cost = ppkwh * runTotal / (1000*60*60) * kwh
	else
		cost = ccfs * runTotal * cfm

	Gdi.Text('Filter:', x, y);  Gdi.Text(filterMins*60, x + fontSize * 7.5, y, 'Time')
	Gdi.Pen(Gdi.Argb(255,20,20,255), 2 )	// Button square
	Pm.Button(x, y, fontSize * 8, fontSize)
	Gdi.Rectangle(x, y, fontSize * 7.8, fontSize, 2)
	Gdi.Text('Cost:', x + fontSize * 7.8, y); 	Gdi.Text( '$' +cost.toFixed(2) , x + fontSize * 14, y, 'Right')

	y += bh
	Gdi.Text('Cycle:', x, y); 	Gdi.Text( cycleTimer, x + fontSize * 6, y, 'Time')
	Gdi.Text('Total:', x+fontSize * 7, y); 	Gdi.Text(runTotal, x + fontSize * 14, y, 'Time')

	heatModes = Array('HP', 'NG', 'Auto')
	fanModes = Array('Auto', 'On')
	buttons = Array(fanModes[fanMode], ' ',
		modes[mode], heatModes[heatMode],
		'+', '-', '+', '-', '+', '-', '+', '-', '+', '-', '+', '-', '+', '-' )

	for (n = 0, row = 0; row < buttons.length / 2; row++)
	{
		for (col = 0; col < 2; col++)
		{
			x = btnX + (col * btnW)
			y = btnY + (row * bh)
			drawButton(buttons[n++], x, y, btnW, bh-2)
		}
	}
}

function drawButton(text, x, y, w, h)
{
	Gdi.GradientBrush( 0,y, 22, 24, Gdi.Argb(200, 200, 200, 255), Gdi.Argb(200, 60, 60, 255 ), 90)
	Gdi.FillRectangle( x, y, w-2, h, 3)
	ShadowText( text, x+(w/2), y, Gdi.Argb(255, 255, 255, 255) )
	Pm.Button(x, y, w, h)
}

function ShadowText(str, x, y, clr)
{
	Gdi.Brush( Gdi.Argb(255, 0, 0, 0) )
	Gdi.Text( str, x+1, y+1, 'Center')
	Gdi.Brush( clr )
	Gdi.Text( str, x, y, 'Center')
}

function LogTemps(snd )
{
	if(cycleThresh == undefined)
		return
	if(targetTemp == 0)
		return

	if( (inTemp == last1 || inTemp == last2) && last == state+fan) // reduce logging some
	{
		last1  = last2
		last2 = inTemp
		return
	}
	if(state != cycleState)
	{
		cycleState = state
		cycleTotal += cycleTimer
		if(state == 0 && cycleTimer != lastCycleTimer && cycleTimer)
			Pm.Echo('Cycle = ' + cycleTimer + ' total = ' + cycleTotal)
	}
	last = state+fan
	last1  = last2
	last2 = inTemp

	ttL = targetTemp
	ttH = targetTemp

	if(Reg.hvacMode == 2)
		    ttH += cycleThresh // heat
	else ttH -= cycleThresh // cool

	if(mode != Reg.hvacMode)
	{
		Reg.hvacMode = mode
		Pm.Echo('HVAC mode change')
	}
	s = hvacJson.t + ',' + state + ',' + fan + ',' + inTemp + ',' + ttL + ',' + ttH.toFixed(1)+ ',' + rh
	for(i=0; i < snd.length; i++)
	{
		s += ',' + snd[i][1]/10
		s +=  ',' + snd[i][2]/10
	}
	Pm.Log( 'statTemp.log', s)
}
