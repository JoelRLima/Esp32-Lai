/**
 * @file ds18b20_sensor.h
 * @brief Driver de alto nível para leitura do sensor de temperatura DS18B20.
 *
 * Este componente gerencia a inicialização do barramento 1-Wire, a busca
 * de dispositivos e cria uma Task do FreeRTOS para leitura periódica.
 *
 */

#ifndef DS18B20_H
#define DS18B20_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o sensor DS18B20 e inicia a tarefa de leitura.
 *
 * Esta função configura o pino GPIO do barramento 1-Wire, busca pelo primeiro
 * sensor disponível e inicia uma tarefa (Task) em background que lê a
 * temperatura periodicamente.
 *
 * @note A função aborta (vTaskDelete) se nenhum sensor for encontrado.
 *
 * @param output_queue Handle da fila (Queue) onde as temperaturas serão depositadas.
 * A fila deve ser capaz de armazenar dados do tipo 'float'.
 * Se passar NULL, a tarefa de leitura não enviará dados.
 *
 * @return void
 */
void ds18b20_init_(QueueHandle_t output_queue);

#ifdef __cplusplus
}
#endif

#endif // DS18B20_H