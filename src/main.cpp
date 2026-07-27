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
// String place[] = {"Living Room", " My Room"};
// String type[] = {"AC", "TV", "Fan"};
// String AC_Key[] = {"Power UP", "Power OFF", "Plus", "Minus"};
String TV_Key[] = {"Power", "Source", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight",
"Nine", "Mute", "Zero", "Channel_List", "Volume_UP", "Arrow_UP", "Channel_Next", "Arrow_Left", "OK", "Arrow_Right", "Volume_Down", "Arrow_Down",
"Channel_Prev", "Back", "Exit", "A", "B", "C", "D", "P_Back", "Resume", "Pause", "P_Forward"};
// String Fan_Key[] = {"Power UP", "Power OFF", "Speed"};

// int placeIndex = 0;
// int typeIndex = 0; int ACIndex = 0;
// int TVIndex = 0;
// int FanIndex = 0;
//
// int placeNumber = sizeof(place) / sizeof(place[0]);
// int typeNumber = sizeof(type) / sizeof(type[0]);
// int AC_KeyNumber = sizeof(AC_Key) / sizeof(AC_Key[0]);
int TVKeysNumber = sizeof(TV_Key) / sizeof(TV_Key[0]);
// int Fan_KeyNumber = sizeof(Fan_Key) / sizeof(Fan_Key[0]);
//
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
      buttonsObject["Code"] = "";
    } 
  } else if (type == "AC") {

    // Adding the entries
    JsonObject newEntry = devices.add<JsonObject>();
    newEntry["Name"] = name;
    newEntry["Type"] = type;
    JsonArray buttons = newEntry["Buttons"].to<JsonArray>();
    JsonObject buttonsObject = buttons.add<JsonObject>();
    buttonsObject["Name"] = "";
    buttonsObject["Assigned"] = false;
    buttonsObject["Code"] = "";
  } else {

    // Adding the entries
    JsonObject newEntry = devices.add<JsonObject>();
    newEntry["Name"] = name;
    newEntry["Type"] = type;
    JsonArray buttons = newEntry["Buttons"].to<JsonArray>();
    JsonObject buttonsObject = buttons.add<JsonObject>();
    buttonsObject["Name"] = "";
    buttonsObject["Assigned"] = false;
    buttonsObject["Code"] = "";
  }

  if (!saveJsonToFlash(doc)) {
    Serial.println("Failed to save json to Flash!");
  }
  serializeJsonPretty(doc, Serial);
}

bool signalAssign(String name, String type, String buttonName) {
  if (!IrReceiver.decode()) {
    return false;
  }

  if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
    IrReceiver.resume();
    return false;
  }

  // التعديل هنا: استخدام IrReceiver.irparams بدلاً من decodedIRData.rawDataPtr
  uint16_t rawLen = IrReceiver.irparams.rawlen;

  // Reject clearly too-short captures (noise)
  if (rawLen < 10) {
    IrReceiver.resume();
    return false;
  }

  Serial.println("SignalFound! rawLen = " + String(rawLen));

  // Copy raw timings BEFORE calling resume() - resume() reuses the buffer
  uint16_t pulseCount = rawLen - 1; // first entry is the initial gap, skip it
  uint16_t rawTimings[pulseCount];
  for (uint16_t i = 1; i < rawLen; i++) {
    // التعديل هنا أيضاً
    rawTimings[i - 1] = IrReceiver.irparams.rawbuf[i] * MICROS_PER_TICK;
  }

  IrReceiver.resume();

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

  JsonObject selectedButton;
  JsonArray buttons = selectedEntry["Buttons"].as<JsonArray>();
  for (JsonObject entry : buttons) {
    if (entry["Name"] == buttonName) {
      selectedButton = entry;
      break;
    }
  }

  // Overwrite any previous raw code for this button
  selectedButton.remove("RawCode");
  JsonArray rawArray = selectedButton["RawCode"].to<JsonArray>();
  for (uint16_t i = 0; i < pulseCount; i++) {
    rawArray.add(rawTimings[i]);
  }
  selectedButton["Assigned"] = true;

  saveJsonToFlash(doc);
  serializeJsonPretty(doc, Serial);
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

  JsonArray buttons = selectedEntry["Buttons"].as<JsonArray>();
  String buttonsJson;
  serializeJson(buttons, buttonsJson);
  server.send(200, "application/json", buttonsJson);
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

  JsonObject selectedButton;
  JsonArray buttons = selectedEntry["Buttons"].as<JsonArray>();
  for (JsonObject entry : buttons) {
    if (entry["Name"] == buttonName) {
      selectedButton = entry;
      break;
    }
  }
  
  sendSignal(selectedButton);
  server.send(200);
}

void handle_learnSignal() {
  // Set the query parameters variables
  String name = server.arg("Name");
  String type = server.arg("Type");
  String buttonName = server.arg("buttonName");

  // Serial.println("Name: " + name);
  // Serial.println("Type: " + type);
  // Serial.println("button Name: " + buttonName);
  
  while (!signalAssign(name, type, buttonName)) {
    yield();
  }

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

  server.begin();
  Serial.println("Server Started!");

}

void loop() {
  server.handleClient();
}

