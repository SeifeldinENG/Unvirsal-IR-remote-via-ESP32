#include <Arduino.h>
#include <IRremote.hpp>
#include <ArduinoJson.h>

#define IRPin 15
#define SWITCH 16
#define SEND 17

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
JsonArray devices = doc["Devices"].to<JsonArray>();
JsonObject newEntry;

void setup() {
  Serial.begin(115200);
  pinMode(SEND, INPUT_PULLUP);
  pinMode(SWITCH, INPUT_PULLUP);
  IrReceiver.begin(IRPin, ENABLE_LED_FEEDBACK);
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

void updateDisplay() {
  switch (selector) {
    case (PLACE):
      Serial.println(place[placeIndex]);
      break;

    case (TYPE):
      Serial.println(type[typeIndex]);
      break;

    case (KEY):
      switch (device) {
        case (AC):
          Serial.println(AC_Key[ACIndex]);
          break;

        case (TV):
          Serial.println(TV_Key[TVIndex]);
          break;

        case (FAN):
          Serial.println(Fan_Key[FanIndex]);
          break;
        }
      break;
  } 
}

void waitForRelease() {
  while (digitalRead(SWITCH) == LOW || digitalRead(SEND) == LOW) {
    yield();
  }
  delay(30); // small delay time after release
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
        // Printing the final Json
        serializeJsonPretty(doc, Serial);
        Serial.println(" ");

        // Reseting the choices
        selector = PLACE;
        placeIndex = 0;
        typeIndex = 0;
        ACIndex = 0;
        TVIndex = 0;
        FanIndex = 0;
        newSignalAssing = false;
    }
    waitForRelease();
  }
}

