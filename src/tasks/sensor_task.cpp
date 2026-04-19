#include <Arduino.h>
#include "../system_state.h"

// Task responsável por simular a detecção de veículos/RFID
// Num cenário real, esta task leria pinos digitais ou comunicaria via I2C/SPI com um leitor MFRC522.
void sensorTaskCode(void *pvParameters) {
    (void)pvParameters; // Parâmetro não utilizado

    addLog("SISTEMA", "Task de Sensores iniciada.");

    unsigned long lastDetectionTime = 0;

    for (;;) {
        // Simulação: A cada 15 segundos, simulamos que um "carro chegou" (se não tiver detectado antes)
        if (!vehicleDetected && (millis() - lastDetectionTime > 15000)) {
            // Em vez de usar delay() bloqueante, apenas mudamos o estado.
            vehicleDetected = true;
            lastDetectionTime = millis();
            addLog("SENSOR", "Veículo detectado na entrada. Aguardando leitura RFID.");
        }

        // Simulação: Após 5 segundos que o carro foi detectado, simulamos a leitura de uma Tag RFID autorizada
        if (vehicleDetected && !isGateOpen && (millis() - lastDetectionTime > 5000)) {
            // O controle de acesso aprova a tag
            addLog("RFID", "TAG autorizada [ID: A1:B2:C3]. Solicitando abertura do portão.");
            isGateOpen = true;        // Avisa a outra Task para abrir o portão
            gateOpenedAt = millis();  // Registra o tempo
            // O controle passa para a ControlTask processar a abertura do portão
        }

        // vTaskDelay é a forma correta do FreeRTOS de pausar a task sem bloquear a CPU.
        // Diferente do delay() nativo, ele avisa ao processador: "Pode executar outras tasks por 500ms".
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}
