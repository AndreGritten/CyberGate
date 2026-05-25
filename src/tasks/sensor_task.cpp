#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include "../system_state.h"

// =============================================
// PINOS — HC-SR04 (Ultrassônico)
// =============================================
#define TRIG_PIN  26
#define ECHO_PIN  27

// =============================================
// PINOS — RC522 (RFID via SPI)
// =============================================
#define SS_PIN    21
#define RST_PIN   22

// =============================================
// CONFIGURAÇÕES
// =============================================
#define DETECTION_DISTANCE_CM  10   // Ativa o RFID quando objeto está a até 50cm

// UIDs autorizados (cadastrados)
const String AUTHORIZED_UIDS[] = {
    "69C9D706" // Apenas este será autorizado. O "01186906" será bloqueado!
};
const int NUM_AUTHORIZED = 1;

// Instância do leitor RFID
MFRC522 rfid(SS_PIN, RST_PIN);

// =============================================
// Funções auxiliares
// =============================================

// Mede a distância em cm usando o HC-SR04
float readDistanceCm() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 50000); // timeout 50ms
    if (duration == 0) return -1; // Sem retorno
    return duration * 0.034 / 2.0;
}

// Converte o UID do cartão RFID para String hexadecimal
String getCardUID() {
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    return uid;
}

// Verifica se o UID está na lista de autorizados
bool isAuthorized(String uid) {
    for (int i = 0; i < NUM_AUTHORIZED; i++) {
        if (uid == AUTHORIZED_UIDS[i]) return true;
    }
    return false;
}

// =============================================
// Task principal dos sensores (FreeRTOS)
// =============================================
void sensorTaskCode(void *pvParameters) {
    (void)pvParameters;

    // Inicializa pinos do ultrassônico
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // Inicializa SPI e RFID
    SPI.begin();
    rfid.PCD_Init();
    delay(100); // Tempo para o RC522 estabilizar

    // Verifica se o RC522 está respondendo
    byte versao = rfid.PCD_ReadRegister(rfid.VersionReg);
    if (versao == 0x00 || versao == 0xFF) {
        addLog("ERRO", "RC522 não detectado! Verifique fiação SPI.");
    } else {
        addLog("RFID", "RC522 inicializado. Firmware: 0x" + String(versao, HEX));
    }

    addLog("SISTEMA", "Task de Sensores iniciada com hardware real.");

    for (;;) {
        unsigned long startUs = micros();
        // -----------------------------------------------
        // ETAPA 1: Leitura do Sensor Ultrassônico
        // -----------------------------------------------
        float distancia = readDistanceCm();

        if (distancia > 0) {
            lastDistanceCm = distancia; // Atualiza estado global para a API

            // Veículo detectado dentro do alcance?
            if (!vehicleDetected && distancia <= DETECTION_DISTANCE_CM) {
                vehicleDetected = true;
                rfidActive = true;
                addLog("SENSOR", "Veiculo detectado a " + String(distancia, 1) + "cm. RFID ativado!");
            }
            // Veículo saiu do alcance e portão não está aberto?
            else if (vehicleDetected && !isGateOpen && distancia > DETECTION_DISTANCE_CM) {
                vehicleDetected = false;
                rfidActive = false;
                addLog("SENSOR", "Veiculo saiu do alcance (" + String(distancia, 1) + "cm). RFID desativado.");
            }
        }

        // -----------------------------------------------
        // ETAPA 2: Leitura RFID (só ativa quando veículo está próximo)
        // -----------------------------------------------
        if (rfidActive && !isGateOpen) {
            if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
                String uid = getCardUID();
                lastRfidUid = uid;

                if (isAuthorized(uid)) {
                    addLog("RFID", "TAG AUTORIZADA [" + uid + "]. Abrindo portão!");
                    isGateOpen = true;
                    gateOpenedAt = millis();
                } else {
                    addLog("RFID", "TAG NEGADA [" + uid + "]. Acesso bloqueado.");
                }

                // Libera o cartão do leitor
                rfid.PICC_HaltA();
                rfid.PCD_StopCrypto1();
            }
        }

        recordFunctionPerf(0, micros() - startUs);

        // Pausa não-bloqueante de 300ms
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
