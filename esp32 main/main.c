#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/queue.h"

// 1 = HTTP (Servidor Python Local / Dash)
// 2 = MQTT (Nuvem / HiveMQ)
#define MODO_OPERACAO  2 

// Bibliotecas de Comunicação
#include "esp_http_client.h"
#include "mqtt_client.h"

// Includes manuais de Hardware
#include "display.h"
#include "ds18b20_sensor.h"
#include "nvs_flash.h"
#include "wifi_connect.h"

static const char *TAG = "SISTEMA_HIBRIDO";

//  PROTOCOLO 1: HTTP
void enviar_via_http(float temperatura) {
    esp_http_client_config_t config = {
        .url = "http://10.7.224.65:5000/telemetry", 
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"temp\": %.2f}", temperatura);
    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[HTTP] Sucesso: %s", payload);
    } else {
        ESP_LOGE(TAG, "[HTTP] Falha: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

//  PROTOCOLO 2: MQTT
esp_mqtt_client_handle_t cliente_mqtt = NULL;

// Broker Público Gratuito (Funciona direto, sem senha)
#define MQTT_BROKER_URI "mqtt://broker.hivemq.com"
// Tópico Exclusivo
#define MQTT_TOPIC      "iniciacao/ricardo/temp"

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    if (event_id == MQTT_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "[MQTT] CONECTADO A NUVEM!");
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        ESP_LOGI(TAG, "[MQTT] Caiu a conexão.");
    }
}

void setup_mqtt(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };
    cliente_mqtt = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(cliente_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(cliente_mqtt);
}

void enviar_via_mqtt(float temperatura) {
    if (cliente_mqtt == NULL) return;

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"temp\": %.2f}", temperatura);

    // Envia mensagem
    // Tópico, Dados, Tamanho(0=calc auto), QoS(1=garantido), Retain(0=não)
    int msg_id = esp_mqtt_client_publish(cliente_mqtt, MQTT_TOPIC, payload, 0, 1, 0);
    
    if (msg_id != -1) {
        ESP_LOGI(TAG, "[MQTT] Publicado na Nuvem: %s", payload);
    } else {
        ESP_LOGE(TAG, "[MQTT] Erro ao publicar");
    }
}

//  MAIN
void app_main(void)
{
    // 1. Inicializa NVS (Obrigatório para Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Conecta no Wi-Fi
    ESP_LOGI(TAG, "Conectando Wi-Fi...");
    wifi_connect_init(); 

    // 3. Inicializa Display
    if (display_init() != ESP_OK) while(1) vTaskDelay(100);

    // 4. Inicializa o Protocolo escolhido
    #if MODO_OPERACAO == 2
        ESP_LOGI(TAG, ">>> INICIANDO MODO MQTT (Nuvem) <<<");
        display_printf_at(10, 10, "Modo: MQTT Nuvem");
        setup_mqtt();
    #else
        ESP_LOGI(TAG, ">>> INICIANDO MODO HTTP (Local) <<<");
        display_printf_at(10, 10, "Modo: HTTP Local");
    #endif

    // 5. Inicializa Sensor
    QueueHandle_t fila_temp = xQueueCreate(5, sizeof(float));
    ds18b20_init_(fila_temp);

    // 6. Loop
    while (1) {
        float temp_lida;
        if (xQueueReceive(fila_temp, &temp_lida, portMAX_DELAY)) {
            
            // Atualiza Display
            display_update_temp(temp_lida);
            
            // Envia pelo protocolo selecionado
            #if MODO_OPERACAO == 1
                enviar_via_http(temp_lida);
            #elif MODO_OPERACAO == 2
                enviar_via_mqtt(temp_lida);
            #endif
        }
    }
}