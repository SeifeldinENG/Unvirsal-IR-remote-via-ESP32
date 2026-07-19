#include <Arduino.h>
#include <IRremote.hpp>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>

#define IRPin 15
#define SWITCH 16
#define SEND 17

// Json saved file path in the esp32
String filePath = "/Devices/devices.json";

// Json Global Variables
int counter = 0;
JsonDocument doc;
JsonArray signals;

// Button globar varibals
bool switchButtonPrevState = true; // input pull up off by default
bool sendButtonPrevState = true; // input pull up off by default

// Buttons Debounce Variables
unsigned long switchClickTime = 0;
unsigned long sendClickTime = 0;
int DEBOUNCE_MS = 30;

// Global Selectors  
String place[] = {"Living Room", " My Room"};
String type[] = {"AC", "TV", "Fan"};
String AC_Key[] = {"Power UP", "Power OFF", "Plus", "Minus"};
String TV_Key[] = {"Power UP", "Power OFF", "OK"};
String Fan_Key[] = {"Power UP", "Power OFF", "Speed"};

int placeIndex = 0;
int typeIndex = 0; int ACIndex = 0;
int TVIndex = 0;
int FanIndex = 0;

int placeNumber = sizeof(place) / sizeof(place[0]);
int typeNumber = sizeof(type) / sizeof(type[0]);
int AC_KeyNumber = sizeof(AC_Key) / sizeof(AC_Key[0]);
int TV_KeyNumber = sizeof(TV_Key) / sizeof(TV_Key[0]);
int Fan_KeyNumber = sizeof(Fan_Key) / sizeof(Fan_Key[0]);

enum SELECTOR {
  PLACE,
  TYPE,
  KEY
};

enum DEVICE {
  AC,
  TV,
  FAN
};

SELECTOR selector = PLACE;
DEVICE device = AC;

// True if assigning a signal
bool newSignalAssing = false;

// JsonDocument Global Variable
JsonArray devices; 
JsonObject newEntry;

// Wifi Credintials
const char *ssid = "ESP32 Universal Remote";

bool loadJsonFromFlash() {
  File file = LittleFS.open(filePath, "r");

  if (!file) {
    Serial.println("Failed to open the file to read!");
    return false;
  }

  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("Failed to deserialize the Json :");
    Serial.println(error.c_str());
    return false;
  }

  return true;
}

bool saveJsonToFlash() {
  File file = LittleFS.open(filePath, "w");

  if (!file) {
    Serial.println("Failed to open file to write!");
    return false;
  }

  size_t writtenBytes = serializeJson(doc, file);
  file.close();

  if (writtenBytes == 0) {
    Serial.println("failed to serialize the Json!");
    return false;
  }

  return true;
}
void setup() {
  Serial.begin(115200);
  
  // Pin Modes
  pinMode(SEND, INPUT_PULLUP);
  pinMode(SWITCH, INPUT_PULLUP);
  
  // IrReceiver object 
  // ENABLE_LED_FEEDBACK uses the esp32 builtin LED
  // AS an indicator that a signal was picked
  IrReceiver.begin(IRPin, ENABLE_LED_FEEDBACK);

  // LittleFS filesystem mounting
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mounting failed!");
  }

  if (!LittleFS.exists("/Devices")) {
    LittleFS.mkdir("/Devices");
  }

  // Loading the JsonDocument from memory
  bool loaded = loadJsonFromFlash(); 

  if (!loaded) {
    Serial.println("Failed to load the Json file!");
  }

  if (loaded && doc["Devices"].is<JsonArray>()) {
    devices = doc["Devices"].as<JsonArray>();
  } else {
    devices = doc["Devices"].to<JsonArray>();
  }


  // Wifi Start 
  WiFi.softAP(ssid,NULL);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("IP Address: ");
  Serial.println(IP);

}

bool switchButtonClick() {
  bool state = digitalRead(SWITCH);
  if (state != switchButtonPrevState) {
    switchClickTime = millis();
    switchButtonPrevState = state;
  }

  if ((millis() - switchClickTime > DEBOUNCE_MS) && !state) {
      switchClickTime = millis();
      return true;
    }
  return false;
}

bool sendButtonClick() {
  bool state = digitalRead(SEND);
  if (state != sendButtonPrevState) {
    sendClickTime = millis();
    sendButtonPrevState = state;
  }

  if ((millis() - sendClickTime > DEBOUNCE_MS) && !state) {
      sendClickTime = millis();
      return true;
    }
  return false;
}
void clearScreen() {
  // Print empty lines as simulate clearing the screen
  for (int i = 0; i < 30; i++) {
    Serial.println();
  }
}
void updateDisplay() {
  clearScreen();

  Serial.println("=========================================");
  Serial.println("        IR SIGNAL CAPTURE MENU           ");
  Serial.println("=========================================");

  if (selector == PLACE) {
    Serial.print("[>] PLACE: "); Serial.println(place[placeIndex]);
  } else {
    // print the place selected and if not selected for some reseaon just print the highlited one
    Serial.print("    PLACE: "); Serial.println(newEntry["Place"] | place[placeIndex]); 
  }

if (selector == TYPE) {
    Serial.print("[>] TYPE: "); Serial.println(type[typeIndex]);
  } else {
    Serial.print("    TYPE: "); Serial.println(newEntry["Type"] | type[typeIndex]); 
  }

if (selector == KEY) {
    Serial.print("[>] KEY: "); 
    switch (device) {
      case (AC): Serial.println(AC_Key[ACIndex]); break;
      case (TV): Serial.println(TV_Key[TVIndex]); break;
      case (FAN): Serial.println(Fan_Key[FanIndex]); break;
    }
  } else {
    Serial.print("    KEY: ");
    if (newEntry["Key"]) {
      Serial.println(newEntry["Key"].as<String>());
    } else {
      Serial.println("----");
    }
  }

  Serial.println("=========================================");
  Serial.println(" [SWITCH]: Next Item  |  [SEND]: Confirm ");
  Serial.println("=========================================");
}

void waitForRelease() {
  while (digitalRead(SWITCH) == LOW || digitalRead(SEND) == LOW) {
    yield();
  }
  delay(30); // small delay time after release
}

bool signalAssign() {
if (!IrReceiver.decode()) { return false; }

  if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
    IrReceiver.resume();
    return false;
  }

  String firstCode = String(IrReceiver.decodedIRData.decodedRawData, HEX);
  IrReceiver.resume();

  //Waiting for second signal
  String secondCode = "";
  unsigned long sentTime = millis();
  while (millis() - sentTime < 150) {
    if (IrReceiver.decode()) {
        secondCode = String(IrReceiver.decodedIRData.decodedRawData, HEX);
        IrReceiver.resume();
        break;
    }
    IrReceiver.resume();
    yield();
  }

  newEntry["Code1"] = firstCode;
  if (secondCode != "") {
    newEntry["Code2"] = secondCode;
  } else {
    newEntry["Code2"] = "";
  }

  // Resuming
  IrReceiver.resume();
  return true;
}

void loop() {
  // Waiting for the switch key to be pressed first
  // to enter assgin signal mode
  if (!newSignalAssing) {
    Serial.println("Enter the SWITCH button key to assign new signal.");
    while (!digitalRead(SWITCH) == LOW) {
      yield(); // tells the esp32 that its not stuck do not reset
    }
    waitForRelease();
    newSignalAssing = true;
    updateDisplay();
    newEntry = devices.add<JsonObject>();
  }

  if (switchButtonClick()) {
    // Updating the selector indexes
    switch (selector) {
      case (PLACE):
        placeIndex = (placeIndex + 1) % placeNumber;
        break;

      case (TYPE):
        typeIndex = (typeIndex + 1) % typeNumber;
        break;
      
      case (KEY):
        switch (device) {
          case (AC):
            ACIndex = (ACIndex + 1) % AC_KeyNumber;
            break;

          case (TV):
            TVIndex = (TVIndex + 1) % TV_KeyNumber;
            break;

          case (FAN):
            FanIndex = (FanIndex + 1) % Fan_KeyNumber;
            break;
        }
    }
    updateDisplay();
    waitForRelease();
  }

  if (sendButtonClick()) {

    //  Saving the Choosen variable in the JSON  
    switch (selector) {
      case (PLACE):
        newEntry["Place"] = place[placeIndex]; 
        selector = TYPE;
        updateDisplay();
        break;

      case (TYPE):
        newEntry["Type"] = type[typeIndex];
        selector = KEY;
        if (type[typeIndex] == "AC")       device = AC;
        else if (type[typeIndex] == "TV")  device = TV;
        else if (type[typeIndex] == "Fan") device = FAN;
        updateDisplay();
        break;

      case (KEY):
        switch (device) {
          case (AC):
            newEntry["Key"] = AC_Key[ACIndex];
            break;
            
          case (TV):
            newEntry["Key"] = TV_Key[TVIndex];
            break;

          case (FAN):
            newEntry["Key"] = Fan_Key[FanIndex];
            break;
        }
        // Ask to send the signal and wait
        clearScreen();
        Serial.println("=========================================");
        Serial.println(" >>> WAITING FOR IR SIGNAL...           ");
        Serial.println("=========================================");
        Serial.println("Please direct the remote to the receiver");
        Serial.println("and press the desired button.");
        Serial.println("=========================================");

        while (!signalAssign()) {
          yield();
        } 
        // Printing the final Json
        clearScreen();
        Serial.println("=========================================");
        Serial.println(" >>> SIGNAL SAVED SUCCESSFULLY!         ");
        Serial.println("=========================================");
        serializeJsonPretty(doc, Serial);
        Serial.println("\n=========================================");
        
        // Reseting the choices
        selector = PLACE;
        placeIndex = 0;
        typeIndex = 0;
        ACIndex = 0;
        TVIndex = 0;
        FanIndex = 0;
        newSignalAssing = false;

        // Saving the Json in the memory
        if (!saveJsonToFlash()) {
          Serial.println("Failed to save the json in the memory!");
        } else {
          Serial.println("Saved the Json succesffully");
        }
        break;
    }
    waitForRelease();
  }
}

