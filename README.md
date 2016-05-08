# ESP8266-HVAC
WiFi Smart HVAC Theromstat  

<b>Update:</b>  Changes have been made in the bom.txt file.  The schematic is incorrect for some resistor values.  4K7 should be used for the SHT21 pullups, and the serial resistors have been changed.  The Nextion has a 10K pullduown resistor (R13: next to the connector) that must be removed for the ESP to boot wothout serial debug connected, or add a 10K pullup between TX and 3V3.  I've opted for the removal of the resistor.  R16 definitely needs a 1K to work correctly.  

This will eventually replace the old Spark-O-Stat with a newer system using the ESP-07, with a better screen (Nextion HMI 2.8" touchscreen), 5 outputs (1 extra output for humidifier), option for SHT21 (I2C) or DHT22/11/AM2302, and anolog input for expansion.

Main and remote units are designed into the single PCB with wired 5V or USB Mini-B fro the remote units, and 24VAC for main.  Headers on top are for onbarod serial programming of the ESP and Nextion.  Only power it with 1 source at a time. (3V3 for just the ESP, encoder, and temp sensor, 5V to include the display, or 24VAC for all of it including the SSRs).

The watchdog chip (PIC10F) is unnecessary, but can be used to monitor activity and reset the ESP if it freezes.  All the components used are cheap.  The PCB is about $25 for 3 from OSH Park, and the Nextion display is $15 off eBay.

Some screens including a keyboard and SSID chooser, but this is using the auto connect with SoftAP server (which attempts to find the stored SSID while waiting on input) so it should never be needed.  The dimmed screensaver will change to a clock, and few other odd displays over time.  

![Some display screenshots](http://www.curioustech.net/images/hvacscreens.png)
