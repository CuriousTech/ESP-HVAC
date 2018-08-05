rmtIP = '192.168.0.105:86'
Url = 'ws://' + rmtIP + '/ws'

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
			mute = false
//Pm.Echo(data)
			procLine(data)
			break
		case 'HTTPSTATUS':
			Pm.Echo('RMT Status ' + event + ' ' + data)
			break
		case 'HTTPCLOSE':
			Pm.Echo('RMT stream retry')
			break
	}
}

function procLine(data)
{
	if(data.length < 2) return
	data = data.replace(/\n|\r/g, "")
	parts = data.split(';')

	switch(parts[0])
	{
		case 'state':
			LogRemote(parts[1])
			break
		case 'print':
			Pm.Echo( 'RMT Print: ' + parts[1])
			break
		case 'alert':
			Pm.Echo( 'RMT Alert: ' + parts[1])
			Pm.Beep(0)
			break
		case 'OTA':
			Pm.Echo( 'RMT Update: ' + parts[1])
			break
	}
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
			Http.Connect( 'event', Url )  // Start the event stream
		}
	}
}

function LogRemote(str)
{
	rmtJson = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
		str.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + str + ')')

	line = rmtJson.tempi + ',' + rmtJson.rhi

	if(line == last || +rmtJson.tempi == -1)
		return
	last = line

	Pm.Log( 'Remote.log', rmtJson.t + ',' + line )
}
