// Remote thermostat stream listener
interval = 10 * 60
Url = 'http://192.168.0.198:80/events?int=' + interval + '&p=1'
if(!Http.Connected)
	Http.Connect( 'event', Url )  // Start the event stream

var last
Pm.SetTimer(10*1000)
heartbeat = 0
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
			Pm.Echo('RMT stream closed')
			break
	}
}

function procLine(data)
{
	if(data.length < 2) return
	if(data == ':ok' )
	{
		Pm.Echo( 'RMT stream started')
		return
	}

	if( data.indexOf( 'event' ) >= 0 )
	{
		event = data.substring( data.indexOf(':') + 2)
		return
	}
	else if( data.indexOf( 'data' ) >= 0 )
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
			LogRemote(data)
			break
		case 'print':
			Pm.Echo( 'RMT Print: ' + data)
			break
		case 'alert':
			Pm.Echo( 'RMT Alert: ' + data)
			Pm.Beep(0)
			break
	}
	event = ''
}

function OnTimer()
{
	time = (new Date()).valueOf()
	if(time - heartbeat > 60*1000)
	{
		if(!Http.Connected)
		{
			Pm.Echo('RMT timeout')
			Http.Connect( 'event', Url )  // Start the event stream
		}
	}
}

function 	LogRemote(str)
{
	rmtJson = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
		str.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + str + ')')

	line = rmtJson.tempi + ',' + rmtJson.rhi

	if(line == last || +rmtJson.tempi == -1)
		return
	last = line

	date = new Date()
	fso = new ActiveXObject( 'Scripting.FileSystemObject' )
	tf = fso.OpenTextFile( 'Remote.log', 8, true)
	tf.WriteLine( Math.floor(date.getTime() / 1000) + ',' + line )
	tf.Close()
	fso = null
}
