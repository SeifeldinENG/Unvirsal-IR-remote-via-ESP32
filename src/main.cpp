#include <Arduino.h>
#include <IRremote.hpp>
#include <ArduinoJson.h>

#define IRPin 15
#define BUTTON 16

// Json Global Variables
int counter = 0;
JsonDocument doc;
JsonArray signals;

// Button globar varibals
bool buttonPrevState;

// Global Selectors  
String place = {"Living Room", " My Room"};
String type = {"AC", "TV", "Fan"};
String AC_Key = {"Power UP", "Power OFF", "Plus", "Minus"};
String TV_Key = {"Power UP", "Power OFF", "OK"};
String Fan_Key = {"Power UP", "Power OFF", "Speed"};

int placeIndex = 0;
int typeIndex = 0;
int ACIndex = 0;
int TVIndex = 0;
int FanIndex = 0;

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

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(SWITCH, INPUT_PULLUP);
  devices = doc["Devices"].to<JsonArray>();
  IrReceiver.begin(IRPin, ENABLE_LED_FEEDBACK);
}

bool switchButtonClick(bool switchButtonCurrentState) {
  if (!switchButtonCurrentState && switchButtonPrevState) {
    delay(50);
    if (digitalRead(SWITCH)) { 
      return true;
    }
  }
  switchButtonPrevState = switchButtonCurrentState;
  return false;
}

bool sendButtonClick(bool sendButtonCurrentState) {
  if (!sendButtonCurrentState && sendButtonPrevState) {
    delay(50);
    if (digitalRead(SEND)) {
      return true;
    }
  }
  sendButtonPrevState = sendButtonCurrentState;
  return false;
}

void updateDisplay() {
  switch (selector) {
    case (PLACE):
      Serial.println(place(placeIndex));
      break;

    case (TYPE):
      Serial.println(type(typeIndex));
      break;

    case (KEY):
      switch (device) {
        case (AC):
          Serial.prinln(AC_Key[ACIndex]);
          break;

        case (TV):
          Seiral.println(TV_key[TVIndex]);
          break;

        case (FAN):
          Serial.println(Fan_Key[FanIndex]);
          break;
        }
      break;
  } 
}

void loop() {
  // Waiting for the switch key to be pressed first
  // to enter assgin signal mode
  if (!newSignalAssing) {
    Serial.println("Enter the SWITCH button key to assign new signal.");
    while (!switchButtonClick(digitalRead(SWITCH))) {
      yield(); // tells the esp32 that its not stuck do not reset
    }
    newSignalAssing = true;
    updateDisplay();
    JsonObject newEntry = devices.add<JsonObject>();
  }

  if (switchButtonClick(digitalRead(SWITCH)) {
    // Updating the selector indexes
    switch (selector) {
      case (PLACE):
        placeIndex = (placeIndex + 1) % length(place);
        break;

      case (TYPE):
        typeIndex = (typeIndex + 1) % length(type);
        break;
      
      case (KEY):
        switch (device) {
          case (AC):
            ACIndex = (ACIndex + 1) % length(AC_Key);
            break;

          case (TV):
            TVIndex = (TVIndex + 1) % length(TV_Key);
            break;

          case (FAN):
            FanIndex = (FanIndex + 1) % length(Fan_Key);
            break;
        }
    }
    updateDisplay();
  }

  if (sendButtonClick(digitalRead(SEND))) {

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
        placeIndex = 0;
        typeIndex = 0;
        ACIndex = 0;
        TVIndex = 0;
        FanIndex = 0;
        newSignalAssing = false;
    }
  }
}

