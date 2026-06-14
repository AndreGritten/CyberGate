#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "../system_state.h"

static unsigned long getStackHighWaterBytes(TaskHandle_t handle) {
    if (handle == NULL) return 0;
    return uxTaskGetStackHighWaterMark(handle) * sizeof(StackType_t);
}

static int getTaskPriority(TaskHandle_t handle) {
    if (handle == NULL) return -1;
    return uxTaskPriorityGet(handle);
}

static float getEstimatedCpuLoadPct() {
    float sensorLoad = performanceData[0].callCount
        ? (float)performanceData[0].totalTimeUs / performanceData[0].callCount / 300000.0f
        : 0.0f;
    float controlLoad = performanceData[1].callCount
        ? (float)performanceData[1].totalTimeUs / performanceData[1].callCount / 200000.0f
        : 0.0f;
    float apiLoad = 0.0f;
    for (uint8_t i = 2; i < 5; i++) {
        if (performanceData[i].callCount) {
            apiLoad += ((float)performanceData[i].totalTimeUs / performanceData[i].callCount) / 1000000.0f;
        }
    }

    float loadPct = (sensorLoad + controlLoad + apiLoad) * 100.0f;
    if (loadPct < 0.0f) return 0.0f;
    if (loadPct > 100.0f) return 100.0f;
    return loadPct;
}

static const char* wifiStatusLabel(wl_status_t status) {
    switch (status) {
        case WL_CONNECTED: return "Conectado";
        case WL_IDLE_STATUS: return "Ocioso";
        case WL_NO_SSID_AVAIL: return "SSID indisponivel";
        case WL_CONNECT_FAILED: return "Falha na conexao";
        case WL_CONNECTION_LOST: return "Conexao perdida";
        case WL_DISCONNECTED: return "Desconectado";
        default: return "Desconhecido";
    }
}

void setupRoutes(AsyncWebServer* server) {

    // Rota GET /api/status - Retorna os dados globais do sistema em formato JSON
    server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        unsigned long startUs = micros(); // Para medir desempenho

        // Cria o documento JSON
        StaticJsonDocument<512> doc;
        doc["isGateOpen"] = isGateOpen;
        doc["vehicleDetected"] = vehicleDetected;
        doc["rfidActive"] = rfidActive;
        doc["distance"] = lastDistanceCm;
        doc["lastUid"] = lastRfidUid;
        doc["lastRfidStatus"] = lastRfidStatus;
        doc["lastRfidMessage"] = lastRfidMessage;
        doc["lastRfidAt"] = lastRfidAt;
        doc["uptime"] = totalUptime;
        doc["memoryFree"] = freeHeapMemory;
        
        // Simulação do tempo que demorou a requisição - Monitoramento de Desempenho
        lastRequestTimeMs = (micros() - startUs) / 1000;
        doc["requestTime"] = lastRequestTimeMs; 
        recordFunctionPerf(2, micros() - startUs);
        
        // Enviamos tudo novamente com esse último campo adicionado
        String response;
        serializeJson(doc, response);

        request->send(200, "application/json", response);
    });

    // Rota POST /api/control - Rota para ativar os atuadores da página
    server->on("/api/control", HTTP_POST, [](AsyncWebServerRequest *request){
        unsigned long startUs = micros();

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

        recordFunctionPerf(4, micros() - startUs);
    });

    // Rota GET /api/logs - Retorna a fila em memória com as últimas atividades
    server->on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request){
        unsigned long startUs = micros();

        // ArduinoJson dinâmico para poder ter array variável
        DynamicJsonDocument doc(3072); 
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
            
            recordFunctionPerf(3, micros() - startUs);
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        } else {
            request->send(500, "application/json", "{\"error\":\"Server Busy\"}");
        }
    });

    // Rota GET /api/performance - Retorna métricas de funções e histórico de memória
    server->on("/api/performance", HTTP_GET, [](AsyncWebServerRequest *request){
        DynamicJsonDocument doc(8192);
        doc["uptime"] = totalUptime;
        doc["memoryFree"] = freeHeapMemory;
        doc["lastRequestTimeMs"] = lastRequestTimeMs;
        doc["cpuLoadPct"] = getEstimatedCpuLoadPct();
        doc["cpuFreqMhz"] = ESP.getCpuFreqMHz();

        JsonObject memory = doc.createNestedObject("memory");
        memory["heapTotal"] = ESP.getHeapSize();
        memory["heapFree"] = ESP.getFreeHeap();
        memory["heapMinFree"] = ESP.getMinFreeHeap();
        memory["heapMaxAlloc"] = ESP.getMaxAllocHeap();
        memory["psramTotal"] = ESP.getPsramSize();
        memory["psramFree"] = ESP.getFreePsram();
        memory["flashSize"] = ESP.getFlashChipSize();
        memory["sketchSize"] = ESP.getSketchSize();
        memory["sketchFree"] = ESP.getFreeSketchSpace();
        memory["littleFsTotal"] = LittleFS.totalBytes();
        memory["littleFsUsed"] = LittleFS.usedBytes();
        memory["sensorStackFree"] = getStackHighWaterBytes(sensorTaskHandle);
        memory["controlStackFree"] = getStackHighWaterBytes(controlTaskHandle);

        JsonObject sensors = doc.createNestedObject("sensors");
        sensors["distanceCm"] = lastDistanceCm;
        sensors["vehicleDetected"] = vehicleDetected;
        sensors["rfidActive"] = rfidActive;
        sensors["lastUid"] = lastRfidUid;
        sensors["lastRfidStatus"] = lastRfidStatus;
        sensors["lastRfidMessage"] = lastRfidMessage;
        sensors["lastRfidAt"] = lastRfidAt;
        sensors["gateOpen"] = isGateOpen;

        JsonObject wifi = doc.createNestedObject("wifi");
        wl_status_t status = WiFi.status();
        wifi["status"] = wifiStatusLabel(status);
        wifi["connected"] = status == WL_CONNECTED;
        wifi["mode"] = WiFi.getMode();
        wifi["ssid"] = WiFi.SSID();
        wifi["rssi"] = WiFi.RSSI();
        wifi["localIp"] = WiFi.localIP().toString();
        wifi["mac"] = WiFi.macAddress();
        wifi["apIp"] = WiFi.softAPIP().toString();
        wifi["apStations"] = WiFi.softAPgetStationNum();

        JsonObject tasks = doc.createNestedObject("tasks");
        tasks["total"] = uxTaskGetNumberOfTasks();
        JsonArray taskArray = tasks.createNestedArray("items");
        JsonObject sensorTask = taskArray.createNestedObject();
        sensorTask["name"] = "SensorTask";
        sensorTask["priority"] = getTaskPriority(sensorTaskHandle);
        sensorTask["core"] = 1;
        sensorTask["periodMs"] = 300;
        sensorTask["stackFree"] = getStackHighWaterBytes(sensorTaskHandle);
        JsonObject controlTask = taskArray.createNestedObject();
        controlTask["name"] = "ControlTask";
        controlTask["priority"] = getTaskPriority(controlTaskHandle);
        controlTask["core"] = 1;
        controlTask["periodMs"] = 200;
        controlTask["stackFree"] = getStackHighWaterBytes(controlTaskHandle);
        JsonObject webTask = taskArray.createNestedObject();
        webTask["name"] = "AsyncWebServer";
        webTask["priority"] = "-";
        webTask["core"] = "WiFi/Async";
        webTask["periodMs"] = 0;
        webTask["stackFree"] = 0;

        JsonArray historyArray = doc.createNestedArray("memoryHistory");
        for (unsigned int i = 0; i < memoryHistorySize; i++) {
            unsigned int idx = memoryHistorySize == PERFORMANCE_HISTORY_SIZE
                ? (memoryHistoryIndex + i) % PERFORMANCE_HISTORY_SIZE
                : i;
            historyArray.add(memoryHistory[idx]);
        }

        JsonArray perfArray = doc.createNestedArray("functions");
        for (uint8_t i = 0; i < 5; i++) {
            JsonObject it = perfArray.createNestedObject();
            it["name"] = performanceData[i].name;
            it["calls"] = performanceData[i].callCount;
            it["lastUs"] = performanceData[i].lastTimeUs;
            it["avgUs"] = performanceData[i].callCount ? (performanceData[i].totalTimeUs / performanceData[i].callCount) : 0;
            it["maxUs"] = performanceData[i].maxTimeUs;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

}
