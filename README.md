#System Status Traffic Light
Have you ever wanted to use a Traffic Light to signal if something is working or not? Well I know we have always wanted one in the office here at Proctorio. We provide a service as well as use a number of third party services, all of which may or may not impact our daily operations. The Arduino checks for a value on a status page (a simple webpage that the server updates a value based on the information of our own status page: https://proctoriostatus.com/), and changes color accordingly. 

You will find the arduino file and schematic in this repository.

The code was written with the Arduino Yun as the target, not your regular Arduino.

You can find the full build writeup here: http://www.instructables.com/id/System-Status-Traffic-Light

#Features
Status is checked every 30 sec, change (statusInterval) if you'd like to change this.
Accommodates for wrong value, long page, broken webpage, no page (only reads first 5 characters).
If no page is found, retrys 3 times with shorter status interval, otherwise waits for next status interval.
Uses millis in the loop instead of delay() in order to always accept commands from the server without getting stuck in the status interval.

#Status Web Page
Status Page (statusPageUrl) displays a single character and is handeled as follows:
  	0 = Green = all systems normal, 
  	1 = Yellow = Ongoing Service Incident Posted, 
  	2 = Red = Proctorio Service Outage (outage that could be related to 3rd party, but not always), 
  	3 = Yellow + Green = Related/Unrelated 3rd party vendor is down or experiencing issues and has posted an incident (i.e. visualstudio is down, but that doesn't mean Proctorio's service is impacted), 
  	Any other value or more than one value = Flashing Yellow, 
	Page Not Found = Flashing Yellow and Red, 

#Server Commands
Server can send on/off command (master kill switch), the lightsOn bool is used to hold this value.
Be sure to change these commands (onCommand/offCommand) to something random to increase security.
You can also visit: "http://arduino's_IP_address/arduino/command" instead.
We use listenOnLocalhost(); to only accept commands from the local server, and our Yun is password protected.
You only need 3 relays, however we used a 4th for a master kill switch.
If you want to usd 3 relays instead, you can adjust the relay pins and digitalWrite's accordingly.
If you do not wish to recieve any commands from a server, you can remove all YunServer and ON/OFF code
 
#Debug Mode
You can un-comment the debug flag (#define DEBUG) to enable debug mode.
If left commented, all code between "#ifdef DEBUG" and "#endif" tags will be excluded when compiled.
Be sure to comment it out when you are done de-bugging.
The status interval is also set to 10 secs instead of the default value.
A valid serial connection (e.g. via USB cable) MUST be made to initialize the code.
Open the Serial Monitor and set the baud rate to 9600 to view the output.
