# 🚗 Sistema Inteligente de Controle de Acesso para Garagem

Este projeto consiste no desenvolvimento de um sistema ciberfísico baseado em ESP32 para controle de acesso em portões de garagem.

O sistema detecta a aproximação de veículos, ativa autenticação via RFID e realiza a validação de acesso. Caso autorizado, o portão é aberto automaticamente.

Além disso, o sistema possui uma interface web embarcada que permite monitoramento em tempo real, controle remoto e análise de desempenho.

---

## 🎯 Objetivos

- Aplicar conceitos de sistemas ciberfísicos
- Implementar multitarefa com FreeRTOS
- Monitorar desempenho (CPU, memória, tempo de execução)
- Registrar logs e eventos do sistema
- Disponibilizar interface web embarcada

---

## ⚙️ Funcionalidades

- Detecção de presença de veículo
- Autenticação via RFID
- Abertura automática do portão
- Controle remoto via interface web
- Monitoramento de métricas de desempenho
- Registro de logs (info, warning, erro)
- Persistência de dados (mínimo 24h)

---

## 🧱 Arquitetura

O sistema é baseado em múltiplas tasks:

- Leitura de sensores
- Autenticação RFID
- Controle de atuadores
- Servidor web
- Logging e persistência

---

## 🌐 Interface Web

Permite:

- Visualizar estado do sistema
- Acompanhar métricas de desempenho
- Acessar logs
- Controlar o portão remotamente
- Configurar parâmetros do sistema

---

## 🧪 Status do Projeto

- [x] Estrutura inicial
- [x] Servidor web básico
- [ ] Integração com sensores
- [ ] Integração com atuadores
- [ ] Métricas completas de performance
- [ ] Persistência de dados

---

## 🛠️ Tecnologias

- ESP32
- C/C++
- FreeRTOS
- ESPAsyncWebServer
- LittleFS / SPIFFS

---

## 👨‍💻 Equipe

- André Gritten
- Mateus Furtado
- João Mezari
- Gustavo Isdra

---

## 📚 Disciplina

Performance em Sistemas Ciberfísicos  
PUCPR
