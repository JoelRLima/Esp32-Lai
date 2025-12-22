// main/wifi_connect.c
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi_connect.h"

// CONFIGURAÇÕES
#define WIFI_SSID      "NomeDaRedeAqui"
#define WIFI_PASS      "SenhaDaRedeAqui"
#define MAXIMUM_RETRY  5

// Tags para Log
static const char *TAG = "WIFI_CONNECT";

// Event Group
static EventGroupHandle_t s_wifi_event_group;

// Definição dos bits
#define WIFI_CONNECTED_BIT BIT0  // Luz Verde (Tem IP)
#define WIFI_FAIL_BIT      BIT1  // Luz Vermelha (Desistiu)

static int s_retry_num = 0;

// Event Handler
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    // Cenario 1: O Driver Wi-Fi acabou de ligar (Start)
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect(); // Tenta conectar no roteador
    } 
    // Cenario 2: A conexão caiu ou falhou
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Tentando reconectar... (Tentativa %d)", s_retry_num);
        } else {
            // Se tentou demais, levanta a bandeira de FALHA
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } 
    // Cenario 3: O roteador entregou um IP (Sucesso!)
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Conectado! IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        // Levanta a bandeira de SUCESSO
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_connect_init(void)
{
    // 1. Cria o grupo de eventos
    s_wifi_event_group = xEventGroupCreate();

    // 2. Inicializa a pilha TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());

    // 3. Cria o Loop de Eventos padrão
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 4. Configuração padrão do driver Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 5. Registra o Callback para eventos de Wi-Fi e IP
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // 6. Configura o SSID e Senha
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 7. Liga o Rádio (Isso dispara o evento WIFI_EVENT_STA_START)
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi iniciado. Aguardando conexão...");

    // 8. BLOQUEIO: A função para aqui e espera até conectar ou falhar
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    // 9. Verifica o resultado
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado com sucesso ao SSID: %s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Falha ao conectar no SSID: %s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Evento inesperado");
    }
}