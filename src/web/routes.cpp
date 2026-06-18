#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "../system_state.h"

static const char* SETUP_AP_SSID = "CyberGate-Setup";
static const char* SETUP_AP_PASSWORD = "cybergate";

static unsigned long getStackHighWaterBytes(TaskHandle_t handle) {
    if (handle == NULL) return 0;
    return uxTaskGetStackHighWaterMark(handle) * sizeof(StackType_t);
}

static int getTaskPriority(TaskHandle_t handle) {
    if (handle == NULL) return -1;
    return uxTaskPriorityGet(handle);
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

static void markWebRequest() {
    lastWebRequestAt = millis();
}

static void ensureSetupAp() {
    WiFi.mode(WIFI_AP_STA);
    if (!wifiApActive) {
        WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASSWORD);
    }
    wifiApActive = true;
    wifiModeLabel = "AP fallback";
    activeWifiSsid = SETUP_AP_SSID;
}

static String configJson() {
    DynamicJsonDocument doc(1024);
    doc["sensorIntervalMs"] = systemConfig.sensorIntervalMs;
    doc["gateAutoCloseMs"] = systemConfig.gateAutoCloseMs;
    doc["lightSleepEnabled"] = systemConfig.lightSleepEnabled;
    doc["wifiSsidMasked"] = maskedWifiSsid();
    doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    doc["wifiStatus"] = wifiStatusLabel(WiFi.status());
    doc["wifiMode"] = wifiModeLabel;
    doc["apActive"] = wifiApActive;
    doc["apSsid"] = SETUP_AP_SSID;
    doc["apIp"] = WiFi.softAPIP().toString();

    String response;
    serializeJson(doc, response);
    return response;
}

static void sendLogsCsv(AsyncWebServerRequest *request) {
    markWebRequest();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/csv; charset=utf-8", buildLogsCsv());
    response->addHeader("Content-Disposition", "attachment; filename=cybergate-logs.csv");
    response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    request->send(response);
}

void setupRoutes(AsyncWebServer* server) {
    server->on("/logs.csv", HTTP_GET, [](AsyncWebServerRequest *request){
        sendLogsCsv(request);
    });

    server->on("/api/logs.csv", HTTP_GET, [](AsyncWebServerRequest *request){
        sendLogsCsv(request);
    });

    server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        markWebRequest();
        unsigned long startUs = micros();

        StaticJsonDocument<1280> doc;
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
        doc["servoCurrentAngle"] = servoCurrentAngle;
        doc["servoTargetAngle"] = servoTargetAngle;
        doc["servoOpenAngle"] = servoOpenAngle;
        doc["servoClosedAngle"] = servoClosedAngle;
        doc["servoCalibrationMode"] = servoCalibrationMode;
        doc["sensorIntervalMs"] = systemConfig.sensorIntervalMs;
        doc["gateAutoCloseMs"] = systemConfig.gateAutoCloseMs;
        doc["lightSleepEnabled"] = systemConfig.lightSleepEnabled;
        doc["lightSleepActive"] = lightSleepActive;
        doc["lightSleepCount"] = lightSleepCount;
        doc["wifiMode"] = wifiModeLabel;
        doc["wifiApActive"] = wifiApActive;
        doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;

        lastRequestTimeMs = (micros() - startUs) / 1000;
        doc["requestTime"] = lastRequestTimeMs;
        recordFunctionPerf(2, micros() - startUs);

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server->on("/api/control", HTTP_POST, [](AsyncWebServerRequest *request){
        markWebRequest();
        unsigned long startUs = micros();

        if (request->hasParam("action")) {
            String act = request->getParam("action")->value();

            if (act == "openGate") {
                servoCalibrationMode = false;
                isGateOpen = true;
                gateOpenedAt = 0;
                setServoTarget(servoOpenAngle);
                addLog("USUARIO", "Comando de abertura do portao enviado via web.");
                request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Portao abrindo\"}");
            } else if (act == "closeGate") {
                servoCalibrationMode = false;
                isGateOpen = false;
                gateOpenedAt = 0;
                setServoTarget(servoClosedAngle);
                addLog("USUARIO", "Comando de fechamento enviado via web.");
                request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Portao fechando\"}");
            } else if (act == "stepServo" && request->hasParam("amount")) {
                int amount = constrain(request->getParam("amount")->value().toInt(), -90, 90);
                servoCalibrationMode = true;
                stepServoTarget(amount);
                request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Posicao ajustada\"}");
            } else if (act == "saveOpen") {
                servoCalibrationMode = true;
                servoOpenAngle = servoCurrentAngle;
                servoTargetAngle = servoCurrentAngle;
                saveServoOpenRequested = true;
                isGateOpen = true;
                request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Posicao aberta registrada\"}");
            } else if (act == "saveClosed") {
                servoCalibrationMode = true;
                servoClosedAngle = servoCurrentAngle;
                servoTargetAngle = servoCurrentAngle;
                saveServoClosedRequested = true;
                isGateOpen = false;
                request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Posicao fechada registrada\"}");
            } else if (act == "testOpen") {
                servoCalibrationMode = true;
                isGateOpen = true;
                setServoTarget(servoOpenAngle);
                request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Movendo para aberto\"}");
            } else if (act == "testClosed") {
                servoCalibrationMode = true;
                isGateOpen = false;
                setServoTarget(servoClosedAngle);
                request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Movendo para fechado\"}");
            } else if (act == "finishCalibration") {
                servoCalibrationMode = false;
                isGateOpen = false;
                setServoTarget(servoClosedAngle);
                request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Calibracao concluida\"}");
            } else {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Comando invalido\"}");
            }
        } else {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Faltando action\"}");
        }

        recordFunctionPerf(4, micros() - startUs);
    });

    server->on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request){
        markWebRequest();
        unsigned long startUs = micros();

        DynamicJsonDocument doc(8192);
        JsonArray logsArray = doc.createNestedArray("logs");

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

    server->on("/api/logs/export", HTTP_GET, [](AsyncWebServerRequest *request){
        sendLogsCsv(request);
    });

    server->on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
        markWebRequest();
        request->send(200, "application/json", configJson());
    });

    server->on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request){
        markWebRequest();
        if (request->hasParam("sensorIntervalMs")) {
            systemConfig.sensorIntervalMs = constrain(request->getParam("sensorIntervalMs")->value().toInt(), 100, 10000);
        }
        if (request->hasParam("gateAutoCloseMs")) {
            systemConfig.gateAutoCloseMs = constrain(request->getParam("gateAutoCloseMs")->value().toInt(), 1000, 60000);
        }
        if (request->hasParam("lightSleepEnabled")) {
            String value = request->getParam("lightSleepEnabled")->value();
            systemConfig.lightSleepEnabled = value == "1" || value == "true" || value == "on";
        }
        saveSystemConfig();
        addLog("CONFIG", "Parametros operacionais salvos pela interface web.");
        request->send(200, "application/json", configJson());
    });

    server->on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *request){
        markWebRequest();
        if (!request->hasParam("ssid") || !request->hasParam("password")) {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Informe ssid e password\"}");
            return;
        }

        String ssid = request->getParam("ssid")->value();
        String password = request->getParam("password")->value();
        setWifiCredentials(ssid, password);
        addLog("REDE", "Novas credenciais Wi-Fi salvas pela interface.");

        WiFi.disconnect(false, false);
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(systemConfig.wifiSsid.c_str(), systemConfig.wifiPassword.c_str());
        wifiModeLabel = "Reconectando";
        activeWifiSsid = systemConfig.wifiSsid;
        ensureSetupAp();

        request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Credenciais salvas. O ESP32 tentara reconectar mantendo o AP de configuracao.\"}");
    });

    server->on("/api/performance", HTTP_GET, [](AsyncWebServerRequest *request){
        markWebRequest();
        DynamicJsonDocument doc(8192);
        doc["uptime"] = totalUptime;
        doc["memoryFree"] = freeHeapMemory;
        doc["lastRequestTimeMs"] = lastRequestTimeMs;
        doc["cpuLoadPct"] = getEstimatedCpuLoadPct();
        doc["cpuFreqMhz"] = ESP.getCpuFreqMHz();

        JsonObject energy = doc.createNestedObject("energy");
        energy["lightSleepEnabled"] = systemConfig.lightSleepEnabled;
        energy["lightSleepActive"] = lightSleepActive;
        energy["lightSleepCount"] = lightSleepCount;
        energy["lastLightSleepAt"] = lastLightSleepAt;

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
        wifi["mode"] = wifiModeLabel;
        wifi["ssid"] = WiFi.SSID();
        wifi["rssi"] = WiFi.RSSI();
        wifi["localIp"] = WiFi.localIP().toString();
        wifi["mac"] = WiFi.macAddress();
        wifi["apActive"] = wifiApActive;
        wifi["apSsid"] = SETUP_AP_SSID;
        wifi["apIp"] = WiFi.softAPIP().toString();
        wifi["apStations"] = WiFi.softAPgetStationNum();

        JsonObject tasks = doc.createNestedObject("tasks");
        tasks["total"] = uxTaskGetNumberOfTasks();
        JsonArray taskArray = tasks.createNestedArray("items");
        JsonObject sensorTask = taskArray.createNestedObject();
        sensorTask["name"] = "SensorTask";
        sensorTask["priority"] = getTaskPriority(sensorTaskHandle);
        sensorTask["core"] = 1;
        sensorTask["periodMs"] = systemConfig.sensorIntervalMs;
        sensorTask["stackFree"] = getStackHighWaterBytes(sensorTaskHandle);
        JsonObject controlTask = taskArray.createNestedObject();
        controlTask["name"] = "ControlTask";
        controlTask["priority"] = getTaskPriority(controlTaskHandle);
        controlTask["core"] = 0;
        controlTask["periodMs"] = 10;
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
