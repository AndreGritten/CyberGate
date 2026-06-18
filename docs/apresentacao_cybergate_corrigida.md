# CyberGate
## Sistema ciberfisico de controle de acesso com ESP32

**Disciplina:** Performance em Sistemas Ciberfisicos  
**Instituicao:** PUCPR  
**Equipe:** Andre Luiz da Silva Gritten, Mateus Furtado dos Santos, Joao Carlos de Souza Coelho Mezari, Gustavo Isdra Correa

---

# Objetivo do Projeto

Implementar e validar um sistema ciberfisico de controle de acesso para garagem, com deteccao de veiculo, autenticacao RFID, acionamento de portao e observabilidade embarcada.

O projeto busca aplicar conceitos de performance em sistemas ciberfisicos usando ESP32, FreeRTOS, sensores fisicos, atuador, interface web e persistencia local de eventos.

---

# Escopo Implementado

- Deteccao de presenca por sensor ultrassonico HC-SR04.
- Leitura RFID com modulo RC522/MFRC522.
- Validacao de tag autorizada diretamente no firmware.
- Acionamento de servo motor para abertura e fechamento do portao.
- Interface web embarcada no ESP32.
- API REST para status, controle, logs, configuracao e metricas.
- Persistencia local com LittleFS.
- Monitoramento de heap, tempo de execucao, stack das tasks, rede e energia.

---

# Hardware Utilizado

- **ESP32:** microcontrolador dual core com Wi-Fi integrado.
- **HC-SR04:** sensor ultrassonico para medir proximidade do veiculo.
- **RC522/MFRC522:** leitor RFID para autenticacao por tag.
- **Servo motor MG996R ou equivalente:** atuador mecanico do portao.
- **LED de estado:** indicacao local do estado do portao.
- **Estrutura fisica:** maquete ou mecanismo de portao com suporte ao servo.

---

# Arquitetura Real do Firmware

O firmware utiliza FreeRTOS, mas a implementacao atual concentra a execucao em duas tasks principais:

- **SensorTask - Core 1:** leitura ultrassonica, ativacao condicional do RFID, validacao de UID e comando de abertura.
- **ControlTask - Core 0:** controle gradual do servo, auto-fechamento, coleta de metricas e persistencia periodica.
- **AsyncWebServer - Wi-Fi/Async:** servidor HTTP embarcado para interface web e API REST.
- **Loop principal:** monitoramento de Wi-Fi e entrada em Light Sleep quando o sistema esta ocioso.

Arquivos vazios como `rfid_task.cpp`, `web_task.cpp` e `log_task.cpp` existem na estrutura, mas nao representam tasks ativas no firmware atual.

---

# Fluxo de Operacao

1. O sensor ultrassonico mede a distancia em amostras e usa a mediana para reduzir leituras instaveis.
2. Quando a distancia fica abaixo do limiar configurado, o sistema confirma a presenca do veiculo.
3. O RFID so fica ativo quando ha veiculo detectado, portao fechado e modo de calibracao desligado.
4. Ao ler uma tag, o firmware compara o UID com a lista autorizada.
5. Se autorizado, o sistema registra o evento, abre o portao e move o servo ate o angulo configurado.
6. Depois do tempo de auto-fechamento, o portao fecha automaticamente.

---

# Sensores e Atuador

## Ultrassonico

- Pinos usados: TRIG no GPIO 26 e ECHO no GPIO 27.
- Leitura por interrupcao no ECHO.
- Timeout de leitura para evitar bloqueios longos.
- Filtro por mediana com tres amostras.

## RFID

- Modulo RC522 usando SPI.
- UID autorizado configurado no firmware.
- Registro de tentativa autorizada ou negada.

## Servo

- Servo no GPIO 13.
- Movimento gradual em passos de 1 grau.
- Angulos aberto/fechado persistidos via Preferences.

---

# Interface Web Embarcada

A interface web fica hospedada no LittleFS do ESP32 e e servida pelo `ESPAsyncWebServer`.

Paginas disponiveis:

- Visao geral do sistema.
- Controle remoto do portao.
- Logs recentes e exportacao CSV.
- Painel de performance.
- Configuracao operacional e Wi-Fi.
- Informacoes do projeto e changelog.

A comunicacao atual e feita por **API REST com polling periodico**, nao por WebSocket.

---

# API REST

Principais endpoints implementados:

- `GET /api/status`: estado do portao, sensor, RFID, memoria e configuracoes ativas.
- `POST /api/control`: abertura, fechamento, calibracao e teste do servo.
- `GET /api/logs`: logs recentes em JSON.
- `GET /api/logs.csv` e `GET /api/logs/export`: exportacao CSV.
- `GET /api/config` e `POST /api/config`: parametros operacionais.
- `POST /api/wifi`: novas credenciais Wi-Fi.
- `GET /api/performance`: metricas de desempenho, memoria, Wi-Fi, tasks e sensores.

---

# Observabilidade e Persistencia

O sistema registra eventos de sensor, RFID, controle, rede, configuracao e erros.

Metricas monitoradas:

- Uptime.
- Heap livre, heap minimo, maior bloco alocavel e PSRAM.
- Uso estimado de CPU.
- Tempo de execucao medio, ultimo e maximo de funcoes instrumentadas.
- Stack livre das tasks principais.
- Estado de Wi-Fi, RSSI, IP, modo AP/STA e clientes conectados.
- Distancia medida, RFID ativo e estado do portao.

Logs e metricas sao persistidos no LittleFS, com rotacao por tamanho e retencao em memoria dos eventos recentes.

---

# Rede e Energia

## Rede

- O ESP32 tenta conectar ao Wi-Fi salvo.
- Se nao houver rede configurada ou a conexao falhar, abre o AP de configuracao `CyberGate-Setup`.
- A interface permite atualizar as credenciais Wi-Fi.

## Energia

- O firmware implementa Light Sleep quando o sistema esta ocioso.
- A entrada em Light Sleep e bloqueada se houver veiculo detectado, portao aberto, RFID ativo, calibracao em andamento, movimento de servo ou acesso web recente.
- O despertar atual e configurado por timer, nao por interrupcao externa de GPIO.

---

# Validacao do Projeto

Validacoes implementadas ou previstas no repositorio:

- Teste isolado do sensor ultrassonico.
- Teste isolado do RFID.
- Teste isolado do servo.
- Verificacao do fluxo integrado: detectar veiculo, autenticar tag e abrir portao.
- Monitoramento pela interface web durante execucao.
- Exportacao de logs para analise posterior.

Resultados quantitativos, como operacao continua por 24 horas ou ausencia de vazamento de memoria, devem ser apresentados somente quando acompanhados por logs ou medicoes coletadas durante o teste.

---

# Limitacoes Atuais

- A lista de UIDs autorizados esta fixa no firmware.
- A comunicacao da interface usa polling REST; WebSocket nao esta implementado.
- Existem arquivos estruturais vazios para services/tasks futuras.
- Nao ha banco de dados externo.
- O Light Sleep acorda por timer, nao por interrupcao do sensor.
- A estimativa de CPU e aproximada, baseada nos tempos instrumentados das funcoes.

Esses pontos nao impedem o funcionamento do prototipo, mas devem ser descritos corretamente na apresentacao.

---

# Conclusao

O CyberGate implementa um sistema ciberfisico funcional usando ESP32, FreeRTOS, sensor ultrassonico, RFID, servo motor, interface web embarcada e persistencia local.

A arquitetura atual prioriza simplicidade operacional: duas tasks principais, servidor web assincrono, API REST e metricas integradas. O projeto atende ao objetivo academico de demonstrar controle fisico, concorrencia, observabilidade e analise de desempenho em um sistema embarcado de baixo custo.

