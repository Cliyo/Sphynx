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

bool cadastrarTag = false;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

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
    ws.textAll("data");
  }      
   
  else if(type == WS_EVT_DISCONNECT){
    Serial.println("Client disconnected");
  }

  else if(type == WS_EVT_DATA){
    handleWebSocketMessage(arg, data, len);
  }
}

void apiRequest(){
  HTTPClient http;

  Serial.println("http://sphynx-api.local:57128/accessRegisters");

  http.begin("http://sphynx-api.local:57128/accessRegisters");

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Access-Control-Allow-Credentials", "true");
  http.addHeader("Access-Control-Allow-Origin", "*");

  String json = "{\"mac\":\""+SphynxWiFi.getMac()+"\",\"tag\":\"dsada\"}";

  Serial.println(SphynxWiFi.getMac());

  int httpResponseCode = http.POST(json);
  Serial.println(json);

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

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "access-control-allow-credentials, access-control-allow-origin, content-type");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Credentials", "true");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET,POST");
  server.begin();

  server.on("/conectar", HTTP_GET, [](AsyncWebServerRequest * request) {
  request->send(200, "text/plain", "Clyio - Sphynx");
  });

  server.on("/tag", HTTP_GET, [](AsyncWebServerRequest * request) {
    cadastrarTag = true;

    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial() && cadastrarTag == true){
      String id_cartao = "";
      byte i;
            
      for (byte i = 0; i < rfid.uid.size; i++){
        id_cartao.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
        id_cartao.concat(String(rfid.uid.uidByte[i], HEX));
      }
        
      id_cartao.toUpperCase();

      cadastrarTag=false;
      request->send(200, "text/plain", id_cartao.substring(1));
    }
  });
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
  
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial() && cadastrarTag == false){
    Serial.println("Coloque o cartão no Leitor.");
    
    String id_cartao = "";
    byte i;
          
    for (byte i = 0; i < rfid.uid.size; i++){
      Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(rfid.uid.uidByte[i], HEX);
      id_cartao.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
      id_cartao.concat(String(rfid.uid.uidByte[i], HEX));
    }
      
    id_cartao.toUpperCase();
    Serial.println(id_cartao.substring(1));

    apiRequest();
  }
  
  delay(2000);
}
