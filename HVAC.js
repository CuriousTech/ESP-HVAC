// HVAC stream listener (PngMagic)
interval = 5 * 60
Url = 'http://192.168.0.100:85/events?i=' + interval + '&p=1'

	oldState = 0
	oldFan = 0

heartbeat = (new Date()).valueOf()

if(!Http.Connected)
{
	Http.Connect( 'event', Url)  // Start the event stream
}

Pm.SetTimer(60*1000)

// Handle published events
function OnCall(msg, event, data)
{
	switch(msg)
	{
		case 'HTTPDATA':
			heartbeat = new Date()
			if(data.length <= 2) break // keep-alive heartbeat
			lines = data.split('\n')
			for(i = 0; i < lines.length; i++)
				procLine(lines[i])
			break
		case 'HTTPCLOSE':
			Http.Connect( 'event', Url )  // Start the event stream
			break
		case 'HTTPSTATUS':
			Pm.Echo('HVAC HTTP status ' + event)
			break
	}
}

function procLine(data)
{
	if(data.length < 2) return
	if(data == ':ok' )
	{
		Pm.Echo( 'HVAC stream started')
		return
	}

	if( data.indexOf( 'event' ) == 0 )
	{
		event = data.substring( data.indexOf(':') + 2)
		return
	}
	else if( data.indexOf( 'data' ) == 0 )
	{
		data = data.substring( data.indexOf(':') + 2)
	}
	else
	{
		return // headers
	}

	switch(event)
	{
		case 'state':
			Pm.HvacRemote('hvacData', data)
			updateData( data )
//			Pm.Echo('state ' + data)
			break
		case 'print':
			Pm.Echo('HVAC Print: ' + data)
			break
		case 'alert':
			Pm.Echo('HVAC Alert: ' + data)
			break
	}
	event = ''
}

function OnTimer()
{
	time = (new Date()).valueOf()
	if(time - heartbeat < 120*1000)
		return

	if(!Http.Connected)
	{
		Pm.Echo('HVAC timeout')
		Http.Connect( 'event', Url )  // Start the event stream
	}
}

function updateData(data)
{
	hvacJson = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
			data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')
	running = +hvacJson.r
	state = +hvacJson.s
	fan = +hvacJson.fr
	inTemp = +hvacJson.it / 10
	rh = +hvacJson.rh / 10
	targetTemp = +hvacJson.tt / 10

	date = new Date()
	if((date.getMinutes() & 3) == 0 || oldFan != fan || oldState != state)
	{
		LogTemps( state, fan, inTemp, targetTemp, rh )
		if( oldFan != fan || oldState != state)
			LogHVAC( Math.floor( date.getTime() / 1000), state, fan )
	}

	oldState = state
	oldFan = fan
}

function LogTemps( stat, fan, inTemp, targetTemp, inrh )
{
	if(targetTemp == 0)
		return
	fso = new ActiveXObject( 'Scripting.FileSystemObject' )

	date = new Date()

	tf = fso.OpenTextFile( 'inTemp.log', 8, true)
	tf.WriteLine( Math.floor( date.getTime() / 1000 ) + ',' + stat + ',' + fan + ',' + inTemp + ',' + targetTemp +',' + inrh )
	tf.Close()
	fso = null
}

function LogHVAC( uxt, state, fan )
{
	fso = new ActiveXObject( 'Scripting.FileSystemObject' )
	tf = fso.OpenTextFile( 'hvacOp.log', 8, true)
	tf.WriteLine( uxt + ',' + state + ',' + fan )
	tf.Close()
	fso = null
}
