#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "../system_state.h"

// Instância principal do servidor web rodando na porta 80 do ESP32
AsyncWebServer server(80);

// Declaração da função que está no routes.cpp
void setupRoutes(AsyncWebServer* srv);

void initWebServer() {
    addLog("SISTEMA", "Inicializando sistema de arquivos LittleFS...");
    
    // Inicia o LittleFS (onde ficam nossos arquivos .html, .css, .js)
    if (!LittleFS.begin(true)) {
        addLog("ERRO", "Falha ao montar o LittleFS. As páginas web não vão funcionar.");
        return;
    }
    addLog("SISTEMA", "LittleFS inicializado com sucesso.");

    // Configura o servidor para servir arquivos estáticos de forma mágica.
    // Tudo que estiver no root '/' do LittleFS poderá ser acessado pelo navegador.
    // A configuração "default" carrega o index.html quando acessamos a raiz.
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    addLog("SISTEMA", "Configurando rotas da API REST...");
    // Vamos chamar a função separada para manter a organização Modular
    setupRoutes(&server);

    // Tratamento genérico para erros 404 (Página não encontrada)
    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "application/json", "{\"error\":\"Not found\"}");
    });

    // Inicia o servidor efetivamente
    server.begin();
    addLog("WEB", "Servidor HTTP rodando na porta 80.");
}
