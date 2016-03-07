/*
 * Proctorio System Status Stoplight (Traffic Signal)
 * 
 * Author: Jon (@jonnycyborg - makers@proctorio.com)
 * Date: 3/6/16
 * Version: 1.1
 * 
 * This project is released under the GNU General Public License *see below for details*
 * You can find the latest version here: https://git.io/v2NAi
 * You can find the entire build here: http://www.instructables.com/id/System-Status-Traffic-Light
 * A crucial aspect of free software is that users are free to cooperate.
 * It is absolutely essential to permit users who wish to help each other to share their bug fixes and improvements with other users.
 * My code is well commented but isn't prfect, if you have a question or find a bug or improvement please post a Issue/Pull Request on GitHub!
 * 
 *--------------------GNU General Public License------------------------
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *----------------------------------------------------------------------
 * 
 * ---Status Web Page---
 * Status Page displays a single character and is handeled as follows:
 *  0 = Green = all systems normal
 *  1 = Yellow = Ongoing Service Incident Posted
 *  2 = Red = Proctorio Service Outage (outage that could be related to 3rd party, but not always)
 *  3 = Yellow + Green = Related/Unrelated 3rd party vendor is down or experiencing issues and has posted an incident (i.e. visualstudio is down, but that doesn't mean Proctorio's service is impacted)
 *  Any other value or more than one value = Flashing Yellow
 *  Page Not Found = Flashing Yellow and Red
 * 
 * ---Server Commands---
 * Server can send on/off command (master kill switch), the lightsOn bool is used to hold this value.
 * Be sure to change these commands (onCommand/offCommand) to something random to increase security.
 * You can also visit: "http://arduino's_IP_address/arduino/command" instead.
 * We use listenOnLocalhost(); to only accept commands from the local server, and our Yun is password protected.
 * You only need 3 relays, however we used a 4th for a master kill switch.
 * If you want to usd 3 relays instead, you can adjust the relay pins and digitalWrite's accordingly.
 * If you do not wish to recieve any commands from a server, you can remove all YunServer and ON/OFF code
 * 
 * ---DEBUG MODE---
 * You can un-comment the debug flag (#define DEBUG) to enable debug mode.
 * If left commented, all code between "#ifdef DEBUG" and "#endif" tags will be excluded when compiled.
 * Be sure to comment it out when you are done de-bugging.
 * The status interval is also set to 10 secs instead of the default value.
 * A valid serial connection (e.g. via USB cable) MUST be made to initialize the code.
 * Open the Serial Monitor and set the baud rate to 9600 to view the output.
 * 
 * ---Features---
 * Status is checked every 30 sec, change "statusInterval" if you'd like to change this.
 * Accommodates for wrong value, long page, broken webpage no page (only reads first 5 characters).
 * If no page is found, retrys 3 times with shorter status interval, otherwise waits for next status interval.
 * Uses millis in the loop instead of delay in order to always accept commands from the server without getting stuck in the status interval.
 * 
 */

#include <Bridge.h>
#include <HttpClient.h>
#include <YunServer.h>
#include <YunClient.h>

YunServer server; // instantiate a server enabling the the Yún to listen for connected clients

//#define DEBUG // comment out to omit all lines from compiler and disable debug mode

bool lightsOn = false; // state used if the traffic signal is on or off (will not grab status page untill turned on by server when false on 1st bootup)

// you will need to modify these to reflect your setup
String statusPageUrl = "_status_page_URL_"; // status webpage URL, you will need to change this to yours (e.g. http://google.com) 
String onCommand = "_on_command_"; // 4 character on command sent from server (e.g. "ONNN", although I'd use something more secure)
String offCommand = "_off_command_"; // 4 character off command sent from server (e.g. "OFFF")

// setup output pins
const int debugLed = 13;  // onboard arduino led 
const int relayPin1 = 8;  // (Master Kill) IN1 pin on relay module connected to digital pin 8 on arduino
const int relayPin2 = 9;  // (Green) IN2 pin on relay module connected to digital pin 8 on arduino
const int relayPin3 = 10; // (Yellow) IN3 pin on relay module connected to digital pin 10 on arduino
const int relayPin4 = 11; // (Red) IN4 pin on relay module connected to digital pin 11 on arduino

// default delay to ping server for status
int statusInterval = 30000; // 30 sec

// if (error detected) then this is the rate the colors will flash
int flashInterval = 2000; // 2 sec

// current millis
unsigned long currentMillis = 0;

// holds millis of last time statusUpdate() was run
unsigned long lastStatusMillis = 0;

// holds millis of last time flashStatus() was run
unsigned long lastFlashMillis = 0;

// chars read from page assembled into string
String pageString = "";

// integers that hold status value
int statusInt = 0; 
int lastStatusInt = -1; // keep different value than statusInt on startup to trigger first write 
int retry = 0;
int flashState = 0;

void setup() {
  // sets the digital pins as outputs
  pinMode(debugLed, OUTPUT);
  pinMode(relayPin1, OUTPUT);      
  pinMode(relayPin2, OUTPUT);
  pinMode(relayPin3, OUTPUT);
  pinMode(relayPin4, OUTPUT);

  /*
   * This code prevents relays/leds from starting up engaged
   * HIGH/LOW will depend on your setup and normally open (NO)/normally closed (NC)
   * In our case, the relay module's optoisolators (relay trigger) are closed when LOW (pulled to ground)
   * (i.e. HIGH=OFF, LOW=ON)
   */
  digitalWrite(relayPin1, HIGH); // turn relay 1 (Kill Switch) off
  digitalWrite(relayPin2, HIGH); // turn relay 2 (Green) off
  digitalWrite(relayPin3, HIGH); // turn relay 3 (Yellow) off
  digitalWrite(relayPin4, HIGH); // turn relay 4 (Red) off
  digitalWrite(debugLed, LOW); // turn debug led off

  #ifdef DEBUG     
    Serial.begin(9600);
    while (!Serial); // wait for a serial connection to be made before starting code
    Serial.println("Serial Monitor Connected!");
    statusInterval = 10000; // change status interval to 10 sec
    Serial.println("---Debug Enabled---");
    Serial.print("Debug Status Interval (ms): ");
    Serial.println(statusInterval);
  #endif

  // bridge takes a few seconds to start up
  #ifdef DEBUG
    Serial.println("Waiting for Bridge...");
  #endif
  Bridge.begin();
  #ifdef DEBUG
    Serial.println("Bridge Initialized!");
  #endif
  // flash on-board LED when initialized
  for (int i = 0; i < 3; i++){
    digitalWrite(debugLed, HIGH);
    delay(500);
    digitalWrite(debugLed, LOW);
    delay(100);
  }

  // tell the server to listen for incoming connections only from localhost and start it
  #ifdef DEBUG
    Serial.println("Setting Up Server...");
  #endif
  server.listenOnLocalhost();
  server.begin();
  #ifdef DEBUG
    Serial.println("Server Initialized!");
  #endif
  // flash on-board LED when initialized
  for (int i = 0; i < 2; i++){
    digitalWrite(debugLed, HIGH);
    delay(500);
    digitalWrite(debugLed, LOW);
    delay(100);
  }
  
  #ifdef DEBUG
    Serial.print("Light Status: ");
  #endif
  if (lightsOn == true) {
    #ifdef DEBUG
      Serial.println("ON");
    #endif
    digitalWrite(relayPin1, LOW); // turn relay 1 (Kill Switch) on
    digitalWrite(debugLed, HIGH);
  } else {
    #ifdef DEBUG
      Serial.println("OFF - Waiting for command from server, will not grab status updates...");
    #endif
  }
  #ifdef DEBUG
    Serial.println("---Setup Complete---");
  #endif
}

void loop() {
  
  // create a named instance of YunClient to get any clients coming from the server
  YunClient client = server.accept();
  // if there is a new client connected, read the command the client sent
  if (client.connected()) {
    #ifdef DEBUG
      Serial.println("Client Connected");
    #endif
    processCommand(client); // void defined below that determins what to do with the command
    client.stop();
    #ifdef DEBUG
      Serial.println("Client Disconnected");
    #endif
  }
  delay(50); // add delay to keep arduino from working too hard

  /*  
   * We use millis to keep track of the current time 
   *  and when the last status was checked vs the status interval.
   * This is to avoid adding a delay(statusInterval); to 
   *  the end of the loop which would pause the loop and not recieve
   *  on/off commands from the server until the delay is finished. 
   */
  currentMillis = millis();

  /* 
   *  Status update is needed if:
   *  - it's the first time starting the arduino (lastStatusMillis = 0)
   *  - or the light has just been turned on (lastStatusMillis = 0)
   *  - if the last attempt failed and has retrys in this interval
   *  - or the status interval has been reached
   *  - and only if the light is currenlty on
   */
  if (lightsOn == true) {
    if(lastStatusMillis == 0 || retry > 0 || currentMillis - lastStatusMillis > (unsigned long)statusInterval) {
      if (currentMillis - lastStatusMillis > (unsigned long)statusInterval/3) { // when retry > 0 and on startup is true above or light has just been turend on, cut the interval to repeat more frequently
        getWebpage(); // void to get webpage data
        statusUpdate(); // void to change color of light
      }
    }
    if (flashState > 0) {
      if (currentMillis - lastFlashMillis >= flashInterval) {
        #ifdef DEBUG 
          Serial.println("***FLASHING***");   
        #endif
        flashStatus(); // void to flash status if mode (flashState) is set
      }
    }
  }
}

//**********Functions**********
void processCommand(YunClient client) {
  String command = client.readStringUntil('/');
  command.trim();
  #ifdef DEBUG
    Serial.print("Command recieved: ");
    Serial.println(command);
  #endif
  if(command.length() == 4) { 
    if (command == onCommand) {
      #ifdef DEBUG
        Serial.println("***LIGHT ON***");
      #endif
      digitalWrite(relayPin1, LOW);
      digitalWrite(debugLed, HIGH);
      lastStatusMillis = 0; // will flag loop to get a new status, regardless of status interval yet
      lastStatusInt = -1; // clear old value in case new value is the same
      pageString = ""; // reset page string to force new light status update
      lightsOn = true; 
    } else if (command == offCommand) {
      #ifdef DEBUG
        Serial.println("...LIGHT OFF...");
      #endif
      digitalWrite(relayPin1, HIGH);
      digitalWrite(relayPin2, HIGH); // turn relay 2 (Green) off
      digitalWrite(relayPin3, HIGH); // turn relay 3 (Yellow) off
      digitalWrite(relayPin4, HIGH); // turn relay 4 (Red) off
      digitalWrite(debugLed, LOW);
      lightsOn = false;
    }
  } else {
    #ifdef DEBUG
      Serial.print("INVALID COMMAND: ");
      Serial.println(command);
    #endif
  }
}

void getWebpage() {
  // initialize the http client and make a HTTP request
  HttpClient client;
  client.get(statusPageUrl);
  #ifdef DEBUG
    Serial.println("------------------");
  #endif
  // if there are incoming bytes available from page server, read them and add to string
  if (client.available()) {
    #ifdef DEBUG
      Serial.println("---Webpage Data---");
    #endif
    while (client.available() && pageString.length() <= 5) { // auto cut off after 5 bytes
      char c = client.read(); // get one byte from page
      pageString += c; // store page data to a string
      retry = 0; // reset retry value to default
    }
    #ifdef DEBUG
      Serial.flush(); // waits for the transmission of outgoing serial data to complete
      Serial.println("------------------");
      Serial.println(pageString); //only first 5 bytes
      Serial.println("------------------");
    #endif
  } else {
    retry = retry - 1;
    if (retry == -1) retry = 3; // default value is 0, implies first time failing
    if (retry > 0) {
      #ifdef DEBUG 
        Serial.print("Webpage Unavailable, retrying: ");
        Serial.print(retry);
        Serial.println(" more times...");
        Serial.println("------------------");
      #endif
    } else {
      #ifdef DEBUG
        Serial.println("Could not find page :(");
        Serial.println("Waiting for next status interval...");
        Serial.println("------------------");
      #endif
      pageString = ""; // reset page string
      retry = 0; // reset to default, stop retrying until next interval
    }
  }
}

void statusUpdate() {
  /* 
   * Run to set to the appropriate color.
   * Status page displays a single value (0 = green, 1 = yellow, 2 = red. 3 = yellow + green).
   * If the length is anything more than 1 value or it is a unrecognized value, then set the light flashing yellow. (wrong page? page changed?)
   * If there is no value (page not found), set it to flashing yellow and red. (internet out? page down?)
   */
  #ifdef DEBUG
    Serial.print("Last status run: ");
    Serial.print(currentMillis - lastStatusMillis);
    Serial.println(" ms ago...");
    Serial.print("Stop Light Status: ");
  #endif
  
  // convert single value string to int
  if (pageString.length() > 0) { // string.toInt() makes a string with no length ("") = 0 which is green in our case
    statusInt = pageString.toInt(); // could be optimized to use strings instead, since pageString is capped at 5 characters elilminating long string comparison
  } else {
    // to trigger next if statement below, 
    statusInt = -1; // again further optimization could be made by setting statusString to "UNKWN" or something similar
  }

  // set traffic signal status
  if (statusInt != lastStatusInt || statusInt == -1) {
    if (pageString == "0") { // (statusInt == 0) {
      #ifdef DEBUG
        Serial.println("GREEN");
      #endif 
      digitalWrite(relayPin2, LOW); // Green
      digitalWrite(relayPin3, HIGH);
      digitalWrite(relayPin4, HIGH);
      flashState = 0;
    } else if (pageString == "1") { // (statusInt == 1) {
      #ifdef DEBUG
        Serial.println("YELLOW");
      #endif
      digitalWrite(relayPin2, HIGH);
      digitalWrite(relayPin3, LOW); // Yellow
      digitalWrite(relayPin4, HIGH);
      flashState = 0;
    } else if (pageString == "2") { // (statusInt == 2) {
      #ifdef DEBUG
        Serial.println("RED");
      #endif
      digitalWrite(relayPin2, HIGH);
      digitalWrite(relayPin3, HIGH);
      digitalWrite(relayPin4, LOW); // Red
      flashState = 0;
    } else if (pageString == "3") { // (statusInt == 3) {
      #ifdef DEBUG
        Serial.println("YELLOW/GREEN");
      #endif
      digitalWrite(relayPin2, LOW); // Green
      digitalWrite(relayPin3, LOW); // Yellow
      digitalWrite(relayPin4, HIGH);
      flashState = 0;
    } else if (pageString == "") { // string is empty = no page found
      #ifdef DEBUG
        Serial.println("FLASHING YELLOW/RED - NO PAGE FOUND");
      #endif
      digitalWrite(relayPin2, HIGH); // turn off green, let flash status handle flash
      flashState = 2;
    } else { // any other value
      #ifdef DEBUG
        Serial.print("FLASHING YELLOW - INVALID STRING: ");
        Serial.print(pageString.substring(0,5));
        Serial.println("...");
      #endif
      digitalWrite(relayPin2, HIGH); // turn off green, let flashstatus() handle flash
      digitalWrite(relayPin4, HIGH); // turn off red, let flashstatus() handle flash
      flashState = 1; // set state to flashing yellow
    }
  } else {
    #ifdef DEBUG
      Serial.println("Unchanged... ");
    #endif
  }
  /* helpful to uncomment for tracking status values
  #ifdef DEBUG
      Serial.print("StatusInt:  ");
      Serial.println(statusInt);
      Serial.print("Last StatusInt:  ");
      Serial.println(lastStatusInt);
      Serial.print("PageString:  ");
      Serial.println(pageString);
  #endif
  */

  // cleanup
  pageString = ""; // reset string
  lastStatusInt = statusInt; // store last status
  lastStatusMillis = currentMillis; // save when statusUpdate() was last run
}

void flashStatus() { 
  // kept seperate from statusUpdate because flash interval is less than status interval and using millis
  // use each relay rather than kill switch because kill switch relay gets the most use, and we can prolong it's life
  if (flashState == 1) { // flashing Yellow
    if (digitalRead(relayPin3) == LOW) {
      digitalWrite(relayPin3, HIGH); // Yellow
      #ifdef DEBUG
          Serial.println("YELLOW - OFF");
      #endif
    } else { // if (digitalRead(relayPin3) == HIGH) {
      digitalWrite(relayPin3, LOW); // Yellow
      #ifdef DEBUG
          Serial.println("YELLOW - ON");
      #endif
    }
  }
  if (flashState == 2){ // flashing Red and Yellow
    if (digitalRead(relayPin3) == LOW && digitalRead(relayPin4) == LOW) {
      digitalWrite(relayPin3, HIGH); // Yellow
      digitalWrite(relayPin4, HIGH); // Red
      #ifdef DEBUG
          Serial.println("YELLOW/RED - OFF");
      #endif
    } else { // if (digitalRead(relayPin3) == HIGH && digitalRead(relayPin4) == HIGH) {
      digitalWrite(relayPin3, LOW); // Yellow
      digitalWrite(relayPin4, LOW); // Red
      #ifdef DEBUG
          Serial.println("YELLOW/RED - ON");
      #endif
    }
  }
  lastFlashMillis = currentMillis; // save when statusUpdate() was last run
}
