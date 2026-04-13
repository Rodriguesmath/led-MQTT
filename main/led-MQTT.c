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
#define GPIO_INPUT_BTN_BITMASK  ((1ULL << 4))
#define PIN_BTN   4 

/* --- Timing & Logic Constants --- */
#define BTN_DEBOUNCE_MS        50

typedef enum {
    BUTTON_STATE_IDLE,
    BUTTON_STATE_DEBOUNCE,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_WAIT_RELEASE
} button_state_t;

typedef struct {
    gpio_num_t gpio_num;
    button_state_t current_state;
    TickType_t debounce_start_tick;
} button_handle_t;

/* --- Private Global Variables --- */
static const char *TAG = "MQTT_TUTORIAL";
static button_handle_t g_btn_light_handle = { .gpio_num = PIN_BTN, .current_state = BUTTON_STATE_IDLE };
static bool g_light_state = false; 

/* --- MQTT Global Variables --- */
#define BROKER_URL "mqtt://172.20.10.2" 

/* --- Module Functions --- */

/**
 * @brief Inicializa os periféricos GPIO.
 */
void bsp_gpio_init(void) {
    // Configuração do Botão (Entrada)
    gpio_config_t btn_cfg = {
        .pin_bit_mask = GPIO_INPUT_BTN_BITMASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, 
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_cfg);

}

/**
 * @brief Gerencia a máquina de estados para debounce.
 * @return true se um evento de clique (pressionar) for detectado.
 */
bool button_poll_event(button_handle_t *handle) {
    int raw_level = gpio_get_level(handle->gpio_num);
    TickType_t current_tick = xTaskGetTickCount();
    bool is_triggered = false;

    switch (handle->current_state) {
        case BUTTON_STATE_IDLE:
            if (raw_level == 0) { 
                handle->debounce_start_tick = current_tick;
                handle->current_state = BUTTON_STATE_DEBOUNCE;
            }
            break;

        case BUTTON_STATE_DEBOUNCE:
            if ((current_tick - handle->debounce_start_tick) >= pdMS_TO_TICKS(BTN_DEBOUNCE_MS)) {
                if (gpio_get_level(handle->gpio_num) == 0) {
                    handle->current_state = BUTTON_STATE_PRESSED;
                    is_triggered = true;
                } else {
                    handle->current_state = BUTTON_STATE_IDLE;
                }
            }
            break;

        case BUTTON_STATE_PRESSED:
            if (raw_level == 1) { 
                handle->current_state = BUTTON_STATE_WAIT_RELEASE;
            }
            break;

        case BUTTON_STATE_WAIT_RELEASE:
            handle->current_state = BUTTON_STATE_IDLE;
            break;
    }
    return is_triggered;
}

/**
 * @brief Lógica principal de controle de envio MQTT.
 */
void system_control_task_handler(esp_mqtt_client_handle_t client) {
    if (button_poll_event(&g_btn_light_handle)) {
        g_light_state = !g_light_state;
        
        const char *payload = g_light_state ? "ON" : "OFF";
        
        esp_mqtt_client_publish(client, "ifpb/projeto/led", payload, 0, 1, 0);
        
        ESP_LOGI(TAG, "Botão acionado! Estado enviado: %s", payload);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado ao Broker!");
            esp_mqtt_client_publish(event->client, "ifpb/projeto/led", "ESP32 Conectado", 0, 1, 0);
            esp_mqtt_client_subscribe(event->client, "ifpb/projeto/led", 0);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MENSAGEM RECEBIDA!");
            printf("Tópico: %.*s\r\n", event->topic_len, event->topic);
            printf("Dados: %.*s\r\n", event->data_len, event->data);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Desconectado");
            break;
        default:
            break;
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_URL,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    bsp_gpio_init();

    ESP_LOGI(TAG, "Sistema iniciado. Aguardando botão...");

    while (1) {
        system_control_task_handler(client);
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}