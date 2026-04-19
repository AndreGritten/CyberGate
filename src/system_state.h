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
extern volatile unsigned long gateOpenedAt;

// Métricas de Performance
extern volatile unsigned long totalUptime;
extern volatile unsigned int freeHeapMemory;
extern volatile unsigned long lastRequestTimeMs; // Tempo que demorou a última request

// Controle de Logs (Armazenados na RAM para nossa simulação)
extern std::vector<SystemLog> systemLogs;
extern SemaphoreHandle_t logMutex; // Disponibilizado globalmente

// Função utilitária para adicionar logs de forma thread-safe (básica)
void addLog(String type, String message);

#endif
