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

bool sensorFound = false;

SphynxFingerClass::SphynxFingerClass(){}

void SphynxFingerClass::setupSensor() {
    Serial2.begin(57600, SERIAL_8N1, TXD2, RXD2);

    finger.begin(57600);
    if (finger.verifyPassword()) {
        Serial.println("Sensor encontrado");
        sensorFound = true;
    } else {
        Serial.println("Sensor não encontrado");
        for (int i = 0; i < 10; i++) {
            delay(1000);
            setupSensor();
            if (sensorFound) {
                break;
            }
        }
    }
}

uint8_t SphynxFingerClass::createModel() {
    Serial.println("Lendo impressão digital pela primeira vez...");
    status = 2;
    while (status == FINGERPRINT_NOFINGER) {
        status = finger.getImage();
    }
    if (status != FINGERPRINT_OK && status != FINGERPRINT_NOFINGER) {
        Serial.println("Falha ao ler a impressão digital");
        return status;
    }

    Serial.println("Convertendo primeira imagem...");
    status = finger.image2Tz(uint8_t(1));
    if (status != FINGERPRINT_OK) {
        Serial.println("Falha ao converter a primeira imagem");
        return status;
    }
    Serial.println("Remova o dedo");

    delay(2000);

    Serial.println("Lendo impressão digital pela segunda vez...");
    while (status != FINGERPRINT_NOFINGER) {
        status = finger.getImage();
        delay(100);
    }

    delay(500);

    while (status == FINGERPRINT_NOFINGER) {
        status = finger.getImage();
    }
    if (status != FINGERPRINT_OK && status != FINGERPRINT_NOFINGER) {
        Serial.println("Falha ao ler a impressão digital");
        return status;
    }

    Serial.println("Convertendo segunda imagem...");
    status = finger.image2Tz(uint8_t(2));
    if (status != FINGERPRINT_OK) {
        Serial.println("Falha ao converter a segunda imagem");
        return status;
    }

    Serial.println("Comparando as imagens...");
    status = finger.createModel();
    if (status == FINGERPRINT_OK) {
        Serial.println("Modelo criado com sucesso");
    } else {
        Serial.println("Falha ao criar o modelo");
        return status;
    }

    return status;
}

uint8_t SphynxFingerClass::enrollFinger() {
    status = createModel();
    if (status != FINGERPRINT_OK) {
        Serial.println("Falha ao criar o modelo");
        return status;
    }

    status = finger.storeModel(uint16_t(1));
    if (status == FINGERPRINT_OK) {
        Serial.println("Modelo armazenado com sucesso");
    } else {
        Serial.println("Falha ao armazenar o modelo");
    }

    status = finger.loadModel(uint16_t(1));
    if (status == FINGERPRINT_OK) {
        Serial.println("Modelo carregado com sucesso");
    } else {
        Serial.println("Falha ao carregar o modelo");
        return status;
    }

    finger.getModel();

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

    uint8_t fingerTemplate[512];
    memset(fingerTemplate, 0xff, 512);

    int uindx = 9, index = 0;
    memcpy(fingerTemplate + index, bytesReceived + uindx, 256);
    uindx += 256;
    uindx += 2;
    uindx += 9;
    index += 256;
    memcpy(fingerTemplate + index, bytesReceived + uindx, 256);

    for (int i = 0; i < 512; ++i) {
        printHex(fingerTemplate[i], 2);
    }

    delay(2000);

    clearUART2();

    // finger.emptyDatabase();

    return status;
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