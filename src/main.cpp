#include <Arduino.h>
#include "SphynxWiFi.h"
#include "ESPAsyncWebServer.h"
#include <HTTPClient.h>
#include <Arduino_JSON.h>

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
};

Mode currentMode = MODE_CONTROL_DOOR;

void controlDoor(String message){
  if(message == "true"){
    digitalWrite(led, !digitalRead(led));
    delay(1000);    
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
    String message = (const char *)data;

    if (message.substring(0, 4) == "data") {
      ws.text(client->id(), "data");
    }
    else if (message.substring(0, 4) == "tags") {
      currentMode = MODE_REGISTER_TAG;
      Serial.println("Register Tag mode begin");
      digitalWrite(led, !digitalRead(led));
    }
  }
}

void apiRequest(String tag){
  HTTPClient http;

  IPAddress api;

  api = SphynxWiFi.getApiAddress();

  String apiUrl = "http://" + api.toString() + ":57128/accessRegisters";
  http.begin(apiUrl);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Access-Control-Allow-Credentials", "true");
  http.addHeader("Access-Control-Allow-Origin", "*");

  http.setConnectTimeout(10000);

  String json = "{\"mac\":\""+SphynxWiFi.getMac()+"\",\"tag\":\""+tag+"\"}";

  Serial.println(json);

  int httpResponseCode = http.POST(json);

  if(httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(payload);
  } 
  
  else {
    Serial.println("Error on HTTP request");
    Serial.println(http.errorToString(httpResponseCode).c_str());
    Serial.println(httpResponseCode);
  }

  http.end();
}

void receiveTag(){
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String id_cartao = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      id_cartao.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
      id_cartao.concat(String(rfid.uid.uidByte[i], HEX));
    }
    id_cartao.toUpperCase();

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

      apiRequest(id_cartao);

      digitalWrite(led, !digitalRead(led));
      delay(500);
      digitalWrite(led, !digitalRead(led));
    }


    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  
}

void sphynx(){
  Serial.println("Sphynx Begun");
  SPI.begin();
  rfid.PCD_Init();
  delay(2000);

  pinMode(acionador, OUTPUT);
  pinMode(led, OUTPUT);
  pinMode(button, INPUT);

  Serial.print("RC522 ");
  rfid.PCD_DumpVersionToSerial();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
}

void setup(){
  Serial.begin(115200);
  if (!SphynxWiFi.connect()) {
    SphynxWiFi.setupWiFi();
  }
  while (!SphynxWiFi.conectado()){
    delay(500);
    continue;
  }
  sphynx();
}

void loop(){
  // while (api[0] == 0){
  //   api = SphynxWiFi.getApiAddress();
  // }

  // Serial.print("RC522 ");
  // rfid.PCD_DumpVersionToSerial();

  receiveTag();
  
  delay(1000);
}
