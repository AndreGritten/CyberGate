#include <Arduino.h>
#include <WiFi.h>

// Incluindo nossos módulos da pasta src
#include "src/system_state.h"

// Se estiver no Arduino IDE, as declaracoes das funções podem precisar existir aqui
// ou podemos criar os ".h" de cada. Simplificando via forward declaration:
extern void initWebServer();
extern void sensorTaskCode(void *pvParameters);
extern void controlTaskCode(void *pvParameters);

// Configuração da Rede WiFi (Ajuste para a sua rede)
const char* ssid = "gritten";
const char* password = "casa1802";

SemaphoreHandle_t logMutex;

void setup() {
    Serial.begin(115200);
    delay(10); // Apenas um pequeno respiro no início
    
    // Criação do Mutex de sistema para controlar acesso às variáveis em uso pelas Threads
    logMutex = xSemaphoreCreateMutex();

    addLog("SISTEMA", "Iniciando CyberGate Controller...");

    // 1. Conexão WiFi (Poderia ser Access Point se preferir, simplificado para Client)
    WiFi.begin(ssid, password);
    addLog("REDE", "Conectando ao WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    addLog("REDE", String("Conectado! IP do Servidor Web: ") + WiFi.localIP().toString());

    // 2. Inicia servidor web
    initWebServer();

    // 3. Orquestração FreeRTOS - Criação das Tasks
    // ----------------------------------------------------
    // Utilizamos o xTaskCreatePinnedToCore para decidir as prioridades e o núcleo.
    
    // Task 1: Sensor. Prioridade média (1) gerando leitura a cada instantes. Núcleo 1 (Padrão para rotinas customizadas)
    xTaskCreatePinnedToCore(
        sensorTaskCode,   /* Função da Task */
        "SensorTask",     /* Nome para debug */
        8192,             /* Tamanho da Pilha — aumentado para suportar SPI + MFRC522 */
        NULL,             /* Parâmetros. Nenhum. */
        1,                /* Prioridade baixa/Media */
        &sensorTaskHandle,
        1                 /* Núcleo do processador ESP32 (0 ou 1) */
    );
    
    // Task 2: Controle do Portão (Motor/Atuadores). Prioridade alta (2) para que responda rápido a comandos.
    xTaskCreatePinnedToCore(
        controlTaskCode,   
        "ControlTask",     
        4096,             
        &controlTaskHandle,
        2,                /* Prioridade alta que a do Sensor */
        NULL,             
        1                 
    );

    addLog("SISTEMA", "Setup concluído! Tasks estão rodando paralelamente.");
}

// O Loop principal do Arduino. Como usamos FreeRTOS Tasks separadas, esse loop nativo não fará nada
// Fica totalmente "Idddle"
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000)); 
}
