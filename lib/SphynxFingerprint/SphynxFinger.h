/*
  SphynxFingerprint library for Sphynx: https://github.com/Cliyo/sphynx-esp32
  will control the fingerprint sensor (sfm v1.7 and hlk-101) for the access control system.
  uses uart2 for communication with the sensor.
  tested with esp32-wroom-32
  By: Snootic - 2024: https://github.com/snootic / @snootic_
*/

#ifndef SphynxFinger_h
#define SphynxFinger_h

#define TXD2 16
#define RXD2 17

#include <Adafruit_Fingerprint.h>

class SphynxFingerClass {
    public:
        SphynxFingerClass();
        void setupSensor();
        void readFinger(int &status);
        uint8_t enrollFinger();
        void enrollFingerLocal();
        bool verifyFinger();
        Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);
    private:
        void printHex(int num, int precision);
        void read(int &status);
        uint8_t convertImage();
        uint8_t createModel();
        void clearUART2();
};

extern SphynxFingerClass SphynxFinger;
#endif