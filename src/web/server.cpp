#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "../system_state.h"

AsyncWebServer server(80);

void setupRoutes(AsyncWebServer* srv);

void initWebServer() {
    addLog("SISTEMA", "Inicializando sistema de arquivos LittleFS...");

    if (!LittleFS.begin(true)) {
        persistenceReady = false;
        addLog("ERRO", "Falha ao montar LittleFS. Interface web indisponivel.");
        return;
    }

    initPersistence();
    addLog("SISTEMA", "LittleFS inicializado com persistencia de 24h.");

    addLog("SISTEMA", "Configurando rotas da API REST...");
    setupRoutes(&server);

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "application/json", "{\"error\":\"Not found\"}");
    });

    server.begin();
    addLog("WEB", "Servidor HTTP rodando na porta 80.");
}
