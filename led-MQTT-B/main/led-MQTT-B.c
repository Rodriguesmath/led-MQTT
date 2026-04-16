#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h" 
#include "mqtt_client.h"
#include "esp_log.h"

/* --- Hardware Abstraction Layer (HAL) Definitions --- */
#define PIN_LED 4 // Usando o GPIO 4
#define GPIO_OUTPUT_LED_BITMASK  ((1ULL << PIN_LED))

/* --- Timing Constants --- */
#define KEEP_ALIVE_INTERVAL_MS 30000

/* --- MQTT Global Variables --- */
#define BROKER_URL "mqtt://192.168.12.1"  
#define MQTT_TOPIC "ifpb/projeto/led"

/* --- Private Global Variables --- */
static const char *TAG = "MQTT_NODE_B";
static TickType_t g_last_keep_alive_tick = 0;

/* --- Module Functions --- */

/**
 * @brief Inicializa os periféricos GPIO (LED como Saída).
 */
void bsp_gpio_init(void) {
    gpio_config_t led_cfg = {
        .pin_bit_mask = GPIO_OUTPUT_LED_BITMASK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, 
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_cfg);
    
    // Garante que o LED inicie desligado
    gpio_set_level(PIN_LED, 0); 
}

/**
 * @brief Manipulador de Eventos MQTT
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado ao Broker!");
            // Inscreve-se no tópico imediatamente após a conexão
            esp_mqtt_client_subscribe(event->client, MQTT_TOPIC, 0);
            ESP_LOGI(TAG, "Inscrito no tópico: %s", MQTT_TOPIC);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MENSAGEM RECEBIDA!");
            printf("Tópico: %.*s\r\n", event->topic_len, event->topic);
            printf("Dados: %.*s\r\n", event->data_len, event->data);

            // Usando strncmp porque as strings no MQTT da ESP não terminam com '\0' (null-terminated)
            if (strncmp(event->topic, MQTT_TOPIC, event->topic_len) == 0) {
                
                // 3. Compara o Payload para ligar ou desligar o LED
                if (strncmp(event->data, "ON", event->data_len) == 0) {
                    gpio_set_level(PIN_LED, 1);
                    ESP_LOGI(TAG, "Ação Executada: LED LIGADO");
                } 
                else if (strncmp(event->data, "OFF", event->data_len) == 0) {
                    gpio_set_level(PIN_LED, 0);
                    ESP_LOGI(TAG, "Ação Executada: LED DESLIGADO");
                }
                else {
                    ESP_LOGW(TAG, "Comando desconhecido recebido.");
                }
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Desconectado");
            break;
            
        default:
            break;
    }
}

void app_main(void) {
    // Inicialização do Sistema Base
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Conexão Wi-Fi
    ESP_ERROR_CHECK(example_connect());

    // Inicialização do Hardware (LED)
    bsp_gpio_init();

    // Configuração e Inicialização do Cliente MQTT
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_URL,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    ESP_LOGI(TAG, "Nó B iniciado. Aguardando mensagens...");

    // Como tudo acontece via interrupção/evento de rede, 
    // a task principal pode apenas ficar "dormindo"
    while (1) {
        TickType_t current_tick = xTaskGetTickCount();
        
        if ((current_tick - g_last_keep_alive_tick) >= pdMS_TO_TICKS(KEEP_ALIVE_INTERVAL_MS)) {
            g_last_keep_alive_tick = current_tick; 
            
            uint32_t uptime_sec = (current_tick * portTICK_PERIOD_MS) / 1000;
            
            char status_payload[64];
            snprintf(status_payload, sizeof(status_payload), "Node B - Uptime: %lu segundos", (unsigned long)uptime_sec);
            
            esp_mqtt_client_publish(client, "ifpb/projeto/status", status_payload, 0, 1, 0);
            
            ESP_LOGI(TAG, "Node B - Keep Alive enviado: %s", status_payload);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}