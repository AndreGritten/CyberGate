#include "system_state.h"
#include <LittleFS.h>
#include <Preferences.h>

static const char* LOG_FILE = "/logs.csv";
static const char* METRICS_FILE = "/metrics.csv";
static const size_t MAX_LOG_CACHE = 80;
static const size_t MAX_LOG_FILE_BYTES = 180000;
static const size_t MAX_METRICS_FILE_BYTES = 120000;
static const char* DEFAULT_WIFI_SSID = "gritten";
static const char* DEFAULT_WIFI_PASSWORD = "casa1802";
static const uint8_t DEFAULT_WIFI_VERSION = 1;

volatile bool isGateOpen = false;
volatile bool vehicleDetected = false;
volatile bool rfidActive = false;
volatile unsigned long gateOpenedAt = 0;
volatile int servoCurrentAngle = 90;
volatile int servoTargetAngle = 90;
volatile int servoOpenAngle = 90;
volatile int servoClosedAngle = 90;
volatile bool servoCalibrationMode = false;
volatile bool saveServoOpenRequested = false;
volatile bool saveServoClosedRequested = false;
volatile float lastDistanceCm = 0;
String lastRfidUid = "";
String lastRfidStatus = "none";
String lastRfidMessage = "Aguardando leitura";
volatile unsigned long lastRfidAt = 0;

volatile unsigned long totalUptime = 0;
volatile unsigned int freeHeapMemory = 0;
volatile unsigned long lastRequestTimeMs = 0;
volatile unsigned long lastWebRequestAt = 0;
volatile bool lightSleepActive = false;
volatile unsigned long lastLightSleepAt = 0;
volatile unsigned long lightSleepCount = 0;
volatile bool wifiApActive = false;
volatile bool persistenceReady = false;
String wifiModeLabel = "Inicializando";
String activeWifiSsid = "";

SystemConfig systemConfig = {
    300,
    7000,
    true,
    DEFAULT_WIFI_SSID,
    DEFAULT_WIFI_PASSWORD
};

FunctionPerf performanceData[5] = {
    {"SensorTask", 0, 0, 0, 0},
    {"ControlTask", 0, 0, 0, 0},
    {"API /api/status", 0, 0, 0, 0},
    {"API /api/logs", 0, 0, 0, 0},
    {"API /api/control", 0, 0, 0, 0}
};

unsigned long memoryHistory[PERFORMANCE_HISTORY_SIZE] = {0};
unsigned int memoryHistorySize = 0;
unsigned int memoryHistoryIndex = 0;
std::vector<MetricSample> metricSamples;
std::vector<SystemLog> systemLogs;
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t controlTaskHandle = NULL;

extern SemaphoreHandle_t logMutex;

static bool fsReady() {
    return persistenceReady;
}

static String csvEscape(const String& value) {
    String escaped = value;
    escaped.replace("\"", "\"\"");
    return "\"" + escaped + "\"";
}

static void rotateFileIfNeeded(const char* path, size_t maxBytes) {
    if (!fsReady() || !LittleFS.exists(path)) return;
    File file = LittleFS.open(path, "r");
    if (!file) return;
    size_t size = file.size();
    file.close();
    if (size <= maxBytes) return;

    String backup = String(path) + ".old";
    if (LittleFS.exists(backup.c_str())) LittleFS.remove(backup.c_str());
    LittleFS.rename(path, backup.c_str());
}

void loadSystemConfig() {
    Preferences prefs;
    prefs.begin("cybergate", true);
    systemConfig.sensorIntervalMs = prefs.getULong("sensor_ms", 300);
    systemConfig.gateAutoCloseMs = prefs.getULong("close_ms", 7000);
    systemConfig.lightSleepEnabled = prefs.getBool("sleep", true);
    systemConfig.wifiSsid = prefs.getString("ssid", DEFAULT_WIFI_SSID);
    systemConfig.wifiPassword = prefs.getString("pass", DEFAULT_WIFI_PASSWORD);
    uint8_t wifiVersion = prefs.getUChar("wifi_ver", 0);
    prefs.end();

    systemConfig.sensorIntervalMs = constrain(systemConfig.sensorIntervalMs, 100UL, 10000UL);
    systemConfig.gateAutoCloseMs = constrain(systemConfig.gateAutoCloseMs, 1000UL, 60000UL);

    if (wifiVersion < DEFAULT_WIFI_VERSION || systemConfig.wifiSsid.length() == 0) {
        systemConfig.wifiSsid = DEFAULT_WIFI_SSID;
        systemConfig.wifiPassword = DEFAULT_WIFI_PASSWORD;
        saveSystemConfig();
    }
}

void saveSystemConfig() {
    Preferences prefs;
    prefs.begin("cybergate", false);
    prefs.putULong("sensor_ms", systemConfig.sensorIntervalMs);
    prefs.putULong("close_ms", systemConfig.gateAutoCloseMs);
    prefs.putBool("sleep", systemConfig.lightSleepEnabled);
    prefs.putString("ssid", systemConfig.wifiSsid);
    prefs.putString("pass", systemConfig.wifiPassword);
    prefs.putUChar("wifi_ver", DEFAULT_WIFI_VERSION);
    prefs.end();
}

void setWifiCredentials(const String& ssid, const String& password) {
    systemConfig.wifiSsid = ssid;
    systemConfig.wifiPassword = password;
    saveSystemConfig();
}

String maskedWifiSsid() {
    if (systemConfig.wifiSsid.length() == 0) return "";
    if (systemConfig.wifiSsid.length() <= 2) return "**";
    return systemConfig.wifiSsid.substring(0, 2) + "***";
}

void recordFunctionPerf(uint8_t index, unsigned long durationUs) {
    if (index >= 5) return;
    FunctionPerf &m = performanceData[index];
    m.callCount += 1;
    m.lastTimeUs = durationUs;
    m.totalTimeUs += durationUs;
    if (durationUs > m.maxTimeUs) m.maxTimeUs = durationUs;
}

float getEstimatedCpuLoadPct() {
    float sensorLoad = performanceData[0].callCount
        ? (float)performanceData[0].totalTimeUs / performanceData[0].callCount / (float)(systemConfig.sensorIntervalMs * 1000UL)
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

void pushMemorySample(unsigned int heapFree) {
    memoryHistory[memoryHistoryIndex] = heapFree;
    memoryHistoryIndex = (memoryHistoryIndex + 1) % PERFORMANCE_HISTORY_SIZE;
    if (memoryHistorySize < PERFORMANCE_HISTORY_SIZE) memoryHistorySize++;
}

void setServoTarget(int angle) {
    servoTargetAngle = constrain(angle, 0, 180);
}

void stepServoTarget(int delta) {
    setServoTarget(servoTargetAngle + delta);
}

void initPersistence() {
    persistenceReady = true;
    if (!LittleFS.exists(LOG_FILE)) {
        File file = LittleFS.open(LOG_FILE, "w");
        if (file) {
            file.println("timestamp,type,message");
            file.close();
        }
    }
    if (!LittleFS.exists(METRICS_FILE)) {
        File file = LittleFS.open(METRICS_FILE, "w");
        if (file) {
            file.println("timestamp,heapFree,distanceCm,cpuLoadPct");
            file.close();
        }
    }
    loadPersistedLogs();
    File metrics = LittleFS.open(METRICS_FILE, "r");
    if (metrics) {
        metricSamples.clear();
        bool header = true;
        while (metrics.available()) {
            String line = metrics.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue;
            if (header) {
                header = false;
                if (line.startsWith("timestamp,")) continue;
            }

            int p1 = line.indexOf(',');
            int p2 = line.indexOf(',', p1 + 1);
            int p3 = line.indexOf(',', p2 + 1);
            if (p1 < 0 || p2 < 0 || p3 < 0) continue;

            MetricSample sample;
            sample.timestamp = strtoul(line.substring(0, p1).c_str(), NULL, 10);
            sample.heapFree = strtoul(line.substring(p1 + 1, p2).c_str(), NULL, 10);
            sample.distanceCm = line.substring(p2 + 1, p3).toFloat();
            sample.cpuLoadPct = line.substring(p3 + 1).toFloat();
            if (metricSamples.size() >= PERFORMANCE_HISTORY_SIZE) metricSamples.erase(metricSamples.begin());
            metricSamples.push_back(sample);
        }
        metrics.close();
    }
}

void loadPersistedLogs() {
    if (!fsReady() || !LittleFS.exists(LOG_FILE)) return;
    File file = LittleFS.open(LOG_FILE, "r");
    if (!file) return;

    if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(200))) {
        systemLogs.clear();
        bool header = true;
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue;
            if (header) {
                header = false;
                if (line.startsWith("timestamp,")) continue;
            }

            int p1 = line.indexOf(',');
            int p2 = line.indexOf(',', p1 + 1);
            if (p1 < 0 || p2 < 0) continue;

            SystemLog item;
            item.timestamp = strtoul(line.substring(0, p1).c_str(), NULL, 10);
            item.type = line.substring(p1 + 1, p2);
            item.message = line.substring(p2 + 1);
            item.message.replace("\"", "");

            if (millis() - item.timestamp <= LOG_RETENTION_MS || item.timestamp > millis()) {
                if (systemLogs.size() >= MAX_LOG_CACHE) systemLogs.erase(systemLogs.begin());
                systemLogs.push_back(item);
            }
        }
        xSemaphoreGive(logMutex);
    }
    file.close();
}

void appendPersistedLog(const SystemLog& logItem) {
    if (!fsReady()) return;
    rotateFileIfNeeded(LOG_FILE, MAX_LOG_FILE_BYTES);
    File file = LittleFS.open(LOG_FILE, "a");
    if (!file) return;
    file.print(logItem.timestamp);
    file.print(',');
    file.print(logItem.type);
    file.print(',');
    file.println(csvEscape(logItem.message));
    file.close();
}

String buildLogsCsv() {
    String csv = "timestamp,type,message\n";
    if (fsReady()) {
        csv = "";
        const char* files[] = {"/logs.csv.old", LOG_FILE};
        for (const char* path : files) {
            if (!LittleFS.exists(path)) continue;
            File file = LittleFS.open(path, "r");
            if (!file) continue;
            String content = file.readString();
            file.close();
            if (csv.length() == 0) {
                csv = content;
            } else {
                int headerEnd = content.indexOf('\n');
                csv += headerEnd >= 0 ? content.substring(headerEnd + 1) : content;
            }
        }
        if (csv.length() > 0) {
            return csv;
        }
    }

    if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(200))) {
        for (const auto& logItem : systemLogs) {
            csv += String(logItem.timestamp) + "," + logItem.type + "," + csvEscape(logItem.message) + "\n";
        }
        xSemaphoreGive(logMutex);
    }
    return csv;
}

void persistMetricSample(unsigned int heapFree, float distanceCm, float cpuLoadPct) {
    MetricSample sample;
    sample.timestamp = millis();
    sample.heapFree = heapFree;
    sample.distanceCm = distanceCm;
    sample.cpuLoadPct = cpuLoadPct;

    metricSamples.push_back(sample);
    if (metricSamples.size() > PERFORMANCE_HISTORY_SIZE) {
        metricSamples.erase(metricSamples.begin());
    }

    if (!fsReady()) return;
    rotateFileIfNeeded(METRICS_FILE, MAX_METRICS_FILE_BYTES);
    File file = LittleFS.open(METRICS_FILE, "a");
    if (!file) return;
    file.print(sample.timestamp);
    file.print(',');
    file.print(sample.heapFree);
    file.print(',');
    file.print(sample.distanceCm, 1);
    file.print(',');
    file.println(sample.cpuLoadPct, 2);
    file.close();
}

void prunePersistence() {
    rotateFileIfNeeded(LOG_FILE, MAX_LOG_FILE_BYTES);
    rotateFileIfNeeded(METRICS_FILE, MAX_METRICS_FILE_BYTES);
}

void addLog(String type, String message) {
    SystemLog newLog;
    newLog.timestamp = millis();
    newLog.type = type;
    newLog.message = message;

    if (logMutex && xSemaphoreTake(logMutex, pdMS_TO_TICKS(200))) {
        if (systemLogs.size() >= MAX_LOG_CACHE) {
            systemLogs.erase(systemLogs.begin());
        }
        systemLogs.push_back(newLog);
        xSemaphoreGive(logMutex);
    }

    appendPersistedLog(newLog);
    Serial.printf("[%s] %s\n", type.c_str(), message.c_str());
}
