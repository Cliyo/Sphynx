/*
  SphynxFingerprint library for Sphynx: https://github.com/Cliyo/sphynx-esp32
  will control the fingerprint sensor (sfm v1.7 and hlk-101) for the access control system.
  uses uart2 for communication with the sensor.
  tested with esp32-wroom-32
  By: Snootic - 2024: https://github.com/snootic / @snootic_
*/

#include "SphynxFinger.h"

int status;
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

void SphynxFingerClass::readFinger(int &status) {
    if (status == -1){
        read(status);
    }
    else {
        while (status != FINGERPRINT_NOFINGER) {
            status = finger.getImage();
            Serial.println("tira o dedo burrao");
        }
        read(status);
    }

}

void SphynxFingerClass::read(int &status) {
    while (status != FINGERPRINT_OK) {
        status = finger.getImage();
        switch (status) {
            case FINGERPRINT_OK:
                Serial.println("Imagem capturada");
                break;
            case FINGERPRINT_NOFINGER:
                break;
            case FINGERPRINT_PACKETRECIEVEERR:
                Serial.println("Erro de comunicação");
                break;
            case FINGERPRINT_IMAGEFAIL:
                Serial.println("Erro ao capturar imagem");
                break;
            default:
                Serial.println("Erro desconhecido");
                break;
        }
    }
}

uint8_t SphynxFingerClass::convertImage() {
    status = finger.image2Tz(1);
    switch (status) {
        case FINGERPRINT_OK:
            Serial.println("Imagem convertida");
            return status;
        case FINGERPRINT_IMAGEMESS:
            Serial.println("Imagem muito desordenada");
            return status;
        case FINGERPRINT_PACKETRECIEVEERR:
            Serial.println("Erro de comunicação");
            return status;
        case FINGERPRINT_FEATUREFAIL:
            Serial.println("Não foi possível encontrar características da impressão digital");
            return status;
        case FINGERPRINT_INVALIDIMAGE:
            Serial.println("Não foi possível encontrar características da impressão digital");
            return status;
        default:
            Serial.println("Erro desconhecido");
            return status;
    }
}

uint8_t SphynxFingerClass::createModel() {
    status = finger.createModel();
    switch (status) {
        case FINGERPRINT_OK:
            Serial.println("Impressões digitais correspondentes!");
            return status;
        case FINGERPRINT_PACKETRECIEVEERR:
            Serial.println("Erro de comunicação");
            return status;
        case FINGERPRINT_ENROLLMISMATCH:
            Serial.println("As impressões digitais não correspondem");
            return status;
        default:
            Serial.println("Erro desconhecido");
            return status;
    }
}

uint8_t SphynxFingerClass::enrollFinger() {
    status = -1;
    readFinger(status);
    
    delay(100);

    Serial.println("Convertendo imagem");
    convertImage();

    delay(2000);

    status = 0;
    readFinger(status);

    delay(100);
    
    Serial.println("Convertendo imagem");
    convertImage();

    delay(100);
    
    Serial.println("Criando modelo");
    createModel();

    finger.storeModel(1);

    finger.loadModel(1);

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