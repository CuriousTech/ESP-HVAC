//  HvacRemote script running on PngMagic  http://www.curioustech.net/pngmagic.html
// Device is set to a fixed IP in the router

	hvacUrl = 'http://192.168.0.100:85'
	password = 'password'

	kwh = 3600 // killowatt hours (compressor+fan)
	ppkwh = 0.126  // price per KWH  (price / KWH)
	ppkwh  += 0.091 + 0.03 // surcharge 9.1%, school 3%, misc
	ccfs	= 0.70 / (60*60) // NatGas cost per hour divided into seconds

	modes = new Array('Off', 'Cool', 'Heat', 'Auto')

	btnX = 120
	btnY = 40
	btnW = 38

	if(Reg.overrideTemp == 0)
		Reg.overrideTemp = -1.2

	var hvacJson

	Pm.Window('HvacRemote')

	Gdi.Width = 208 // resize drawing area
	Gdi.Height = 320

	Pm.HVAC() // Start the HVAC listener

	Pm.SetTimer(60*1000)
	OnTimer()

// Handle published events
function OnCall(msg, event, data, d2)
{
	switch(msg)
	{
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
			if(data.length) procLine(event, data)
			break
		case 'HTTPCLOSE':
			break

		case 'hvacData':
			timeout = new Date()
//Pm.Echo('hvacData: ' + event)

			hvacJson = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
					event.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + event + ')')
			running = +hvacJson.r
			state = +hvacJson.s
			fan = +hvacJson.fr
			inTemp = +hvacJson.it / 10
			rh = +hvacJson.rh / 10
			targetTemp = +hvacJson.tt / 10
			filterMins = +hvacJson.fm
			outTemp = +hvacJson.ot / 10
			outFL = +hvacJson.ol
			outFH = +hvacJson.oh
			cycleTimer = +hvacJson.ct
			fanTimer = +hvacJson.ft
			runTotal = +hvacJson.rt

			if(Pm.FindWindow( 'History' ))
				Pm.History( 'REFRESH' )
			Draw()
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
					fanMode ^= 1; SetVar('fanmode', fanMode)
					break
				case 3:		// mode
					mode = (mode + 1) & 3; SetVar('mode', mode)
					break
				case 4:		// mode
					heatMode = (heatMode+1) % 3; SetVar('heatMode', heatMode)
					break
				case 5:		// Unused
					GetVar( 'rssi' )
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
				case 16:		// fanDelay up
					if(fanDelay < 255){ fanDelay += 10; SetVar('fanpostdelay', fanDelay); }
					break
				case 17:		// fanDelay dn
					if(fanDelay > 0){ fanDelay -= 10; SetVar('fanpostdelay', fanDelay); }
					break
				case 18:		// idleMin up
					idleMin++; SetVar('idlemin', idleMin)
					break
				case 19:		// idleMin dn
					idleMin--; SetVar('idlemin', idleMin)
					break
				case 20:		// cycleMin up
					cycleMin++; SetVar('cyclemin', cycleMin)
					break
				case 21:		// cycleMin dn
					cycleMin--; SetVar('cyclemin', cycleMin)
					break
				case 22:		// cycleMax up
					cycleMax+=60; SetVar('cyclemax', cycleMax)
					break
				case 23:		// cycleMax dn
					cycleMax--; SetVar('cyclemax', cycleMax)
					break
				case 24:		// override time up
					overrideTime+=60; SetVar('overridetime', overrideTime)
					break
				case 25:		// override time dn
					overrideTime-=10; SetVar('overridetime', overrideTime)
					break	
				case 26:		// override temp up
					Reg.overrideTemp += 0.1
					break
				case 27:		// override temp dn
					Reg.overrideTemp -= 0.1
					break
			}
			Draw()
			break

		default:
			Pm.Echo('HR Unrecognised ' + msg)
			break
	}
}

function OnTimer()
{
	GetSettings()
}

function SetVar(v, val)
{
	Http.Connect( 'setvar', hvacUrl + '/s?key=' + password + '&' + v + '=' + val  )
}

function GetSettings()
{
	Http.Connect('settings', hvacUrl + '/json' )
}

function procLine(event, data)
{
	Json = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
			data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')

	switch(event)
	{
		case 'settings':
			mode = +Json.m
			autoMode = +Json.am
			heatMode = +Json.hm
			fanMode = +Json.fm
			ovrActive = +Json.ot
			eHeatThresh = +Json.ht

			coolTempL = +Json.c0 / 10
			coolTempH = +Json.c1 / 10
			heatTempL = +Json.h0 / 10
			heatTempH = +Json.h1 / 10
			idleMin = +Json.im
			cycleMin = +Json.cn
			cycleMax = +Json.cx
			cycleThresh = +Json.ct / 10
			Reg.cycleThresh = cycleThresh
			fanDelay = Json.fd
			overrideTime = +Json.ov
			remoteTimer = Json.rm
			remoteTimeout = Json.ro

			Draw()
			break

		case 'setvar':
			break

		default:
			Pm.Echo('HR Unknown event: ' + event)
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
			if(Temp < 65.0 || Temp > 88.0)    // ensure sane values
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

	// rounded window
	Gdi.Brush( Gdi.Argb( 160, 0, 0, 0) )
	Gdi.FillRectangle(0, 0, Gdi.Width-1, Gdi.Height-1)
	Gdi.Pen( Gdi.Argb(255, 0, 0, 255), 1 )
	Gdi.Rectangle(0, 0, Gdi.Width-1, Gdi.Height-1)

	// Title
	Gdi.Font( 'Courier New', 15, 'BoldItalic')
	Gdi.Brush( Gdi.Argb(255, 255, 230, 25) )
	Gdi.Text( 'HVAC Remote', 5, 1 )

	color = Gdi.Argb(255, 255, 0, 0)
	Gdi.Brush( color )
	Gdi.Text( 'X', Gdi.Width-17, 1 )

	Gdi.Font( 'Arial' , 11, 'Regular')
	Gdi.Brush( Gdi.Argb(255, 255, 255, 255) )

	date = new Date()
	Gdi.Text( date.toLocaleTimeString(), Gdi.Width-84, 2 )
	Gdi.Font( 'Arial' , 13, 'Regular')

	x = 5
	y = 22
	if(hvacJson == undefined)
		return

	Gdi.Text('In: ' + inTemp + '°', x, y)
	Gdi.Text( '>' + targetTemp + '°  ' + rh + '%', x + 54, y)

	Gdi.Text('O:' + outTemp + '°', x + 150, y)

	y = btnY
	Gdi.Text('Fan:', x, y); 	Gdi.Text(fan ? "On" : "Off", x + 100, y, 'Right')
	y += 20

	s = 'huh'
	switch(mode)
	{
		case 1: s = 'Cooling'; break
		case 2: s = 'Heating'; break
		case 3: s = 'eHeating'; break
	}

	bh = 18

	Gdi.Text('Run:', x, y)
	Gdi.Text(running ? s : "Off", x + 100, y, 'Right')
	y += bh

	Gdi.Text('Cool Hi:', x, y); 	Gdi.Text(coolTempH.toFixed(1) + '°', x + 112, y, 'Right')
	y += bh
	Gdi.Text('Cool Lo:', x, y); 	Gdi.Text(coolTempL.toFixed(1) + '°', x + 112, y, 'Right')
	y += bh
	Gdi.Text('Heat Hi:', x, y); 	Gdi.Text(heatTempH.toFixed(1) + '°', x + 112, y, 'Right')
	y += bh
	Gdi.Text('Heat Lo:', x, y); 	Gdi.Text(heatTempL.toFixed(1) + '°', x + 112, y, 'Right')
	y += bh
	Gdi.Text('Threshold:', x, y); 	Gdi.Text(cycleThresh.toFixed(1) + '°', x + 112, y, 'Right')
	y += bh
	Gdi.Text('Fan Delay:', x, y); Gdi.Text(fanDelay , x + 112, y, 'Time')
	y += bh
	Gdi.Text('Idle Min:', x, y); 	Gdi.Text(idleMin , x + 112, y, 'Time')
	y += bh
	Gdi.Text('cycle Min:', x, y); 	Gdi.Text(cycleMin, x + 112, y, 'Time')
	y += bh
	Gdi.Text('cycle Max:', x, y); 	Gdi.Text(cycleMax , x + 112, y, 'Time')
	y += bh
	Gdi.Text('ovr Time:', x, y); 	Gdi.Text(overrideTime , x + 112, y, 'Time')
	y += bh
	a = Reg.overrideTemp
	Gdi.Text('Override:', x, y);  Gdi.Text(a + '°' , x + 112, y, 'Right')

	if(ovrActive)
		Gdi.Pen(Gdi.Argb(255,255,20,20), 2 )	// Button square
	else
		Gdi.Pen(Gdi.Argb(255,20,20,255), 2 )	// Button square
	Gdi.Rectangle(x, y, 64, 15, 2)
	Pm.Button(x, y, 64, 15)

	y = Gdi.Height - 36

 	if(mode == 1 || (mode==2 && heatMode == 0))  // cool or HP
		cost = ppkwh * runTotal / (1000*60*60) * kwh
	else
		cost = ccfs * runTotal

	Gdi.Text('Filter:', x, y);  Gdi.Text(filterMins*60, x + 100, y, 'Time')
	Gdi.Pen(Gdi.Argb(255,20,20,255), 2 )	// Button square
	Pm.Button(x, y, 100, 15)
	Gdi.Rectangle(x, y, 100, 15, 2)
	Gdi.Text('Cost:', x+110, y); 	Gdi.Text( '$' +cost.toFixed(2) , x + 190, y, 'Right')

	y += bh
	Gdi.Text('Cycle:', x, y); 	Gdi.Text( cycleTimer, x + 100, y, 'Time')
	Gdi.Text('Total:', x+110, y); 	Gdi.Text(runTotal, x + 190, y, 'Time')

	heatModes = Array('HP', 'NG', 'Auto')
	buttons = Array(fanMode ? 'On' : 'Auto', modes[mode],
		heatModes[heatMode], ' ',
		'+', '-', '+', '-', '+', '-', '+', '-', '+', '-', '+', '-', '+', '-', '+', '-', '+', '-', '+', '-', '+', '-' )

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
