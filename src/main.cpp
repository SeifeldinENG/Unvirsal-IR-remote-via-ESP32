#include <Arduino.h>
#include <IRremote.hpp>
#include <ArduinoJson.h>

#define IRPin 15

// Json Global Variables
int counter = 0;
JsonDocument doc;
JsonArray signals;

// Structure of devices data
struct AC {
  String name, powerUP, powerOFF;
};

struct TV {
  String name, powerUP, powerOFF;
};

struct Fan {
  String name, powerUP, powerOFF;
};

void setup() {
  Serial.begin(115200);
  signals = doc["Signals"].to<JsonArray>();
  IrReceiver.begin(IRPin, ENABLE_LED_FEEDBACK);
  Serial.println("ready to receive data!");
}

void loop() {
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

