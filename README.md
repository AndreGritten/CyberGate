# CyberGate

Sistema ciberfisico de controle de acesso para garagem baseado em ESP32.

O CyberGate detecta a aproximacao de um veiculo com sensor ultrassonico, ativa a autenticacao RFID somente quando necessario e aciona um servo motor para abrir o portao quando a tag e autorizada. O projeto tambem oferece uma interface web embarcada para monitoramento, controle remoto, configuracao, logs e metricas de desempenho.

---

## Objetivos

- Aplicar conceitos de sistemas ciberfisicos em um prototipo funcional.
- Usar FreeRTOS no ESP32 com separacao de responsabilidades entre tasks.
- Integrar sensores, atuador, rede Wi-Fi e interface web embarcada.
- Monitorar desempenho, memoria, tempo de execucao, rede e estado dos sensores.
- Registrar e persistir eventos do sistema para analise posterior.

---

## Funcionalidades Implementadas

- Deteccao de veiculo com sensor ultrassonico HC-SR04.
- Leitura RFID com modulo RC522/MFRC522.
- Validacao de UID autorizado diretamente no firmware.
- Abertura automatica do portao por servo motor.
- Fechamento automatico apos tempo configuravel.
- Controle remoto do portao pela interface web.
- Calibracao dos angulos aberto/fechado do servo.
- Interface web embarcada no ESP32 via LittleFS.
- API REST para status, controle, logs, configuracao, Wi-Fi e performance.
- Logs estruturados em memoria e persistidos em LittleFS.
- Exportacao de logs em CSV.
- Metricas de heap, CPU estimada, stack das tasks, uptime, Wi-Fi, RSSI e sensores.
- Modo AP de configuracao quando nao ha Wi-Fi salvo ou a conexao falha.
- Light Sleep quando o sistema esta ocioso.

---

## Hardware

- ESP32.
- Sensor ultrassonico HC-SR04.
- Leitor RFID RC522/MFRC522.
- Servo motor MG996R ou equivalente.
- LED de estado.
- Estrutura fisica ou maquete de portao/cancela.

---

## Arquitetura do Firmware

A implementacao atual usa duas tasks principais e o servidor web assincrono:

| Componente | Nucleo | Responsabilidade |
| --- | --- | --- |
| `SensorTask` | Core 1 | Leitura do HC-SR04, ativacao condicional do RFID, leitura/validacao de UID e comando de abertura |
| `ControlTask` | Core 0 | Controle gradual do servo, auto-fechamento, coleta de metricas e persistencia periodica |
| `AsyncWebServer` | Wi-Fi/Async | Servidor HTTP embarcado, arquivos HTML/CSS/JS no LittleFS e API REST |
| `loop()` | Principal | Monitoramento de Wi-Fi e entrada em Light Sleep quando ocioso |

Observacao: arquivos como `rfid_task.cpp`, `web_task.cpp` e `log_task.cpp` existem na estrutura, mas nao representam tasks ativas no firmware atual.

---

## Fluxo de Operacao

1. O HC-SR04 mede a distancia usando interrupcao no pino ECHO.
2. A leitura usa timeout e mediana de amostras para reduzir instabilidade.
3. Quando um veiculo e confirmado, o RFID e ativado.
4. O UID da tag e comparado com a lista autorizada no firmware.
5. Se autorizado, o sistema registra o evento e abre o portao.
6. A `ControlTask` move o servo gradualmente ate o angulo aberto.
7. Apos o tempo configurado, o portao fecha automaticamente.

---

## Interface Web

A interface web e servida pelo proprio ESP32 e permite:

- Visualizar estado do portao, sensor ultrassonico e RFID.
- Abrir e fechar o portao remotamente.
- Calibrar posicoes do servo.
- Consultar logs recentes.
- Exportar logs em CSV.
- Acompanhar metricas de performance.
- Configurar intervalo do sensor, tempo de auto-fechamento e Light Sleep.
- Configurar credenciais Wi-Fi.

A comunicacao atual usa API REST com polling periodico via `fetch`/`setInterval`. O firmware atual nao utiliza WebSocket.

---

## API REST

| Endpoint | Metodo | Descricao |
| --- | --- | --- |
| `/api/status` | GET | Estado atual do portao, sensores, RFID, Wi-Fi e memoria |
| `/api/control` | POST | Comandos de abertura, fechamento, teste e calibracao do servo |
| `/api/logs` | GET | Logs recentes em JSON |
| `/api/logs.csv` | GET | Exportacao de logs em CSV |
| `/api/logs/export` | GET | Exportacao alternativa de logs em CSV |
| `/api/config` | GET/POST | Leitura e atualizacao de parametros operacionais |
| `/api/wifi` | POST | Atualizacao das credenciais Wi-Fi |
| `/api/performance` | GET | Metricas de desempenho, memoria, rede, tasks e sensores |

---

## Observabilidade e Persistencia

O sistema registra eventos de:

- Sistema.
- Sensor.
- RFID.
- Controle.
- Usuario.
- Rede.
- Configuracao.
- Erros.

Metricas monitoradas:

- Uptime.
- Heap livre, heap minimo e maior bloco alocavel.
- PSRAM e flash.
- Uso estimado de CPU.
- Tempo ultimo, medio e maximo das funcoes instrumentadas.
- Stack livre da `SensorTask` e `ControlTask`.
- Estado do Wi-Fi, RSSI, IP e modo AP/STA.
- Distancia medida, RFID ativo e estado do portao.
- Uso do LittleFS.

Logs e metricas sao armazenados em LittleFS, com rotacao por tamanho. A retencao em memoria cobre os eventos recentes e a constante de retencao de logs e de 24 horas.

---

## Rede e Energia

- O ESP32 tenta conectar ao Wi-Fi salvo em `Preferences`.
- Se a conexao falhar ou nao houver credenciais, abre o AP `CyberGate-Setup`.
- A interface permite salvar novas credenciais Wi-Fi.
- O Light Sleep e habilitado quando o sistema esta ocioso.
- A entrada em Light Sleep e bloqueada quando ha veiculo detectado, portao aberto, RFID ativo, calibracao em andamento, movimento do servo ou acesso web recente.
- O despertar atual do Light Sleep e feito por timer.

---

## Limitacoes Atuais

- Os UIDs autorizados estao fixos no firmware.
- Nao ha banco de dados externo.
- Nao ha WebSocket implementado.
- A estimativa de CPU e aproximada, baseada nas funcoes instrumentadas.
- Algumas pastas e arquivos de `services` e `tasks` estao reservados para evolucao futura, mas ainda nao possuem implementacao ativa.

---

## Tecnologias

- ESP32.
- Arduino/C++.
- FreeRTOS.
- ESPAsyncWebServer.
- AsyncTCP.
- ArduinoJson.
- LittleFS.
- Preferences.
- ESP32Servo.
- MFRC522.

---

## Arquivos Principais

- `CyberGate.ino`: inicializacao, Wi-Fi, criacao das tasks e Light Sleep.
- `src/tasks/sensor_task.cpp`: HC-SR04, RFID e decisao de abertura.
- `src/tasks/control_task.cpp`: servo, auto-fechamento, metricas e persistencia.
- `src/system_state.h` e `src/system_state.cpp`: estado global, configuracoes, logs, metricas e persistencia.
- `src/web/server.cpp`: inicializacao do LittleFS e servidor HTTP.
- `src/web/routes.cpp`: endpoints da API REST.
- `data/`: arquivos da interface web embarcada.
- `tests/`: sketches de teste isolado dos componentes.
- `docs/apresentacao_cybergate_corrigida.md`: roteiro corrigido da apresentacao.
- `docs/apresentacao_cybergate_corrigida.html`: apresentacao HTML corrigida.

---

## Status do Projeto

- [x] Estrutura inicial do firmware.
- [x] Leitura ultrassonica com HC-SR04.
- [x] Integracao com RFID RC522/MFRC522.
- [x] Controle de servo motor.
- [x] Interface web embarcada.
- [x] API REST.
- [x] Logs estruturados.
- [x] Persistencia em LittleFS.
- [x] Metricas de performance.
- [x] Configuracao de Wi-Fi e AP fallback.
- [x] Light Sleep por ociosidade.
- [ ] Cadastro dinamico de UIDs pela interface.
- [ ] WebSocket para atualizacao em tempo real sem polling.
- [ ] Separacao futura em services/tasks dedicadas, se necessario.

---

## Equipe

- Andre Gritten.
- Mateus Furtado.
- Joao Mezari.
- Gustavo Isdra.

---

## Disciplina

Performance em Sistemas Ciberfisicos  
PUCPR
