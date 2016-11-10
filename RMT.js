Url = 'http://192.168.0.103:86/events'
if(!Http.Connected)
	Http.Connect( 'event', Url )  // Start the event stream

var last
mute = false
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
			mute = false
			lines = data.split('\n')
			for(i = 0; i < lines.length; i++)
				procLine(lines[i])
			break
		case 'HTTPSTATUS':
			Pm.Echo('RMT Status ' + event)
			break
		case 'HTTPCLOSE':
			Pm.Echo('RMT stream retry')
//			Http.Connect( 'event', Url )  // Start the event stream
			break
	}
}

function procLine(data)
{
	if(data.length < 2) return
	data = data.replace(/\n|\r/g, "")
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
	if(time - heartbeat > 120*1000)
	{
		if(!Http.Connected)
		{
			if(!mute)
			{
				mute = true
				Pm.Echo('RMT timeout')
			}
	//		Http.Connect( 'event', Url )  // Start the event stream
		}
	}
}

function LogRemote(str)
{
	rmtJson = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
		str.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + str + ')')

	line = rmtJson.tempi + ',' + rmtJson.rhi

//Pm.Echo(str)
	if(line == last || +rmtJson.tempi == -1)
		return
	last = line

//	Pm.Echo( new Date(rmtJson.t * 1000) )
	fso = new ActiveXObject( 'Scripting.FileSystemObject' )
	tf = fso.OpenTextFile( 'Remote.log', 8, true)
	tf.WriteLine( rmtJson.t + ',' + line )
	tf.Close()
	fso = null
}
