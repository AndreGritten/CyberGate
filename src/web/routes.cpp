#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "../system_state.h"

void setupRoutes(AsyncWebServer* server) {

    // Rota GET /api/status - Retorna os dados globais do sistema em formato JSON
    server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        unsigned long start = millis(); // Para medir desempenho

        // Cria o documento JSON
        StaticJsonDocument<256> doc;
        doc["isGateOpen"] = isGateOpen;
        doc["vehicleDetected"] = vehicleDetected;
        doc["rfidActive"] = rfidActive;
        doc["distance"] = lastDistanceCm;
        doc["lastUid"] = lastRfidUid;
        doc["uptime"] = totalUptime;
        doc["memoryFree"] = freeHeapMemory;
        
        String response;
        serializeJson(doc, response);
        
        // Simulação do tempo que demorou a requisição - Monitoramento de Desempenho
        lastRequestTimeMs = millis() - start;
        doc["requestTime"] = lastRequestTimeMs; 
        
        // Enviamos tudo novamente com esse último campo adicionado
        response = "";
        serializeJson(doc, response);

        request->send(200, "application/json", response);
    });

    // Rota POST /api/control - Rota para ativar os atuadores da página
    server->on("/api/control", HTTP_POST, [](AsyncWebServerRequest *request){
        // Verifica o comando passado na URL ?action=openGate
        if (request->hasParam("action")) {
            String act = request->getParam("action")->value();
            
            if (act == "openGate") {
                isGateOpen = true; // Muda o estado global
                gateOpenedAt = millis(); // Marca quando abriu
                addLog("USUARIO", "Comando de abertura do portão enviado via WEB.");
                request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Portal abrindo\"}");
            } else {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Comando invalido\"}");
            }
        } else {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Faltando action\"}");
        }
    });

    // Rota GET /api/logs - Retorna a fila em memória com as últimas atividades
    server->on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request){
        // ArduinoJson dinâmico para poder ter array variável
        DynamicJsonDocument doc(2048); 
        JsonArray logsArray = doc.createNestedArray("logs");

        // Bloqueamos temporariamente os logs para garantir que ninguém adicione nada na memória
        // Enquanto o JSON é gerado, para não corromper o array (Thread safety)
        if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(100))) {
            for (const auto& logItem : systemLogs) {
                JsonObject logObj = logsArray.createNestedObject();
                logObj["timestamp"] = logItem.timestamp;
                logObj["type"] = logItem.type;
                logObj["message"] = logItem.message;
            }
            xSemaphoreGive(logMutex);
            
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        } else {
            request->send(500, "application/json", "{\"error\":\"Server Busy\"}");
        }
    });

}
