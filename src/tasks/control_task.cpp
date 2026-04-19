#include <Arduino.h>
#include "../system_state.h"

// Pino do LED que simulará o nosso atuador do portão
const int GATE_LED_PIN = 2; // O pino D2 na maioria dos ESP32 é o LED embutido da placa.

// Task responsável por controlar o "Motor" do portão (Atuadores).
void controlTaskCode(void *pvParameters) {
    (void)pvParameters;

    // Configura o pino como saída
    pinMode(GATE_LED_PIN, OUTPUT);
    digitalWrite(GATE_LED_PIN, LOW); // Garante que começa desligado

    addLog("SISTEMA", "Task de Controle iniciada.");

    for (;;) {
        // Atualiza métricas de gerenciamento de memória no estado do sistema
        freeHeapMemory = ESP.getFreeHeap();
        totalUptime = millis() / 1000;

        // Verifica se houve comando via Web (isGateOpen foi setada para true)
        // Ou se houve aprovação pelo sensor
        if (isGateOpen) {
            // Acende o LED simulando que o motor está rodando / portão está aberto
            digitalWrite(GATE_LED_PIN, HIGH);
            
            // Simula o tempo que o portão fica aberto (por exemplo, 7 segundos)
            if (millis() - gateOpenedAt > 7000) {
                // Terminou o tempo, fecha o portão
                isGateOpen = false;
                vehicleDetected = false; // Resetamos a simulação do sensor
                digitalWrite(GATE_LED_PIN, LOW);
                addLog("CONTROLE", "Portão fechado.");
            }
        } else {
            // Garante que o LED fique apagado
            digitalWrite(GATE_LED_PIN, LOW);
        }

        // Pausa de 200 milissegundos
        vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}
