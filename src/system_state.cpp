#include "system_state.h"

// Inicialização das variáveis globais
volatile bool isGateOpen = false;
volatile bool vehicleDetected = false;
volatile bool rfidActive = false;
volatile unsigned long gateOpenedAt = 0;
volatile float lastDistanceCm = 0;
String lastRfidUid = "";

volatile unsigned long totalUptime = 0;
volatile unsigned int freeHeapMemory = 0;
volatile unsigned long lastRequestTimeMs = 0;

std::vector<SystemLog> systemLogs;

// Usamos um mutex do FreeRTOS para proteger a adição de logs
extern SemaphoreHandle_t logMutex; 

void addLog(String type, String message) {
    if (xSemaphoreTake(logMutex, portMAX_DELAY)) {
        SystemLog newLog;
        newLog.timestamp = millis();
        newLog.type = type;
        newLog.message = message;
        
        // Mantém apenas os últimos 20 logs para não encher a memória (vazamento de heap)
        if (systemLogs.size() >= 20) {
            systemLogs.erase(systemLogs.begin());
        }
        systemLogs.push_back(newLog);
        
        Serial.printf("[%s] %s\n", type.c_str(), message.c_str());
        xSemaphoreGive(logMutex);
    }
}
