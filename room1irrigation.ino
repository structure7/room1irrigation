#include <SimpleTimer.h>
#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>            // Used by WidgetRTC.h
#include <WidgetRTC.h>

#include <ESP8266mDNS.h>        // Required for OTA
#include <WiFiUdp.h>            // Required for OTA
#include <ArduinoOTA.h>         // Required for OTA

/*
  #include <OneWire.h>
  #include <DallasTemperature.h>
  #define ONE_WIRE_BUS 12         // ESP-12E Pin D6
  OneWire oneWire(ONE_WIRE_BUS);
  DallasTemperature sensors(&oneWire);
*/

const char auth[] = "fromBlynkApp";

const char* ssid = "ssid";
const char* pw = "pw";

WiFiClient client;

SimpleTimer timer;

WidgetRTC rtc;
WidgetTerminal terminal(V2);

bool runOnce = true;

String currentTime = "(RTC not set)";
String currentTimeDate = "(RTC not set)";

int room1trays12 = 15;      // WeMos D1 Mini Pro pin D8
int room1trays34 = 13;      // WeMos D1 Mini Pro pin D7
int room2trays12 = 12;      // WeMos D1 Mini Pro pin D6
int room2trays34 = 14;      // WeMos D1 Mini Pro pin D5

bool readyToPickTray = true;
bool readyToPickDuration = false;
int currentTraySelection;
int currentDurationSelection;                   // In seconds, run duration (timer) selected from the app
bool readyToStartStatus = false;
bool auto12start, auto34start, man12start, man34start; // True when valve first opens.
bool auto12run, auto34run, man12run, man34run;      // True when valve is open.
unsigned long startTime12, startTime34;             // Unix time when valve first opened.
unsigned long stopTime12, stopTime34;               // Unix time when valve will close.
String counter12string;
String counter34string;
String counterUp12string;                           // Always counts up. Used for auto timer that usually counts down, but when stops reports how long it ran for.
String counterUp34string;

void setup()
{
  Serial.begin(9600);

  Blynk.begin(auth, ssid, pw);

  while (Blynk.connect() == false) {
    // Wait until connected
  }

  // Default all relays to off
  digitalWrite(room1trays12, LOW);
  digitalWrite(room1trays34, LOW);
  digitalWrite(room2trays12, LOW);
  digitalWrite(room2trays34, LOW);

  pinMode(room1trays12, OUTPUT);
  pinMode(room1trays34, OUTPUT);
  pinMode(room2trays12, OUTPUT);
  pinMode(room2trays34, OUTPUT);

  //sensors.begin();
  //sensors.setResolution(10);

  // START OTA ROUTINE
  ArduinoOTA.setHostname("thc1-WeMosD1mini");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println(String("[") + millis() + "] OTA Ready");
  Serial.print(String("[") + millis() + "] IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print(String("[") + millis() + "] MAC address: ");
  Serial.println(WiFi.macAddress());
  // END OTA ROUTINE

  rtc.begin();

  timer.setInterval(2000L, runOnceSub);
  timer.setInterval(1000L, timeSetAndCheck);
  timer.setInterval(500L, runTimer);
  timer.setInterval(1000L, stopWatcher);
}

void loop()
{
  timer.run();
  Blynk.run();
  ArduinoOTA.handle();
}

void runOnceSub() {
  if (runOnce == true && currentTimeDate == "(RTC not set)") {
    terminal.println(""); terminal.println("");
    terminal.print(".");
    terminal.flush();
  }
  else if (runOnce == true && currentTimeDate != "(RTC not set)") {
    terminal.println(""); terminal.println(""); terminal.println("");
    terminal.println(String(currentTimeDate) + " ** THC1 started! **");
    terminal.print(String("  RSSI=") + WiFi.RSSI() + ",IP=");
    terminal.println(WiFi.localIP());
    terminal.flush();
    runOnce = false;
    Blynk.virtualWrite(V3, "pick", 0);
    Blynk.virtualWrite(V4, "pick", 0);
  }
}

void runTimer() {   // This simply takes a start condition, sets those tanks as running, logs the start time, sets the end time, etc.
  if (auto12start == true) {    // If something started
    startTime12 = now();        // set the start time
    auto12run = true;           // set that something is running
    auto12start = false;        // and "lock out" the start condition.
  }
  if (auto34start == true) {
    startTime34 = now();
    auto34run = true;
    auto34start = false;
  }
  if (man12start == true) {
    startTime12 = now();
    man12run = true;
    man12start = false;
  }
  if (man34start == true) {
    startTime34 = now();
    man34run = true;
    man34start = false;
  }
}

void stopWatcher() {
  // Counts Trays 1 and 2 up during manual button press
  if (man12run == true)
  {
    if (((now() - startTime12) - (60 * ((now() - startTime12) / 60))) > 9) {
      counter12string = String( ((now() - startTime12) / 60) ) + ":" + ((now() - startTime12) - (60 * ((now() - startTime12) / 60)));
    }
    else {
      counter12string = String( ((now() - startTime12) / 60) ) + ":0" + ((now() - startTime12) - (60 * ((now() - startTime12) / 60)));
    }
  }
  // Counts Trays 1 and 2 down (and up) during auto duration selection
  else if (auto12run == true)
  {
    int i = ((stopTime12 - now()) / 60);
    if (((stopTime12 - now()) - (i * 60)) > 9) {
      counter12string = String(i) + ":" + ((stopTime12 - now()) - (i * 60));
      counterUp12string = String( ((now() - startTime12) / 60) ) + ":" + ((now() - startTime12) - (60 * ((now() - startTime12) / 60)));
    }
    else {
      counter12string = String(i) + ":0" + ((stopTime12 - now()) - (i * 60));
      counterUp12string = String( ((now() - startTime12) / 60) ) + ":0" + ((now() - startTime12) - (60 * ((now() - startTime12) / 60)));
    }
  }

  // Counts Trays 3 and 4 up during manual button press
  if (man34run == true)
  {
    if (((now() - startTime34) - (60 * ((now() - startTime34) / 60))) > 9) {
      counter34string = String( ((now() - startTime34) / 60) ) + ":" + ((now() - startTime34) - (60 * ((now() - startTime34) / 60)));
    }
    else {
      counter34string = String( ((now() - startTime34) / 60) ) + ":0" + ((now() - startTime34) - (60 * ((now() - startTime34) / 60)));
    }
  }
  // Counts Trays 3 and 4 down (and up) during auto duration selection
  else if (auto34run == true)
  {
    int i = ((stopTime34 - now()) / 60);
    if (((stopTime34 - now()) - (i * 60)) > 9) {
      counter34string = String(i) + ":" + ((stopTime34 - now()) - (i * 60));
      counterUp34string = String( ((now() - startTime34) / 60) ) + ":" + ((now() - startTime34) - (60 * ((now() - startTime34) / 60)));
    }
    else {
      counter34string = String(i) + ":0" + ((stopTime34 - now()) - (i * 60));
      counterUp34string = String( ((now() - startTime34) / 60) ) + ":0" + ((now() - startTime34) - (60 * ((now() - startTime34) / 60)));
    }
  }

  // Updates the status label in the app depending on which combinations of situations are true or false
  if ( (man12run == true && man34run == true) || (man12run == true && auto34run == true) || (auto12run == true && auto34run == true) || ( (auto12run == true && man34run == true) )) {
    Blynk.virtualWrite(V7, String("Tray 1/2 ") + counter12string + " | Tray 3/4 " + counter34string);
    Blynk.virtualWrite(V9, String("Tray 1/2 ") + counter12string + " | Tray 3/4 " + counter34string);   // V9 updates the master/summary page
  }
  else if ( (man12run == true && man34run == false ) || ( auto12run == true && auto34run == false ) || ( man12run == true && auto34run == false ) || ( auto12run == true && man34run == false )) {
    Blynk.virtualWrite(V7, String("Tray 1/2 ") + counter12string + " | Tray 3/4 OFF");
    Blynk.virtualWrite(V9, String("Tray 1/2 ") + counter12string + " | Tray 3/4 OFF");
  }
  else if ( (man12run == false && man34run == true) || ( auto12run == false && auto34run == true ) || ( man12run == false && auto34run == true ) || ( auto12run == false && man34run == true )) {
    Blynk.virtualWrite(V7, String("Tray 1/2 OFF | Tray 3/4 ") + counter34string);
    Blynk.virtualWrite(V9, String("Tray 1/2 OFF | Tray 3/4 ") + counter34string);
  }
  else {
    Blynk.virtualWrite(V7, "Watering system OFF");
    Blynk.virtualWrite(V9, "Watering system OFF");
  }

  // Turns off automatic watering when timer reaches zero.
  if (stopTime12 <= now() && (auto12run == true) ) {
    digitalWrite(room1trays12, LOW);
    Blynk.virtualWrite(V0, 0);
    Blynk.syncVirtual(V0);
  }
  if (stopTime34 <= now() && (auto34run == true) ) {
  digitalWrite(room1trays34, LOW);
    Blynk.virtualWrite(V1, 0);
    Blynk.syncVirtual(V1);
  }
  

}

void water12() {
  Blynk.virtualWrite(V0, 1);
  Blynk.setProperty(V0, "onLabel", "AUTO RUN");
  Blynk.syncVirtual(V0);
}

void water34() {
  Blynk.virtualWrite(V1, 1);
  Blynk.setProperty(V1, "onLabel", "AUTO RUN");
  Blynk.syncVirtual(V1);
}

BLYNK_WRITE(V0)  // Trays 1 & 2 manual button
{
  int pinData = param.asInt();

  if (pinData == 1) // Start manual watering
  {
    digitalWrite(room1trays12, HIGH);
    Blynk.setProperty(V0, "onLabel", "MANUAL RUN");
    if (auto12run == false) {
      man12start = true;
    }
  }
  if (pinData == 0) // Normal button state
  {
    digitalWrite(room1trays12, LOW);
    Blynk.setProperty(V0, "onLabel", "MANUAL RUN");   // Placed here to queue up the correct labeling for when pinData == 1
    if (man12run == true) {
      terminal.println(String(currentTime) + " Trays 1 & 2 watered for " + counter12string + ".");
    }
    else if (auto12run == true) {
      terminal.println(String(currentTime) + " Trays 1 & 2 watered for " + counterUp12string + ".");
    }
    terminal.flush();
    man12run = false;
    auto12run = false;
  }
}

BLYNK_WRITE(V1)  // Trays 3 & 4 manual button
{
  int pinData = param.asInt();

  if (pinData == 1) // Start manual watering
  {
    digitalWrite(room1trays34, HIGH);
    Blynk.setProperty(V1, "onLabel", "MANUAL RUN");
    if (auto34run == false) {
      man34start = true;
    }
  }
  if (pinData == 0) // Normal button state
  {
    digitalWrite(room1trays34, LOW);
    Blynk.setProperty(V1, "onLabel", "MANUAL RUN");   // Placed here to queue up the correct labeling for when pinData == 1
    if (man34run == true) {
      terminal.println(String(currentTime) + " Trays 3 & 4 watered for " + counter34string + ".");
    }
    else if (auto34run == true) {
      terminal.println(String(currentTime) + " Trays 3 & 4 watered for " + counterUp34string + ".");
    }
    terminal.flush();
    man34run = false;
    auto34run = false;
  }
}

BLYNK_WRITE(V3) {   // Tray selection
  switch (param.asInt())
  {
    case 2:
      if (readyToPickTray == true) {
        currentTraySelection = 12;
        readyToPickDuration = true;
      }
      break;
    case 3:
      if (readyToPickTray == true) {
        currentTraySelection = 34;
        readyToPickDuration = true;
      }
      break;
  }
}

BLYNK_WRITE(V4) {   // Run duration
  switch (param.asInt())
  {
    case 2:
      if (readyToPickDuration == true) {
        currentDurationSelection = 60;   // 1m
        readyToStartStatus = true;
      }
      break;
    case 3:
      if (readyToPickDuration == true) {
        currentDurationSelection = 300;  // 5m
        readyToStartStatus = true;
      }
      break;
    case 4:
      if (readyToPickDuration == true) {
        currentDurationSelection = 600;  // 10m
        readyToStartStatus = true;
      }
      break;
    case 5:
      if (readyToPickDuration == true) {
        currentDurationSelection = 1200;  // 20m
        readyToStartStatus = true;
      }
      break;
    case 6:
      if (readyToPickDuration == true) {
        currentDurationSelection = 1800;   // 30m
        readyToStartStatus = true;
      }
      break;
  }
}

BLYNK_WRITE(V5) // Start watering button
{
  int pinData = param.asInt();

  if (pinData == 1 && currentTraySelection == 12 && currentDurationSelection != 0)
  {
    Blynk.virtualWrite(V3, "pick", 0);
    Blynk.virtualWrite(V4, "pick", 0);
    readyToPickDuration = true;
    readyToPickDuration = false;
    readyToStartStatus = false;
    auto12start = true;
    currentTraySelection = 0;
    stopTime12 = now() + currentDurationSelection;    // set the end time
    currentDurationSelection = 0;
    water12();
  }
  else if (pinData == 1 && currentTraySelection == 34 && currentDurationSelection != 0)
  {
    Blynk.virtualWrite(V3, "pick", 0);
    Blynk.virtualWrite(V4, "pick", 0);
    readyToPickDuration = true;
    readyToPickDuration = false;
    readyToStartStatus = false;
    auto34start = true;
    currentTraySelection = 0;
    stopTime34 = now() + currentDurationSelection;
    currentDurationSelection = 0;
    water34();
  }
}

BLYNK_WRITE(V6) // Tray/duration clear
{
  int pinData = param.asInt();

  if (pinData == 1)
  {
    Blynk.virtualWrite(V3, "pick", 0);
    Blynk.virtualWrite(V4, "pick", 0);
    readyToPickDuration = true;
    readyToPickDuration = false;
    readyToStartStatus = false;
  }
}

BLYNK_WRITE(V8) // Emergency stop
{
  int pinData = param.asInt();

  if (pinData == 1 && ( (man12run == true && man34run == true) || (man12run == true && auto34run == true) || (auto12run == true && auto34run == true) || ( (auto12run == true && man34run == true) ) ))
  {
    digitalWrite(room1trays12, LOW);
    Blynk.virtualWrite(V0, 0);
    Blynk.syncVirtual(V0);
    timer.setTimeout(1000, v8part2);
  }
  else if (pinData == 1 && ( (man12run == true && man34run == false ) || ( auto12run == true && auto34run == false ) || ( man12run == true && auto34run == false ) || ( auto12run == true && man34run == false ) ))
  {
    digitalWrite(room1trays12, LOW);
    Blynk.virtualWrite(V0, 0);
    Blynk.syncVirtual(V0);
  }
  else if (pinData == 1 && ( (man12run == false && man34run == true) || ( auto12run == false && auto34run == true ) || ( man12run == false && auto34run == true ) || ( auto12run == false && man34run == true ) ))
  {
    digitalWrite(room1trays34, LOW);
    Blynk.virtualWrite(V1, 0);
    Blynk.syncVirtual(V1);
  }
}

void v8part2() {
  digitalWrite(room1trays34, LOW);
  Blynk.virtualWrite(V1, 0);
  Blynk.syncVirtual(V1);
}

void timeSetAndCheck()
{
  if (year() != 1970)
  {
    // Below gives me leading zeros on minutes and AM/PM.
    if (minute() > 9 && hour() > 11 && second() > 9) {    // No zeros needed at all, PM.
      currentTime = String(hourFormat12()) + ":" + minute() + ":" + second() + "pm";
    }
    else if (minute() > 9 && hour() > 11 && second() < 10) {    // Zero only for second, PM.
      currentTime = String(hourFormat12()) + ":" + minute() + ":0" + second() + "pm";
    }
    else if (minute() < 10 && hour() > 11 && second() > 9) {  // Zero only for hour, PM.
      currentTime = String(hourFormat12()) + ":0" + minute() + ":" + second() + "pm";
    }
    else if (minute() < 10 && hour() > 11 && second() < 10) {  // Zero for hour and second, PM.
      currentTime = String(hourFormat12()) + ":0" + minute() + ":0" + second() + "pm";
    }

    if (minute() > 9 && hour() < 12 && second() > 9) {    // No zeros needed at all, AM.
      currentTime = String(hourFormat12()) + ":" + minute() + ":" + second() + "am";
    }
    else if (minute() > 9 && hour() < 12 && second() < 10) {    // Zero only for second, AM.
      currentTime = String(hourFormat12()) + ":" + minute() + ":0" + second() + "am";
    }
    else if (minute() < 10 && hour() < 12 && second() > 9) {  // Zero only for hour, AM.
      currentTime = String(hourFormat12()) + ":0" + minute() + ":" + second() + "am";
    }
    else if (minute() < 10 && hour() < 12 && second() < 10) {  // Zero for hour and second, AM.
      currentTime = String(hourFormat12()) + ":0" + minute() + ":0" + second() + "am";
    }

    // Below gives me leading zeros on minutes and AM/PM.
    if (minute() > 9 && hour() > 11 && second() > 9) {    // No zeros needed at all, PM.
      currentTimeDate = String(hourFormat12()) + ":" + minute() + ":" + second() + "pm " + dayShortStr(weekday()) + " " + month() + "/" + day();
    }
    else if (minute() > 9 && hour() > 11 && second() < 10) {    // Zero only for second, PM.
      currentTimeDate = String(hourFormat12()) + ":" + minute() + ":0" + second() + "pm " + dayShortStr(weekday()) + " " + month() + "/" + day();
    }
    else if (minute() < 10 && hour() > 11 && second() > 9) {  // Zero only for hour, PM.
      currentTimeDate = String(hourFormat12()) + ":0" + minute() + ":" + second() + "pm " + dayShortStr(weekday()) + " " + month() + "/" + day();
    }
    else if (minute() < 10 && hour() > 11 && second() < 10) {  // Zero for hour and second, PM.
      currentTimeDate = String(hourFormat12()) + ":0" + minute() + ":0" + second() + "pm " + dayShortStr(weekday()) + " " + month() + "/" + day();
    }

    if (minute() > 9 && hour() < 12 && second() > 9) {    // No zeros needed at all, AM.
      currentTimeDate = String(hourFormat12()) + ":" + minute() + ":" + second() + "am " + dayShortStr(weekday()) + " " + month() + "/" + day();
    }
    else if (minute() > 9 && hour() < 12 && second() < 10) {    // Zero only for second, AM.
      currentTimeDate = String(hourFormat12()) + ":" + minute() + ":0" + second() + "am " + dayShortStr(weekday()) + " " + month() + "/" + day();
    }
    else if (minute() < 10 && hour() < 12 && second() > 9) {  // Zero only for hour, AM.
      currentTimeDate = String(hourFormat12()) + ":0" + minute() + ":" + second() + "am " + dayShortStr(weekday()) + " " + month() + "/" + day();
    }
    else if (minute() < 10 && hour() < 12 && second() < 10) {  // Zero for hour and second, AM.
      currentTimeDate = String(hourFormat12()) + ":0" + minute() + ":0" + second() + "am " + dayShortStr(weekday()) + " " + month() + "/" + day();
    }
  }
}
