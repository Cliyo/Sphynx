#include "SphynxWiFi.h"
#include "SphynxFinger.h"
#include "ESPAsyncWebServer.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <stdio.h>
#include <string.h>

#include <MFRC522.h>
#include <SPI.h>

#define SS_PIN 5
#define RST_PIN 4
#define MISO_PIN  19 
#define MOSI_PIN  23  
#define SCK_PIN   18

#define button 21

#define led 2

MFRC522 rfid(SS_PIN, RST_PIN);

int acionador = 15;

String message;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

enum Mode {
  MODE_CONTROL_DOOR,
  MODE_REGISTER_TAG,
  MODE_ENROLL_FINGERPRINT,
};

Mode currentMode = MODE_CONTROL_DOOR;

void controlDoor(String message){
  if(message == "true"){
    digitalWrite(led, !digitalRead(led));
    digitalWrite(acionador, HIGH);
    delay(5000);
    digitalWrite(acionador, LOW);
    digitalWrite(led, !digitalRead(led));
  }
  else if(message == "false"){
    digitalWrite(led, !digitalRead(led));
    delay(500);
    digitalWrite(led, !digitalRead(led));  
    delay(500);
    digitalWrite(led, !digitalRead(led));  
    delay(500);
    digitalWrite(led, !digitalRead(led));  
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    message = (char*)data;
    controlDoor(message);
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
      currentMode = MODE_REGISTER_TAG;
      Serial.println("Register Tag mode begin");
      digitalWrite(led, !digitalRead(led));
    }
    else if (message.substring(0, 6) == "finger") {
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

  Serial.println(api.toString());

  String apiUrl = "http://" + api.toString() + ":57128/"+ method + "/" + subMethod;

  Serial.println(apiUrl);
  http.begin(apiUrl);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Access-Control-Allow-Credentials", "true");
  http.addHeader("Access-Control-Allow-Origin", "*");

  http.setConnectTimeout(10000);

  Serial.println(json);

  int httpResponseCode = http.POST(json);

  if(httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(payload);
    if (httpResponseCode == 200 && method == "accessRegisters") {
      controlDoor("true");
    } else if (httpResponseCode !=200 && method != "accessRegisters") {
      controlDoor("false");
    }
  } 
  
  else {
    Serial.println(http.errorToString(httpResponseCode).c_str());
    Serial.println(httpResponseCode);
  }

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

void receiveTag(){
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String id_cartao = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      id_cartao.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
      id_cartao.concat(String(rfid.uid.uidByte[i], HEX));
    }
    id_cartao.toUpperCase();

    Serial.println("Tag readed: " + id_cartao);

    id_cartao = id_cartao.substring(1);

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

      apiRequestWithTag(id_cartao);
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  
}

void reverseFinder() {
  String json = WiFi.localIP().toString() +","+ WiFi.macAddress();
  
  apiRequest(json, "deviceFinder", "push");

}

void sphynx(){
  Serial.println("Sphynx Begun");
  SPI.begin();
  rfid.PCD_Init();
  delay(2000);

  pinMode(acionador, OUTPUT);
  pinMode(button, INPUT);

  Serial.print("RC522 ");
  rfid.PCD_DumpVersionToSerial();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
}

void setup(){
  Serial.begin(57600);

  pinMode(led, OUTPUT);

  if (!SphynxWiFi.connect()) {
    SphynxWiFi.setupWiFi();
  }
  while (!SphynxWiFi.conectado()){
    digitalWrite(led, !digitalRead(led));
    delay(500);
    continue;
  }
  sphynx();
}

void loop(){
  static unsigned long lastReverseFinderTime = 0;
  if (millis() - lastReverseFinderTime >= 5000) {
    lastReverseFinderTime = millis();
    reverseFinder();
  }

  if (SphynxFinger.sensorFound) {
    readFingerprint();
  }

  receiveTag();
  
  delay(1000);
}
