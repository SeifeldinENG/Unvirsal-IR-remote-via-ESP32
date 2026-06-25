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

// Devices enum  
enum PLACE {
  LIVING_ROOM,
  MY_ROOM
};

enum TYPE {
  AC,
  TV,
  FAN
};

enum SELECTOR {
  IDLE,
  PLACE,
  TYPE
};

PLACE place = LIVING_ROOM;
TYPE type = AC;
SELECTOR selector = IDLE;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON, INPUT_PULLUP);
  devices = doc["Devices"].to<JsonArray>();
  IrReceiver.begin(IRPin, ENABLE_LED_FEEDBACK);
  Serial.println("ready to receive data!");

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

    case (PLACE): {
      switch (place) {
        case (LIVING_ROOM):
          Serial.println("Living room");
          place = (place + 1) % 2;
          break;
        case (MY_ROOM):
          Serial.println("My room");
          place = (place + 1) % 2;
          break;
      }
    } 

    case (TYPE): {
      switch (type) {
        case (AC):
          Serial.println("AC");
          type = (type + 1) % 3;
          break;
        case (TV):
          Serial.println("TV");
          type = (type + 1) % 3;
          break;
        case (FAN):
          Serial.println("Fan");
          type = (type + 1) % 3;
          break;
      }
    }
    
    case (IDLE):
    selector = PLACE;
    Serial.println("Specify the Location:");
    break;
  }
}

void loop() {
  if (switchButtonClick(digitalRead(SWITCH)) {
    updateDisplay();
    selector = ((selector + 1) % 2) + 1;
  }

  if (sendButtonClick(digitalRead(SEND))) {
    // CODE
  }

  if (!IrReceiver.decode()) { return; }

  // if (IrReceiver.decodedIRData.protocol == 0 || 
  //     IrReceiver.decodedIRData.protocol == UNKNOWN) {
  //     Serial.println("Unknown or zero signal detected");
  //     IrReceiver.resume();
  //     return;
  // }

  if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
    IrReceiver.resume();
    return;
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

  // Checking for dublicates
  // for (JsonObject entry : signals) {
  //   if (entry["Code1"].as<String>() == firstCode) {
  //     IrReceiver.resume();
  //     return;
  //   }
  // }

  JsonObject newEntry = signals.add<JsonObject>();
  newEntry["site"] = "IRDevice" + String(counter);
  newEntry["Code1"] = firstCode;
  if (secondCode != "") {
    newEntry["Code2"] = secondCode;
    Serial.println("Two signal codes detected!");
  } else {
    newEntry["Code2"] = "";
    Serial.println("One signal code detected");
  }
  serializeJsonPretty(doc, Serial);

  // Resuming
  IrReceiver.resume();
  counter++;
}

