# Projeto 1: Controle de LED via MQTT 💡

Este projeto implementa um sistema de controle de LED utilizando o protocolo MQTT. Foi desenvolvido como atividade prática utilizando o framework ESP-IDF, demonstrando conceitos de *Publish/Subscribe*, manipulação de GPIOs e conectividade Wi-Fi.

## 📋 Arquitetura do Sistema

O sistema é composto por três partes principais operando na mesma rede local:

1. **Broker MQTT (Mosquitto):** Servidor central responsável por rotear as mensagens (IP configurável).
2. **Node A (Interruptor/Publisher):** Lê o estado de um botão físico e publica comandos de ligar/desligar.
3. **Node B (Atuador/Subscriber):** Assina o tópico de controle e aciona um LED físico ao receber os comandos.

## 🛠️ Hardware Necessário

* 2x Placas de Desenvolvimento ESP32
* 1x Push-button
* 1x Resistor de 220Ω (Pull-up para o botão)
* 1x LED
* 1x Resistor de 220Ω (Limitador de corrente para o LED)
* Protoboards e Jumpers

### Pinagem Utilizada
| Componente | Nó (Node) | Pino ESP32 |
| :--- | :--- | :--- |
| Botão | Node A | GPIO 4 |
| LED | Node B | GPIO 4 |

## Software e Dependências

* **Framework:** [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
* **Protocolo:** Wi-Fi (Modo Station) e MQTT (via `espressif/mqtt`)
* **Broker:** [Eclipse Mosquitto](https://mosquitto.org/) rodando localmente.

## Interface MQTT (Tópicos e Payloads)

| Tópico | Direção | Payload | Descrição |
| :--- | :--- | :--- | :--- |
| `ifpb/projeto/led` | A ➡️ Broker ➡️ B | `"ON"` / `"OFF"` | Comando para alterar o estado do LED. |
| `ifpb/projeto/status` | A, B ➡️ Broker | `"Uptime: X segundos"` | Mensagem de *Keep Alive* enviada a cada 30s. |
