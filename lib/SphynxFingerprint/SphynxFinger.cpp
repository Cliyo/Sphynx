/*
  SphynxFingerprint library for Sphynx: https://github.com/Cliyo/sphynx-esp32
  will control the fingerprint sensor (sfm v1.7 and hlk-101) for the access control system.
  uses uart2 for communication with the sensor.
  tested with esp32-wroom-32
  By: Snootic - 2024: https://github.com/snootic / @snootic_
*/

#include "SphynxFinger.h"

uint8_t status;
int length;

uint8_t fingerTemplate[512];

SphynxFingerClass::SphynxFingerClass(){}

void SphynxFingerClass::setupSensor() {
    Serial2.begin(115200, SERIAL_8N1, TXD2, RXD2);
    finger.begin(115200);

    if (finger.verifyPassword()) {
        finger.getParameters();
        sensorFound = true;
    } else {
        Serial.println("Did not find fingerprint sensor :(");
        for (int i = 0; i < 10; i++) {
            delay(1000);
            Serial2.begin(115200, SERIAL_8N1, TXD2, RXD2);
            finger.begin(115200);
            if (sensorFound) {
                finger.getParameters();
                sensorFound = true;
                break;
            }
        }
        Serial.println("No sensor found");
    }
    delay(500);
}

uint8_t SphynxFingerClass::createModel() {
    status = 2;
    Serial.println("Insira dedo");
    while (status == FINGERPRINT_NOFINGER) {
        status = finger.getImage();
    }
    if (status != FINGERPRINT_OK && status != FINGERPRINT_NOFINGER) {
        return status;
    }

    Serial.println("Convertendo imagem");
    status = finger.image2Tz(uint8_t(1));
    if (status != FINGERPRINT_OK) {
        return status;
    }

    Serial.println("Remova o dedo");
    delay(2000);

    while (status != FINGERPRINT_NOFINGER) {
        status = finger.getImage();
        delay(100);
    }

    delay(500);

    Serial.println("Insira o mesmo dedo novamente");
    while (status == FINGERPRINT_NOFINGER) {
        status = finger.getImage();
    }
    if (status != FINGERPRINT_OK && status != FINGERPRINT_NOFINGER) {
        return status;
    }

    Serial.println("Convertendo imagem");
    status = finger.image2Tz(uint8_t(2));
    if (status != FINGERPRINT_OK) {
        return status;
    }

    Serial.println("Criando modelo");
    status = finger.createModel();

    return status;
}

uint8_t* SphynxFingerClass::verifyFinger() {
    setupSensor();
    
    clearUART2();
    
    status = finger.getImage();
    if (status == FINGERPRINT_NOFINGER) {
        return nullptr;
    }
    else if (status != FINGERPRINT_OK) {
        return nullptr;
    }

    status = finger.image2Tz();
    if (status != FINGERPRINT_OK) {
        return nullptr;
    }

    status = finger.createModel();
    if (status != FINGERPRINT_OK) {
        return nullptr;
    }

    status = finger.getModel();
    if (status != FINGERPRINT_OK) {
        return nullptr;
    }

    uint8_t* fingerTemplate = generateTemplate();

    return fingerTemplate;
}

uint8_t SphynxFingerClass::verifyFinger(uint8_t id) {
    setupSensor();
    
    clearUART2();

    uint8_t p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
        return 0;
    } else if (p != FINGERPRINT_OK) {
        Serial.println("Erro ao capturar imagem");
        return 0;
    }

    // OK success!

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK) {
        Serial.println("Erro ao converter imagem");
        return 0;
    }

    // OK converted!
    p = finger.fingerSearch();
    if (p == FINGERPRINT_OK) {
        Serial.println("Found a print match!");
    } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
        Serial.println("Communication error");
        return 0;
    } else if (p == FINGERPRINT_NOTFOUND) {
        Serial.println("Did not find a match");
        return 0;
    } else {
        Serial.println("Unknown error");
        return 0;
    }

    // found a match!
    Serial.print("Found ID #"); Serial.print(finger.fingerID);
    Serial.print(" with confidence of "); Serial.println(finger.confidence);

    return finger.fingerID;
}

uint8_t* SphynxFingerClass::enrollFinger() {
    status = createModel();
    if (status != FINGERPRINT_OK) {
        return &status;
    }

    status = finger.storeModel(uint16_t(1));
    if (status != FINGERPRINT_OK) {
        return &status;
    }
        

    status = finger.loadModel(uint16_t(1));
    if (status != FINGERPRINT_OK) {
        return &status;
    } 

    finger.getModel();

    uint8_t* fingerTemplate = generateTemplate();

    delay(2000);

    clearUART2();

    finger.emptyDatabase();

    return fingerTemplate;
}

uint8_t* SphynxFingerClass::generateTemplate() {

    // if (finger.get_template_buffer(512, fingerTemplate) != FINGERPRINT_OK){
    //     Serial.println("Error: get_template_buffer");
    // };

    // finger.getParameters();

    // for (int k = 0; k < 4; k++) {
    //   for (int l = 0; l < 128; l++) {
    //     Serial.print(fingerTemplate[(k * 128) + l], HEX);
    //     Serial.print(",");
    //   }
    //   Serial.println("");
    // }

    //using the template from adafruit
    uint8_t bytesReceived[534];
    memset(bytesReceived, 0xff, 534);

    uint32_t starttime = millis();
    int i = 0;
    while (i < 534 && (millis() - starttime) < 20000) {
        if (Serial2.available()) {
        bytesReceived[i++] = Serial2.read();
        }
    }
    Serial.print(i); Serial.println(" bytes read.");
    Serial.println("Decoding packet...");

    uint8_t* fingerTemplate = new u_int8_t[512];
    memset(fingerTemplate, 0xff, 512);

    int uindx = 9, index = 0;
    memcpy(fingerTemplate + index, bytesReceived + uindx, 256);
    uindx += 256;
    uindx += 2;
    uindx += 9;
    index += 256;
    memcpy(fingerTemplate + index, bytesReceived + uindx, 256);

    for (int i = 0; i < 512; i++) {
        printHex(fingerTemplate[i], 2);
    }

    return fingerTemplate;
}

boolean SphynxFingerClass::enrollFingerLocal(uint16_t id) {
    status = createModel();
    if (status != FINGERPRINT_OK) {
        Serial.println("Erro ao criar modelo");
        return false;
    }

    Serial.println("Armazenando modelo no id: " + String(id));
    status = finger.storeModel(id);
    if (status != FINGERPRINT_OK) {
        return false;
    }

    delay(2000);

    clearUART2();

    return true;
}

void SphynxFingerClass::clearUART2() {
    while (Serial2.available() > 0) {
        Serial2.read();
    }
    Serial2.flush();
}

void SphynxFingerClass::printHex(int num, int precision) {
  char tmp[16];
  char format[128];

  sprintf(format, "%%.%dX", precision);

  sprintf(tmp, format, num);
  Serial.print(tmp);
}

SphynxFingerClass SphynxFinger;