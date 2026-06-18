#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <vector>

constexpr unsigned long LOG_RETENTION_MS = 24UL * 60UL * 60UL * 1000UL;
constexpr int PERFORMANCE_HISTORY_SIZE = 96;

struct SystemLog {
    unsigned long timestamp;
    String type;
    String message;
};

struct MetricSample {
    unsigned long timestamp;
    unsigned long heapFree;
    float distanceCm;
    float cpuLoadPct;
};

struct SystemConfig {
    unsigned long sensorIntervalMs;
    unsigned long gateAutoCloseMs;
    bool lightSleepEnabled;
    String wifiSsid;
    String wifiPassword;
};

extern volatile bool isGateOpen;
extern volatile bool vehicleDetected;
extern volatile bool rfidActive;
extern volatile unsigned long gateOpenedAt;
extern volatile int servoCurrentAngle;
extern volatile int servoTargetAngle;
extern volatile int servoOpenAngle;
extern volatile int servoClosedAngle;
extern volatile bool servoCalibrationMode;
extern volatile bool saveServoOpenRequested;
extern volatile bool saveServoClosedRequested;
extern volatile float lastDistanceCm;
extern String lastRfidUid;
extern String lastRfidStatus;
extern String lastRfidMessage;
extern volatile unsigned long lastRfidAt;

extern volatile unsigned long totalUptime;
extern volatile unsigned int freeHeapMemory;
extern volatile unsigned long lastRequestTimeMs;
extern volatile unsigned long lastWebRequestAt;
extern volatile bool lightSleepActive;
extern volatile unsigned long lastLightSleepAt;
extern volatile unsigned long lightSleepCount;
extern volatile bool wifiApActive;
extern volatile bool persistenceReady;
extern String wifiModeLabel;
extern String activeWifiSsid;

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
extern std::vector<MetricSample> metricSamples;
extern SystemConfig systemConfig;
extern TaskHandle_t sensorTaskHandle;
extern TaskHandle_t controlTaskHandle;
extern SemaphoreHandle_t logMutex;

extern std::vector<SystemLog> systemLogs;

void recordFunctionPerf(uint8_t index, unsigned long durationUs);
float getEstimatedCpuLoadPct();
void pushMemorySample(unsigned int heapFree);
void setServoTarget(int angle);
void stepServoTarget(int delta);

void loadSystemConfig();
void saveSystemConfig();
void setWifiCredentials(const String& ssid, const String& password);
String maskedWifiSsid();

void initPersistence();
void loadPersistedLogs();
void appendPersistedLog(const SystemLog& logItem);
String buildLogsCsv();
void persistMetricSample(unsigned int heapFree, float distanceCm, float cpuLoadPct);
void prunePersistence();

void addLog(String type, String message);

#endif
