#include <Arduino.h>
#include <IRremote.hpp>
#include <ArduinoJson.h>

#define IRPin 15

void setup() {
  Serial.begin(115200);
  delay(300);
  IrReceiver.begin(IRPin, ENABLE_LED_FEEDBACK);
  Serial.println("ready to receive data!");
}
//TODO: Save the Received IR signal
//TODO: Prompt the user to enter the name of this signal
//TODO: println the current database we have

void loop() {
  static JsonDocument doc;
  static JsonArray signals = doc["signals"].to<JsonArray>();

  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol != UNKNOWN && IrReceiver.decodedIRData.protocol != 0) {
      String firstCode = String(IrReceiver.decodedIRData.decodedRawData, HEX);
      String secondCode = "";
      IrReceiver.resume();

      unsigned long sentTime = millis();
      while (millis() - sentTime < 150) {
        if (IrReceiver.decode()) {
          if (IrReceiver.decodedIRData.protocol != UNKNOWN && IrReceiver.decodedIRData.protocol != 0) {
            secondCode = String(IrReceiver.decodedIRData.decodedRawData, HEX);
            IrReceiver.resume();
            break;
          } 
        }
        yield();
      }

      JsonObject newEntry = signals.add<JsonObject>();
      newEntry["site"] = "IRDevice";
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
    }
  }
}

