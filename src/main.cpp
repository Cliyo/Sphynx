#include "OfflineDB.h"
#include "SphynxWiFi.h"
#include "SphynxFinger.h"
#include "ESPAsyncWebServer.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <stdio.h>
#include <string.h>

#include <Wire.h>
#include <SPI.h>

//Although the RC522 can read NFC tags
//It cannot identify or decrypt dynamic uid
//So we'll need PN532 for that

//Comment out to use RC522
#define USE_PN532

#define SS_PIN 5
#define RST_PIN 4
#define MISO_PIN  19 
#define MOSI_PIN  23  
#define SCK_PIN   18

#ifdef USE_PN532
  #include <Adafruit_PN532.h>
  Adafruit_PN532 nfc(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
#else
  #include <MFRC522.h>
  MFRC522 rfid(SS_PIN, RST_PIN);
#endif


#define button 21

#define led 2

int acionador = 15;

String message;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool rfSensorFound = false;

enum Mode {
  MODE_CONTROL_DOOR,
  MODE_REGISTER_TAG,
  MODE_ENROLL_FINGERPRINT,
};

Mode currentMode = MODE_CONTROL_DOOR;

void controlDoor(bool message){
  if(message == true){
    digitalWrite(led, !digitalRead(led));
    digitalWrite(acionador, HIGH);
    delay(5000);
    digitalWrite(acionador, LOW);
    digitalWrite(led, !digitalRead(led));
  }
  else if(message == false){
    digitalWrite(led, !digitalRead(led));
    delay(500);
    digitalWrite(led, !digitalRead(led));  
    delay(500);
    digitalWrite(led, !digitalRead(led));  
    delay(500);
    digitalWrite(led, !digitalRead(led));  
  }
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.println("Websocket client connection received");
  }      
  else if(type == WS_EVT_DISCONNECT){
    Serial.println("Client disconnected");
  }
  else if(type == WS_EVT_DATA){
    message = (const char *)data;

    if (message.substring(0, 4) == "data") {
      ws.text(client->id(), "data");
    }
    else if (message.substring(0, 4) == "tags") {
      if (!rfSensorFound) return;

      currentMode = MODE_REGISTER_TAG;
      Serial.println("Register Tag mode begin");
      digitalWrite(led, !digitalRead(led));
    }
    else if (message.substring(0, 6) == "finger") {
      if (!SphynxFinger.sensorFound) return;

      if (currentMode == MODE_ENROLL_FINGERPRINT){
        client->close();
        return;
      }
      currentMode = MODE_ENROLL_FINGERPRINT;
      Serial.println("Enroll Fingerprint mode begin");
      digitalWrite(led, !digitalRead(led));
    }
  }
}

void apiRequest(String json, String method, String subMethod){
  IPAddress api;
  HTTPClient http;

  api = SphynxWiFi.getApiAddress();

  String apiUrl = "http://" + api.toString() + ":57128/"+ method + "/" + subMethod;

  http.begin(apiUrl);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Access-Control-Allow-Credentials", "true");
  http.addHeader("Access-Control-Allow-Origin", "*");

  http.setConnectTimeout(10000);

  int httpResponseCode = http.POST(json);

  if(httpResponseCode > 0) {
    String payload = http.getString();
    if (httpResponseCode == 200 && method == "accessRegisters") {
      controlDoor(true);
    } else if (httpResponseCode !=200 && method != "accessRegisters") {
      controlDoor(false);
    }
  } 
  
  Serial.println(httpResponseCode);

  http.end();
}

void apiRequestWithTag(String tag){
  String json = "{\"mac\":\""+SphynxWiFi.getMac()+"\",\"tag\":\""+tag+"\"}";

  apiRequest(json, "accessRegisters" ,"tag");
}

void apiRequestWithFingerTemplate(uint8_t* fingerTemplate){
  JsonDocument doc;

  doc["mac"] = SphynxWiFi.getMac();

  doc["fingerprint"].to<JsonArray>();
    
  for (int k = 0; k < 4; k++) {
      for (int l = 0; l < 128; l++) {
          doc["fingerprint"].add(fingerTemplate[(k * 128) + l]);
      }
  }

  String json;
  serializeJson(doc, json);

  apiRequest(json, "accessRegisters","fingerprint");
}

void apiRequestWithFingerTemplate(uint16_t fingerID){
  JsonDocument doc;

  doc["mac"] = SphynxWiFi.getMac();

  doc["fingerprint"] = fingerID;

  String json;
  serializeJson(doc, json);

  apiRequest(json, "accessRegisters", "fingerprint");
}

void readFingerprint() {
  uint16_t match = 0;
  switch (currentMode)
  {
    case MODE_CONTROL_DOOR:
      match = SphynxFinger.verifyFinger(uint8_t(1));

      if (match != 0) {
        Serial.println("Fingerprint match");
        Serial.println(match);
        digitalWrite(led, !digitalRead(led));
        delay(500);
        digitalWrite(led, !digitalRead(led));
        apiRequestWithFingerTemplate(match);
      }
      break;
    case MODE_ENROLL_FINGERPRINT:
      Serial.println(message);

      if (message.substring(6, 8) == "id") {
        boolean enrolled = false;
        uint16_t fingerID = message.substring(8,9).toInt();
        enrolled = SphynxFinger.enrollFingerLocal(fingerID);
        digitalWrite(led, !digitalRead(led));
        ws.textAll(enrolled ? "true" : "false");
        currentMode = MODE_CONTROL_DOOR;

      } else {
        uint8_t* fingerTemplate = nullptr;
        fingerTemplate = SphynxFinger.enrollFinger();
        
        if (fingerTemplate != nullptr) {
          uint8_t* fingerTemplate = nullptr;
          fingerTemplate = SphynxFinger.enrollFinger();
          String fingerTemplateStr = String((char*)fingerTemplate);
          Serial.println(fingerTemplateStr);
          ws.textAll(fingerTemplateStr);
          digitalWrite(led, !digitalRead(led));
          currentMode = MODE_CONTROL_DOOR;
        }
      }
    default:
      break;
  }
}

void handleTag(String id_cartao) {
    if (currentMode == MODE_REGISTER_TAG) {
      ws.textAll(id_cartao);
      Serial.println("Tag registering completed");
      currentMode = MODE_CONTROL_DOOR;
      Serial.println("Returning to control door mode");
      digitalWrite(led, !digitalRead(led));
    }
    else if (currentMode == MODE_CONTROL_DOOR) {
      digitalWrite(led, !digitalRead(led));
      delay(500);
      digitalWrite(led, !digitalRead(led));

      if (SphynxWiFi.conectado() && SphynxWiFi.getApiAddress() != IPAddress(0, 0, 0, 0)) {
        apiRequestWithTag(id_cartao);
        return;
      }

      if (OfflineDB.isTagAuthorized(id_cartao)) {
        controlDoor(true);
      } else {
        controlDoor(false);
      }
    }
}

#ifndef USE_PN532
void receiveRC522Tag(){
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String id_cartao = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      id_cartao.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
      id_cartao.concat(String(rfid.uid.uidByte[i], HEX));
    }
    id_cartao.toUpperCase();

    id_cartao = id_cartao.substring(1);

    handleTag(id_cartao);

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  
}
#endif

#ifdef USE_PN532
void receivePN532Tag() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50);

  if (success) {
    String id_cartao = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      id_cartao.concat(String(uid[i] < 0x10 ? " 0" : " "));
      id_cartao.concat(String(uid[i], HEX));
    }
    id_cartao.toUpperCase();
    id_cartao = id_cartao.substring(1); 

    handleTag(id_cartao);

    delay(500); 
  }
}
#endif

void receiveTag() {
  if (!rfSensorFound) return;

  #ifdef USE_PN532
    receivePN532Tag();
  #else
    receiveRC522Tag();
  #endif
}

void reverseFinder() {
  String json = WiFi.localIP().toString() +","+ WiFi.macAddress();
  
  apiRequest(json, "deviceFinder", "push");

}

void setupRFSensor() {
  #ifdef USE_PN532
    nfc.begin();
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
      Serial.println("PN532 not found");
      rfSensorFound = false;
    } else {
      rfSensorFound = true;
      Serial.print("PN532 ");
      Serial.println((versiondata>>24) & 0xFF, HEX);
      nfc.SAMConfig(); 
    }
  #else
    SPI.begin();
    rfid.PCD_Init();
    byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
    
    if (v == 0x00 || v == 0xFF) {
        Serial.println("RC522 not found");
        rfSensorFound = false;
    } else {
        Serial.print("RC522 ");
        Serial.println(v, HEX);
        rfSensorFound = true;
    }
  #endif
}

void setup(){
  Serial.begin(57600);

  pinMode(led, OUTPUT);

  OfflineDB.begin();

  SphynxFinger.setupSensor();

  if (!SphynxWiFi.connect()) {
    SphynxWiFi.setupWiFi();
  }

  setupRFSensor();
  delay(2000);

  pinMode(acionador, OUTPUT);
  pinMode(button, INPUT);

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
}

void loop(){
  if (!SphynxWiFi.conectado()) {
    static unsigned short lastBlinkTime = 0;
    if (millis() - lastBlinkTime >= 500) {
      lastBlinkTime = millis();
      digitalWrite(led, !digitalRead(led));
    }
  }

  static unsigned short lastReverseFinderTime = 0;
  if (millis() - lastReverseFinderTime >= 5000) {
    lastReverseFinderTime = millis();
    reverseFinder();
  }

  if (SphynxFinger.sensorFound) {
    readFingerprint();
  }

  receiveTag();
  
  delay(10);
}
