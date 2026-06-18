#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include "../system_state.h"

#define TRIG_PIN  26
#define ECHO_PIN  27
#define SS_PIN    21
#define RST_PIN   22

#define DETECTION_DISTANCE_CM   4
#define RELEASE_DISTANCE_CM     DETECTION_DISTANCE_CM
#define DETECTION_READINGS      2
#define RELEASE_READINGS        2
#define INVALID_READINGS_LIMIT  5
#define ULTRASONIC_TIMEOUT_US   12000UL
#define ULTRASONIC_SAMPLE_COUNT 3
#define ULTRASONIC_SAMPLE_GAP_MS 20

const float MIN_VALID_DISTANCE_CM = 1.0f;
const float MAX_VALID_DISTANCE_CM = 80.0f;

const String AUTHORIZED_UIDS[] = {
    "69C9D706"
};
const int NUM_AUTHORIZED = 1;

MFRC522 rfid(SS_PIN, RST_PIN);

static volatile unsigned long echoRiseUs = 0;
static volatile unsigned long echoPulseUs = 0;
static volatile bool echoPulseReady = false;
static volatile bool echoCaptureActive = false;

void IRAM_ATTR echoInterrupt() {
    if (!echoCaptureActive) return;

    if (digitalRead(ECHO_PIN) == HIGH) {
        echoRiseUs = micros();
        echoPulseReady = false;
    } else if (echoRiseUs > 0) {
        unsigned long pulseUs = micros() - echoRiseUs;
        if (pulseUs <= ULTRASONIC_TIMEOUT_US) {
            echoPulseUs = pulseUs;
            echoPulseReady = true;
        }
        echoRiseUs = 0;
        echoCaptureActive = false;
    }
}

static void triggerUltrasonicPulse() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
}

static float readSingleDistanceCm() {
    noInterrupts();
    echoRiseUs = 0;
    echoPulseReady = false;
    echoPulseUs = 0;
    echoCaptureActive = true;
    interrupts();

    triggerUltrasonicPulse();

    unsigned long startedAt = micros();
    while (!echoPulseReady && micros() - startedAt < ULTRASONIC_TIMEOUT_US) {
        taskYIELD();
    }

    noInterrupts();
    bool pulseReady = echoPulseReady;
    unsigned long pulseUs = echoPulseUs;
    echoCaptureActive = false;
    echoRiseUs = 0;
    interrupts();

    if (!pulseReady || pulseUs == 0) return -1;

    float distance = pulseUs * 0.0343f / 2.0f;
    if (distance < MIN_VALID_DISTANCE_CM || distance > MAX_VALID_DISTANCE_CM) return -1;
    return distance;
}

float readDistanceCm() {
    float samples[ULTRASONIC_SAMPLE_COUNT];
    uint8_t validSamples = 0;

    for (uint8_t i = 0; i < ULTRASONIC_SAMPLE_COUNT; i++) {
        float distance = readSingleDistanceCm();
        if (distance > 0) {
            samples[validSamples++] = distance;
        }

        if (i + 1 < ULTRASONIC_SAMPLE_COUNT) {
            vTaskDelay(pdMS_TO_TICKS(ULTRASONIC_SAMPLE_GAP_MS));
        }
    }

    if (validSamples == 0) return -1;

    for (uint8_t i = 0; i + 1 < validSamples; i++) {
        for (uint8_t j = i + 1; j < validSamples; j++) {
            if (samples[j] < samples[i]) {
                float temp = samples[i];
                samples[i] = samples[j];
                samples[j] = temp;
            }
        }
    }

    return samples[validSamples / 2];
}

String getCardUID() {
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    return uid;
}

bool isAuthorized(String uid) {
    for (int i = 0; i < NUM_AUTHORIZED; i++) {
        if (uid == AUTHORIZED_UIDS[i]) return true;
    }
    return false;
}

void sensorTaskCode(void *pvParameters) {
    (void)pvParameters;

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoInterrupt, CHANGE);

    SPI.begin();
    rfid.PCD_Init();
    vTaskDelay(pdMS_TO_TICKS(100));

    byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
    if (version == 0x00 || version == 0xFF) {
        addLog("ERRO", "RC522 nao detectado. Verifique fiacao SPI.");
    } else {
        addLog("RFID", "RC522 inicializado. Firmware: 0x" + String(version, HEX));
    }

    addLog("SISTEMA", "Task de sensores iniciada. RFID ativo apenas com veiculo detectado pelo HC-SR04.");

    uint8_t detectionCount = 0;
    uint8_t releaseCount = 0;
    uint8_t invalidCount = 0;

    for (;;) {
        unsigned long startUs = micros();
        float distance = readDistanceCm();

        if (distance > 0) {
            invalidCount = 0;
            lastDistanceCm = distance;

            if (!vehicleDetected) {
                releaseCount = 0;
                if (distance < DETECTION_DISTANCE_CM) {
                    detectionCount++;
                    if (detectionCount >= DETECTION_READINGS) {
                        vehicleDetected = true;
                        detectionCount = 0;
                        addLog("SENSOR", "Veiculo confirmado a " + String(distance, 1) + "cm.");
                    }
                } else {
                    detectionCount = 0;
                }
            } else {
                detectionCount = 0;
                if (distance >= RELEASE_DISTANCE_CM) {
                    releaseCount++;
                    if (releaseCount >= RELEASE_READINGS) {
                        vehicleDetected = false;
                        releaseCount = 0;
                        addLog("SENSOR", "Veiculo fora do alcance de deteccao a " + String(distance, 1) + "cm.");
                    }
                } else {
                    releaseCount = 0;
                }
            }
        } else {
            invalidCount++;
            detectionCount = 0;
            lastDistanceCm = 0;

            if (invalidCount >= INVALID_READINGS_LIMIT) {
                invalidCount = INVALID_READINGS_LIMIT;
                releaseCount = 0;
                if (vehicleDetected) {
                    vehicleDetected = false;
                    addLog("SENSOR", "Deteccao removida apos leituras invalidas.");
                }
            }
        }

        rfidActive = vehicleDetected && !isGateOpen && !servoCalibrationMode;

        if (rfidActive) {
            if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
                String uid = getCardUID();
                lastRfidUid = uid;

                if (isAuthorized(uid)) {
                    lastRfidStatus = "authorized";
                    lastRfidMessage = "Tag autorizada";
                    lastRfidAt = millis();
                    addLog("RFID", "TAG AUTORIZADA [" + uid + "]. Abrindo portao.");
                    isGateOpen = true;
                    gateOpenedAt = 0;
                    setServoTarget(servoOpenAngle);
                } else {
                    lastRfidStatus = "denied";
                    lastRfidMessage = "Tag negada";
                    lastRfidAt = millis();
                    addLog("RFID", "TAG NEGADA [" + uid + "]. Acesso bloqueado.");
                }

                rfid.PICC_HaltA();
                rfid.PCD_StopCrypto1();
            }
        }

        recordFunctionPerf(0, micros() - startUs);
        vTaskDelay(pdMS_TO_TICKS(systemConfig.sensorIntervalMs));
    }
}
