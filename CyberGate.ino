#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include "src/system_state.h"

extern void initWebServer();
extern void sensorTaskCode(void *pvParameters);
extern void controlTaskCode(void *pvParameters);

static const char* SETUP_AP_SSID = "CyberGate-Setup";
static const char* SETUP_AP_PASSWORD = "cybergate";
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 12000;
static const unsigned long LIGHT_SLEEP_IDLE_MS = 15000;
static const unsigned long LIGHT_SLEEP_WEB_IDLE_MS = 10000;
static const uint64_t LIGHT_SLEEP_WAKE_US = 250000ULL;
static const BaseType_t SENSOR_TASK_CORE = 1;
static const BaseType_t CONTROL_TASK_CORE = 0;

SemaphoreHandle_t logMutex;
static unsigned long wifiAttemptStartedAt = 0;
static bool wifiConnecting = false;

static void startSetupAccessPoint() {
    WiFi.mode(WIFI_AP_STA);
    bool apStarted = WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASSWORD, 6, false, 4);
    wifiApActive = apStarted;
    wifiModeLabel = apStarted ? "AP fallback" : "Falha no AP";
    activeWifiSsid = apStarted ? SETUP_AP_SSID : "";
    if (apStarted) {
        addLog("REDE", String("AP de configuracao ativo: ") + SETUP_AP_SSID + " IP " + WiFi.softAPIP().toString());
    } else {
        addLog("ERRO", "Falha ao iniciar AP de configuracao CyberGate-Setup.");
    }
}

static void connectConfiguredWifi() {
    if (systemConfig.wifiSsid.length() == 0) {
        addLog("REDE", "Nenhum Wi-Fi salvo. Abrindo AP de configuracao.");
        startSetupAccessPoint();
        return;
    }

    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(systemConfig.wifiSsid.c_str(), systemConfig.wifiPassword.c_str());
    wifiModeLabel = "Conectando";
    activeWifiSsid = systemConfig.wifiSsid;
    wifiConnecting = true;
    wifiAttemptStartedAt = millis();
    addLog("REDE", "Tentando conectar ao Wi-Fi salvo: " + maskedWifiSsid());
}

static void monitorWifiConnection() {
    if (!wifiConnecting && systemConfig.wifiSsid.length() > 0 && WiFi.status() == WL_CONNECTED && wifiApActive) {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        wifiApActive = false;
        wifiModeLabel = "Cliente Wi-Fi";
        activeWifiSsid = WiFi.SSID();
        addLog("REDE", String("Reconexao concluida. IP do servidor: ") + WiFi.localIP().toString());
        return;
    }

    if (!wifiConnecting) return;

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnecting = false;
        if (wifiApActive) {
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            wifiApActive = false;
        }
        wifiModeLabel = "Cliente Wi-Fi";
        activeWifiSsid = WiFi.SSID();
        addLog("REDE", String("Conectado. IP do servidor: ") + WiFi.localIP().toString());
        return;
    }

    if (millis() - wifiAttemptStartedAt >= WIFI_CONNECT_TIMEOUT_MS) {
        wifiConnecting = false;
        addLog("WARNING", "Falha ao conectar no Wi-Fi salvo. Mantendo AP de configuracao.");
        startSetupAccessPoint();
    }
}

static bool canEnterLightSleep() {
    if (!systemConfig.lightSleepEnabled) return false;
    if (wifiApActive && WiFi.softAPgetStationNum() > 0) return false;
    if (WiFi.status() == WL_CONNECTED && millis() - lastWebRequestAt < LIGHT_SLEEP_WEB_IDLE_MS) return false;
    if (isGateOpen || vehicleDetected || rfidActive || servoCalibrationMode) return false;
    if (servoCurrentAngle != servoTargetAngle) return false;
    if (millis() < LIGHT_SLEEP_IDLE_MS) return false;
    if (millis() - lastLightSleepAt < LIGHT_SLEEP_IDLE_MS) return false;
    return true;
}

static void runLightSleepIfIdle() {
    if (!canEnterLightSleep()) return;

    lightSleepActive = true;
    lastLightSleepAt = millis();
    lightSleepCount++;
    esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_WAKE_US);
    esp_light_sleep_start();
    lightSleepActive = false;
}

void setup() {
    Serial.begin(115200);
    WiFi.persistent(false);
    WiFi.setSleep(false);

    logMutex = xSemaphoreCreateMutex();
    loadSystemConfig();

    addLog("SISTEMA", "Iniciando CyberGate Controller...");
    connectConfiguredWifi();
    initWebServer();

    xTaskCreatePinnedToCore(
        sensorTaskCode,
        "SensorTask",
        8192,
        NULL,
        1,
        &sensorTaskHandle,
        SENSOR_TASK_CORE
    );

    xTaskCreatePinnedToCore(
        controlTaskCode,
        "ControlTask",
        4096,
        NULL,
        2,
        &controlTaskHandle,
        CONTROL_TASK_CORE
    );

    addLog("SISTEMA", "Setup concluido. SensorTask no core 1, ControlTask no core 0, web server no core Wi-Fi/Async.");
}

void loop() {
    monitorWifiConnection();
    runLightSleepIfIdle();
    vTaskDelay(pdMS_TO_TICKS(500));
}
