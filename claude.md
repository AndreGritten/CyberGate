# Claude Context - Smart Garage System

## 📌 Project Overview
This project is an ESP32-based cyber-physical system for garage access control.

Main features:
- Vehicle detection (sensor)
- RFID authentication
- Gate control (servo)
- Web interface for monitoring and control
- Performance monitoring (CPU, memory, execution time)

---

## 🧠 Architecture

The system uses FreeRTOS with multiple tasks:

- sensor_task → reads proximity sensor
- rfid_task → handles authentication
- control_task → controls gate actuator
- web_task → serves web interface
- log_task → logs events and metrics

---

## ⚙️ Coding Rules

- Do NOT use delay()
- Prefer non-blocking code
- Use FreeRTOS tasks for concurrency
- Use interrupts for real-time events
- Keep functions small and modular

---

## 📊 Performance Requirements

The system must track:

- CPU usage
- Memory usage (heap/stack)
- Execution time of key functions
- Task states

---

## 🌐 Web Server

- Use ESPAsyncWebServer
- Provide endpoints:
  - /status
  - /metrics
  - /logs
  - /control

---

## 💾 Persistence

- Use LittleFS or SPIFFS
- Store logs for at least 24h

---

## 🚫 Avoid

- Blocking loops
- Long synchronous operations
- Hardcoded delays
- Monolithic code

---

## 🎯 Goal

The focus is NOT just functionality.
The system must demonstrate performance, observability, and concurrency.