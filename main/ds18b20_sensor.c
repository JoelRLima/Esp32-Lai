/**
 * @file ds18b20_sensor.c
 * @brief Implementação da Task FreeRTOS para leitura periódica do sensor.
 *
 * Utiliza o driver RMT (Remote Control) do ESP32 para simular o protocolo 1-Wire.
 * A leitura é feita em loop infinito e enviada para uma Queue.
 */
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"

#include "onewire_bus.h"
#include "ds18b20.h"
#include "ds18b20_sensor.h"

#include "display.h"   // permite chamar display_update_temp(), display_log_add(), etc.

static QueueHandle_t s_fila_temperatura = NULL; //Serve pra guardar a fila de temperatura


// DEFINIÇÕES DO HARDWARE
#define ONEWIRE_BUS_GPIO 17
#define MAX_DS18B20_DEVICES 1       // 1 sensor
#define DS18B20_RESOLUTION DS18B20_RESOLUTION_12B

#define TASK_DELAY_MS 3000

static const char *TAG_SENSOR = "DS18B20_SERVICE";

static onewire_bus_handle_t bus_handle;

//Static porque só precisa ser vista por esse arquivo
static void ds18b20_read_task(void *pvParameter){
    ds18b20_device_handle_t dev_handle = (ds18b20_device_handle_t)pvParameter;
    float temperatura_c;

    if(dev_handle == NULL){
        ESP_LOGE(TAG_SENSOR, "Handle da tarefa é NULL, task abortada");
        vTaskDelete(NULL);
        return;
    }  

    ESP_LOGI(TAG_SENSOR, "Tarefa de leitura iniciada");
    while (1){

        // 1. Disparar conversão
        esp_err_t err = ds18b20_trigger_temperature_conversion(dev_handle); 
        if(err!= ESP_OK){
            ESP_LOGE(TAG_SENSOR, "Falha ao disparar conversão.");
            vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
            continue;
        }

        // 2. Aguardar o tempo de conversão (Obrigatório, 750ms para 12B)
        vTaskDelay(pdMS_TO_TICKS(800));

        // 3. Ler temperatura
        err = ds18b20_get_temperature(dev_handle, &temperatura_c);
        if (err == ESP_OK){
            ESP_LOGI(TAG_SENSOR, "Temperatura lida: %.2f C", temperatura_c);
            if (s_fila_temperatura != NULL) {
                xQueueSend(s_fila_temperatura, &temperatura_c, 0);
        }
        }else{
            ESP_LOGE(TAG_SENSOR, "Falha ao ler temperatura.");
        }

        // 4. Aguardar próximo ciclo
        int tempo_restante = TASK_DELAY_MS - 800;
        if (tempo_restante > 0) {
            vTaskDelay(pdMS_TO_TICKS(tempo_restante));
        }
    }
}

void ds18b20_init_(QueueHandle_t queue_saida){
    s_fila_temperatura = queue_saida;

    ESP_LOGI(TAG_SENSOR, "Inicializando o barramento 1-Wire no GPIO %d...", ONEWIRE_BUS_GPIO);

    // INICIALIZAR BARRAMENTO 1-WIRE
    bus_handle = NULL;
    onewire_bus_config_t bus_config = {
        .bus_gpio_num = ONEWIRE_BUS_GPIO,
        .flags = {
            .en_pull_up = false     // Para ativar o resistor de pull-up
        }
    };  
    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = 10, 
    };
    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus_handle));

    // Buscar por dispositivos DS18B20
    int ds18b20_device_num = 0;
    ds18b20_device_handle_t ds18b20s[MAX_DS18B20_DEVICES];
    onewire_device_iter_handle_t iter = NULL;
    onewire_device_t next_onewire_device;
    esp_err_t search_result = ESP_OK;

    // Criar o iterador de dispositivo para a busca
    ESP_ERROR_CHECK(onewire_new_device_iter(bus_handle, &iter));
    ESP_LOGI(TAG_SENSOR, "Iterador de dispositivo criado, iniciando busca...");

    // Loop de busca
    do {
        search_result = onewire_device_iter_get_next(iter, &next_onewire_device);

        if (search_result == ESP_OK) { // Encontrou um dispositivo
            
            ds18b20_config_t ds_cfg = {}; // Configuração padrão
            
            // Verifica se é um DS18B20 e se ainda tem espaço no array (MAX_DS18B20_DEVICES)
            if (ds18b20_device_num < MAX_DS18B20_DEVICES &&
                ds18b20_new_device_from_enumeration(&next_onewire_device, &ds_cfg, &ds18b20s[ds18b20_device_num]) == ESP_OK) 
            {
                onewire_device_address_t address;
                ds18b20_get_device_address(ds18b20s[ds18b20_device_num], &address);
                ESP_LOGI(TAG_SENSOR, "Encontrado DS18B20[%d], Endereço: %016llX", ds18b20_device_num, address);
                
                // Depois de encontrar, configuramos e iniciamos a tarefa de leitura

                // Configurar a Resolução
                ESP_ERROR_CHECK(ds18b20_set_resolution(ds18b20s[ds18b20_device_num], DS18B20_RESOLUTION));

                // Criar a Tarefa de Leitura para este sensor
                xTaskCreate(
                    ds18b20_read_task,
                    "ds18b20_read_task", // Nome da tarefa
                    2048,                // Stack
                    (void*)ds18b20s[ds18b20_device_num], // Passa o handle do sensor encontrado
                    5,                   // Prioridade
                    NULL
                );
                
                ds18b20_device_num++; // Incrementa o contador de sensores encontrados
    
            } else {
                ESP_LOGW(TAG_SENSOR, "Encontrado dispositivo 1-Wire desconhecido, Endereço: %016llX", next_onewire_device.address);
            }
        }
    } while (search_result != ESP_ERR_NOT_FOUND); // Para quando não encontrar mais nada

    // Destruir o iterador
    ESP_ERROR_CHECK(onewire_del_device_iter(iter));
    ESP_LOGI(TAG_SENSOR, "Busca concluída, %d sensor(es) DS18B20 iniciados.", ds18b20_device_num);

    if (ds18b20_device_num == 0) {
        ESP_LOGE(TAG_SENSOR, "NENHUM sensor DS18B20 encontrado no GPIO %d. Verifique a fiação e o resistor!", ONEWIRE_BUS_GPIO);
    }
}