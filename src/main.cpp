#include <Arduino.h>
#include <IRremote.hpp>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>

#define IRPin 15
#define SendingPIN 16

// Json saved file path in the esp32
String filePath = "/Devices/devices.json";

// Global Selectors  
String TV_Key[] = {"Power", "Source", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight",
"Nine", "Mute", "Zero", "Channel_List", "Volume_UP", "Arrow_UP", "Channel_Next", "Arrow_Left", "OK", "Arrow_Right", "Volume_Down", "Arrow_Down",
"Channel_Prev", "Back", "Exit", "A", "B", "C", "D", "P_Back", "Resume", "Pause", "P_Forward"};
String AC_Key[] =  {"Preset1", "Preset2", "Preset3", "Preset4", "Preset5", "Preset6", "Preset7", "Preset8", "Preset9", "OFF"}; 
String Others_Key[] = {"Power", "Home", "Menu", "OK", "Up", "Down", "Left", "Right", "Volume_Up", "Volume_Down", "Mute", "Return", "Exit", "Settings", "Info"};
int AC_KeyNumber = sizeof(AC_Key) / sizeof(AC_Key[0]);
int TVKeysNumber = sizeof(TV_Key) / sizeof(TV_Key[0]);
int Others_KeyNumber = sizeof(Others_Key) / sizeof(Others_Key[0]);
// enum SELECTOR {
//   PLACE,
//   TYPE,
//   KEY
// };
//
// enum DEVICE {
//   AC,
//   TV,
//   FAN
// };
//
// SELECTOR selector = PLACE;
// DEVICE device = AC;

// Wifi name and passsword
char *ssid = "WE_7A8A8B";
char *password = "cb71f8ab";

// Web Server Object at port 80
WebServer server(80);

JsonDocument loadJsonFromFlash(String path) {
  File file = LittleFS.open(path, "r");
  JsonDocument doc;

  if (!file) {
    Serial.println("Failed to open the file to read!");
    return doc;
  }
  

  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("Failed to deserialize the Json :");
    Serial.println(error.c_str());
    return doc;
  }

  return doc;
}

bool saveJsonToFlash(JsonDocument &doc) {
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

void createDocument(String name, String type) {
  JsonDocument doc = loadJsonFromFlash(filePath);
  JsonArray devices;

  // Creating an array inside the json object variable
  if (doc["Devices"].is<JsonArray>()) {
    devices = doc["Devices"].as<JsonArray>();
  } else {
    devices = doc["Devices"].to<JsonArray>();
  }

  if (type == "TV") {
    // Adding the entries
    JsonObject newEntry = devices.add<JsonObject>();
    newEntry["Name"] = name;
    newEntry["Type"] = type;
    JsonArray buttons = newEntry["Buttons"].to<JsonArray>();

    for (int i = 0; i < TVKeysNumber; i++) {
      JsonObject buttonsObject = buttons.add<JsonObject>();
      buttonsObject["Name"] = TV_Key[i];
      buttonsObject["Assigned"] = false;
    } 
  } else if (type == "AC") {

    // Adding the entries
    JsonObject newEntry = devices.add<JsonObject>();
    newEntry["Name"] = name;
    newEntry["Type"] = type;
    JsonArray presets = newEntry["Presets"].to<JsonArray>();
    for (int i = 0; i < AC_KeyNumber; i++) {
      JsonObject buttonsObject = presets.add<JsonObject>();
      buttonsObject["Name"] = AC_Key[i];
      buttonsObject["Assigned"] = false;
    }
  } else {

    // Adding the entries
    JsonObject newEntry = devices.add<JsonObject>();
    newEntry["Name"] = name;
    newEntry["Type"] = type;
    JsonArray buttons = newEntry["Buttons"].to<JsonArray>();

    for (int i = 0; i < Others_KeyNumber; i++) {
      JsonObject buttonsObject = buttons.add<JsonObject>();
      buttonsObject["Name"] = Others_Key[i];
      buttonsObject["Assigned"] = false;
    } 
  }

  if (!saveJsonToFlash(doc)) {
    Serial.println("Failed to save json to Flash!");
  }
  serializeJsonPretty(doc, Serial);
}

bool signalAssign(String name, String type, JsonObject selected) {
  if (!IrReceiver.decode()) {
    return false;
  }

  if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
    IrReceiver.resume();
    return false;
  }

  uint16_t rawLen = IrReceiver.irparams.rawlen;

  if (rawLen < 40) {
    IrReceiver.resume();
    return false;
  }

  Serial.println("SignalFound! rawLen = " + String(rawLen));

  uint16_t pulseCount = rawLen - 1; // first entry is the initial gap, skip it
  uint16_t rawTimings[pulseCount];
  for (uint16_t i = 1; i < rawLen; i++) {
    rawTimings[i - 1] = IrReceiver.irparams.rawbuf[i] * MICROS_PER_TICK;
  }

  IrReceiver.resume();

  selected.remove("RawCode");
  JsonArray rawArray = selected["RawCode"].to<JsonArray>();
  for (uint16_t i = 0; i < pulseCount; i++) {
    rawArray.add(rawTimings[i]);
  }
  selected["Assigned"] = true;

  return true;
}

void sendSignal(JsonObject buttonObject) {
  JsonArray rawArray = buttonObject["RawCode"].as<JsonArray>();
  uint16_t len = rawArray.size();

  uint16_t rawData[len];
  for (uint16_t i = 0; i < len; i++) {
    rawData[i] = rawArray[i].as<uint16_t>();
  }

  Serial.println("Sending raw signal, " + String(len) + " pulses");
  IrSender.sendRaw(rawData, len, 38); // 38kHz - standard for ~99% of consumer IR remotes
}


void handle_getDevices() {
  
  JsonDocument doc = loadJsonFromFlash(filePath);

  String jsonBuffer;
  serializeJson(doc, jsonBuffer);

  server.send(200, "application/json", jsonBuffer);
}

void handle_getButtons() {
  String name = server.arg("Name");
  String type = server.arg("Type");
  JsonDocument doc = loadJsonFromFlash(filePath);
  JsonArray devices;
  if (doc["Devices"].is<JsonArray>()) {
    devices = doc["Devices"].as<JsonArray>();
  } else {
    devices = doc["Devices"].to<JsonArray>();
  }

  JsonObject selectedEntry;
  for (JsonObject entry : devices) {
    if (entry["Name"] == name && entry["Type"] == type) {
      selectedEntry = entry;
      break;
    }
  }

  if (selectedEntry.isNull()) {
    Serial.printf("No device found for Name=%s Type=%s\n", name.c_str(), type.c_str());
    server.send(404, "application/json", "{\"error\":\"device not found\"}");
    return;
  }

  if (type == "AC") {
    JsonArray presets = selectedEntry["Presets"].as<JsonArray>();
    String presetsJson;
    serializeJson(presets, presetsJson);
    server.send(200, "application/json", presetsJson);
  } else if (type == "TV" || type == "Others") {
    JsonArray buttons = selectedEntry["Buttons"].as<JsonArray>();
    String buttonsJson;
    serializeJson(buttons, buttonsJson);
    server.send(200, "application/json", buttonsJson);
  } else {
    server.send(400, "application/json", "{\"error\":\"unknown type\"}");
  }
}

void handle_sendSignal() {
  // Set the query parameters variables
  String name = server.arg("Name");
  String type = server.arg("Type");
  String buttonName = server.arg("buttonName");

  JsonDocument doc = loadJsonFromFlash(filePath);
  JsonArray devices;

  // Creating an array inside the json object variable
  if (doc["Devices"].is<JsonArray>()) {
    devices = doc["Devices"].as<JsonArray>();
  } else {
    devices = doc["Devices"].to<JsonArray>();
  }

  JsonObject selectedEntry;
  for (JsonObject entry : devices) {
    if (entry["Name"] == name && entry["Type"] == type) {
      selectedEntry = entry;
      break;
    }
  }

  JsonObject selected;
  if (server.hasArg("Preset")) {
    JsonArray presets = selectedEntry["Presets"].as<JsonArray>();
    for (JsonObject entry : presets) {
      if (entry["Name"] == server.arg("Preset")) {
        selected = entry;
        break;
      }
    }
  } else if (server.hasArg("buttonName")) {
    JsonArray buttons = selectedEntry["Buttons"].as<JsonArray>();
    for (JsonObject entry : buttons) {
      if (entry["Name"] == server.arg("buttonName")) {
        selected = entry;
        break;
      }
    }
  }
  
  sendSignal(selected);
  server.send(200);
}

void handle_learnSignal() {
  // Set the query parameters variables
  String name = server.arg("Name");
  String type = server.arg("Type");

  JsonDocument doc = loadJsonFromFlash(filePath);
  JsonArray devices;
  if (doc["Devices"].is<JsonArray>()) {
    devices = doc["Devices"].as<JsonArray>();
  } else {
    devices = doc["Devices"].to<JsonArray>();
  }

  JsonObject selectedEntry;
  for (JsonObject entry : devices) {
    if (entry["Name"] == name && entry["Type"] == type) {
      selectedEntry = entry;
      break;
    }
  }

  JsonObject selected;
  if (type == "TV" && server.hasArg("buttonName")) {
    String buttonName = server.arg("buttonName");
    JsonArray buttons = selectedEntry["Buttons"].as<JsonArray>();
    for (JsonObject entry : buttons) {
      if (entry["Name"] == buttonName) {
        selected = entry;
        break;
      }
    }
  } else if (type == "AC" && server.hasArg("Preset")) {
    String preset = server.arg("Preset");
    JsonArray presets = selectedEntry["Presets"].as<JsonArray>();
    for (JsonObject entry : presets) {
      if (entry["Name"] == preset) {
        selected = entry;
        break;
      }
    }

    if (preset != "OFF") {
      selected["Temp"] = server.arg("Temp");
      selected["Speed"] = server.arg("Speed");
      selected["Mode"] = server.arg("Mode");
      selected["HorSwing"] = server.arg("HorSwing");
      selected["VerSwing"] = server.arg("VerSwing");
    }
  } else {
    String buttonName = server.arg("buttonName");
    JsonArray buttons = selectedEntry["Buttons"].as<JsonArray>();
    for (JsonObject entry : buttons) {
      if (entry["Name"] == buttonName) {
        selected = entry;
        break;
      }
    }
  }

  while (!signalAssign(name, type, selected)) {
    yield();
  }

  saveJsonToFlash(doc);
  serializeJsonPretty(doc, Serial);

  server.send(200);
}

void handle_addDevice() {
  // Set the query parameters Variables
  String name = server.arg("Name");
  String type = server.arg("Type");

  createDocument(name, type);

  server.send(200);
}

void handle_deleteDevice() {
  // Get the query parameters variables
  String name = server.arg("Name");
  String type = server.arg("Type");

  JsonDocument doc = loadJsonFromFlash(filePath); 
  JsonArray devices; 

  // Creating an array inside the json object variable
  if (doc["Devices"].is<JsonArray>()) {
    devices = doc["Devices"].as<JsonArray>();
  } else {
    devices = doc["Devices"].to<JsonArray>();
  }

  int indexToRemove = 0;

  for (JsonObject entry : devices) {
    if (entry["Name"] == name && entry["Type"] == type) {
      devices.remove(indexToRemove);
      break;
    }
    indexToRemove++;
  }

  saveJsonToFlash(doc);
  serializeJsonPretty(doc, Serial);
  server.send(200);
}

void handle_addCustomButton() {

  // Set the query parameters variables
  String name = server.arg("Name");
  String type = server.arg("Type");

  JsonDocument doc = loadJsonFromFlash(filePath);
  JsonArray devices;

  // Creating an array inside the json object variable
  if (doc["Devices"].is<JsonArray>()) {
    devices = doc["Devices"].as<JsonArray>();
  } else {
    devices = doc["Devices"].to<JsonArray>();
  }

  JsonObject selectedEntry;
  for (JsonObject entry : devices) {
    if (entry["Name"] == name && entry["Type"] == type) {
      selectedEntry = entry;
      break;
    }
  }

  JsonArray buttons = selectedEntry["Buttons"].as<JsonArray>();
  JsonObject selected = buttons.add<JsonObject>();
  selected["Name"] = server.arg("buttonName");
  selected["customNumber"] = server.arg("whichCustom");
  selected["Assigned"] = true;

  saveJsonToFlash(doc);

  server.send(200);
}

void setup() {
  Serial.begin(115200);
  
  // IrReceiver object 
  // ENABLE_LED_FEEDBACK uses the esp32 builtin LED
  // AS an indicator that a signal was picked
  IrReceiver.begin(IRPin, ENABLE_LED_FEEDBACK);
  IrSender.begin(SendingPIN);

  // LittleFS filesystem mounting
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mounting failed!");
  }
  if (!LittleFS.exists("/Devices")) {
    LittleFS.mkdir("/Devices");
  }

  if (!LittleFS.exists("/Devices/devices.json")) {
  File f = LittleFS.open("/Devices/devices.json", "w");
    if (!f) {
      Serial.println("Failed to create devices.json!");
    } else {
      f.print("{}");
      f.close();
    }
}
  // Loading the JsonDocument from memory
  // bool loaded = loadJsonFromFlash(); 
  //
  // if (!loaded) {
  //   Serial.println("Failed to load the Json file!");
  // }

  // if (loaded && doc["Devices"].is<JsonArray>()) {
  //   devices = doc["Devices"].as<JsonArray>();
  // } else {
  //   devices = doc["Devices"].to<JsonArray>();
  // }
  
  // WiFi start
  WiFi.begin(ssid, password);
  Serial.println("Connecting to Wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Connected to wifi!");
  
  // Printing the IP address to the serial print
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Server start
  server.on("/getDevices", handle_getDevices);  
  server.on("/sendSignal", handle_sendSignal);  
  server.on("/learnSignal", handle_learnSignal);  
  server.on("/addDevice", handle_addDevice);  
  server.on("/deleteDevice", handle_deleteDevice);
  server.on("/getButtons", handle_getButtons);
  server.on("/addCustomButton", handle_addCustomButton);

  server.begin();
  Serial.println("Server Started!");

}

void loop() {
  server.handleClient();
}

