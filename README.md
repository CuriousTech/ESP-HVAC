# ESP8266-HVAC
WiFi Smart Omniscient HVAC Touchscreen Theromstat  

<b>Update 2:</b> Added a picture.  There will need to be a rev 1 board.  The SHT21 works fine on the desk, but reaches 91°F in the box.  I've moved it to an external board and not sure where to go with it now.  

<b>Update:</b>  Changes have been made in the bom.txt file.  The schematic is incorrect for some resistor values.  2K2 should be used for the SHT21 pullups.

Notes about the Nextion: First, add the line bauds=115200 to the main initialize varaibles once (if using MicroSD) or send it the command over serial. This will alow it to communicate at the highest speed.  

It has a 10K pullduown resistor (R13: next to the connector).  Removing this allows the ESP8266 to boot normally and operate without any resistors added, or add a 10K pullup between TX and 3V3 when serial debug is not connected.  I've opted for the removal of the resistor, which fixes all problems.  The 5K6 on the thermostat (R17) allowed it to operate with serial connected plus the pulldown, but not without.  The blue wire (data sent to the ESP) doesn't really need any resistors.  The ESP can't be programmed without a 1K while both are connected, and with the 1K and debug the touchscreen won't work.  So just disconnect the wire while programming and reconnect it after.  

This will eventually replace the old Spark-O-Stat with a newer system using the ESP-07, with a better screen (Nextion HMI 2.8" touchscreen), 5 outputs (1 extra output for humidifier), option for SHT21 (I2C) or DHT22/11/AM2302, and anolog input for expansion.

Main and remote units are designed into the single PCB with wired 5V or USB Mini-B for the remote units, and 24VAC for main.  Headers on top are for onbarod serial programming of the ESP and Nextion.  Only power it with 1 source at a time. (3V3 for just the ESP, encoder, and temp sensor, 5V to include the display, or 24VAC for all of it including the SSRs).

The watchdog chip (PIC10F) is unnecessary, but can be used to monitor activity and reset the ESP if it freezes.  All the components used are cheap.  The PCB is about $25 for 3 from OSH Park, and the Nextion display is $15 off eBay.

<b>Remote:</b>  The Remote folder contains all the changes to the Arduino folder needed to build the remote code that handles mirroring all display data, sending commands to the main unit, and streaming temp/rh.  By tapping the target temperature on the display, it will toggle transmitting the remote temperature and humidity to the main unit to use in all operations.  Tapping the same item on the main unit will also end the remote temperature connection.  There are notifications to indicate use as well as a flashing run indicator instead of solid.  

Some screens including a keyboard and SSID chooser, but this is using the auto connect with SoftAP server (which attempts to find the stored SSID while waiting on input) so it should never be needed.  The dimmed screensaver will change to a clock, and few other odd displays over time.  

![Some display screenshots](http://www.curioustech.net/images/hvacscreens2.png)

![The thing](http://www.curioustech.net/images/esphvac2.jpg)

Cool/heat low/high:  
These are the desired temperature ranges for cooling and heating, which are calculated based on outside temperature over the day.  

Threshold:  
The temperature offset to complete a cycle.  

Pre-cycle fan time:  
0 disables, 1 second to 5 minutes turns fan on to circulate air when threshold hit.  Thermostat takes over to reach target when it times out.  

Post-cycle fan time:  
Runs fan after cycle completes.  

Humidifier settings:  
Off: Just off  
Fan: Run when fan is on  
Run: Run when thermostat is running a cycle  
Auto1: Operate by humidistat during run cycles  
Auto2: Humidistat runs indepentantly of thermostat (shares fan control)  

Override:  
Use to heat or cool by a selected offset temperature for a specified time.

Idle min, cycle min/max:  
These are timers in seconds to control the thermstat operating limits.  Idle min is the shortest time between cycles.  Cycle min is the shortest time for a cycle, and max is the longest.  Be careful with these settings.  Running the compressor too short, too long or without the fan can cause damage.

![remotepage](http://www.curioustech.net/images/hvacremote.png)  

![dualtherms](http://www.curioustech.net/images/hvac2.jpg)  
