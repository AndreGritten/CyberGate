#include <Arduino.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include "../system_state.h"

const int GATE_LED_PIN = 2;
const int GATE_SERVO_PIN = 13;
const int SERVO_STEP_DELAY_MS = 20;

static Servo gateServo;
static Preferences servoPreferences;

void controlTaskCode(void *pvParameters) {
    (void)pvParameters;

    pinMode(GATE_LED_PIN, OUTPUT);
    digitalWrite(GATE_LED_PIN, LOW);

    servoPreferences.begin("gate-servo", false);

    // Versao 5 inverte aberto/fechado para o sentido mecanico atual.
    if (servoPreferences.getUChar("version", 0) < 5) {
        servoPreferences.putInt("closed", 0);
        servoPreferences.putInt("open", 170);
        servoPreferences.putUChar("version", 5);
    }

    servoClosedAngle = constrain(servoPreferences.getInt("closed", 0), 0, 180);
    servoOpenAngle = constrain(servoPreferences.getInt("open", 170), 0, 180);
    servoCurrentAngle = servoClosedAngle;
    servoTargetAngle = servoClosedAngle;

    gateServo.setPeriodHertz(50);
    gateServo.attach(GATE_SERVO_PIN, 500, 2400);
    gateServo.write(servoCurrentAngle);

    addLog("SISTEMA", "Servo iniciado no GPIO 13. Fechado=" + String(servoClosedAngle)
        + " graus, aberto=" + String(servoOpenAngle) + " graus.");

    unsigned long lastMetricsAt = 0;
    unsigned long lastPersistedMetricAt = 0;
    unsigned long lastServoStepAt = 0;

    for (;;) {
        unsigned long startUs = micros();

        if (millis() - lastMetricsAt >= 1000) {
            lastMetricsAt = millis();
            freeHeapMemory = ESP.getFreeHeap();
            totalUptime = millis() / 1000;
            pushMemorySample(freeHeapMemory);
            if (lastPersistedMetricAt == 0 || millis() - lastPersistedMetricAt >= 60000) {
                lastPersistedMetricAt = millis();
                persistMetricSample(freeHeapMemory, lastDistanceCm, getEstimatedCpuLoadPct());
            }
        }

        if (saveServoOpenRequested) {
            saveServoOpenRequested = false;
            servoPreferences.putInt("open", servoOpenAngle);
            addLog("CALIBRACAO", "Posicao aberta salva em " + String(servoOpenAngle) + " graus.");
        }
        if (saveServoClosedRequested) {
            saveServoClosedRequested = false;
            servoPreferences.putInt("closed", servoClosedAngle);
            addLog("CALIBRACAO", "Posicao fechada salva em " + String(servoClosedAngle) + " graus.");
        }

        if (isGateOpen && !servoCalibrationMode && servoCurrentAngle == servoOpenAngle && gateOpenedAt == 0) {
            gateOpenedAt = millis();
            addLog("CONTROLE", "Portao chegou a posicao aberta.");
        }

        if (isGateOpen && !servoCalibrationMode && gateOpenedAt > 0 && millis() - gateOpenedAt > systemConfig.gateAutoCloseMs) {
            isGateOpen = false;
            vehicleDetected = false;
            rfidActive = false;
            setServoTarget(servoClosedAngle);
            gateOpenedAt = 0;
            addLog("CONTROLE", "Fechando portao automaticamente.");
        }

        if (millis() - lastServoStepAt >= SERVO_STEP_DELAY_MS && servoCurrentAngle != servoTargetAngle) {
            lastServoStepAt = millis();
            servoCurrentAngle += servoCurrentAngle < servoTargetAngle ? 1 : -1;
            gateServo.write(servoCurrentAngle);
        }

        digitalWrite(GATE_LED_PIN, isGateOpen ? HIGH : LOW);
        recordFunctionPerf(1, micros() - startUs);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
