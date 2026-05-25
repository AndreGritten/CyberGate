#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <vector>

// Estrutura para salvar os logs em memória
struct SystemLog {
    unsigned long timestamp;
    String type;
    String message;
};

// Variáveis Globais (Estado do Sistema)
// Usamos volatile para variáveis que são alteradas dentro de Tasks e lidas em outras.
extern volatile bool isGateOpen;
extern volatile bool vehicleDetected;
extern volatile bool rfidActive;           // RFID está ativado (veículo próximo)
extern volatile unsigned long gateOpenedAt;
extern volatile float lastDistanceCm;      // Última leitura do ultrassônico
extern String lastRfidUid;                 // UID do último cartão lido

// Métricas de Performance
extern volatile unsigned long totalUptime;
extern volatile unsigned int freeHeapMemory;
extern volatile unsigned long lastRequestTimeMs; // Tempo que demorou a última request

// Performance tracking
constexpr int PERFORMANCE_HISTORY_SIZE = 20;

struct FunctionPerf {
    const char* name;
    unsigned long callCount;
    unsigned long totalTimeUs;
    unsigned long lastTimeUs;
    unsigned long maxTimeUs;
};

extern FunctionPerf performanceData[5];
extern unsigned long memoryHistory[PERFORMANCE_HISTORY_SIZE];
extern unsigned int memoryHistorySize;
extern unsigned int memoryHistoryIndex;

void recordFunctionPerf(uint8_t index, unsigned long durationUs);
void pushMemorySample(unsigned int heapFree);

// Controle de Logs (Armazenados na RAM para nossa simulação)
extern std::vector<SystemLog> systemLogs;
extern SemaphoreHandle_t logMutex; // Disponibilizado globalmente

// Função utilitária para adicionar logs de forma thread-safe (básica)
void addLog(String type, String message);

#endif
