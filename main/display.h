/**
 * @file display.h
 * @brief Interface de controle para o display LCD ILI9341 usando LVGL.
 *
 * Este componente abstrai toda a complexidade da biblioteca gráfica LVGL e
 * dos drivers SPI do ESP32. Ele fornece funções simples para exibir texto,
 * atualizar valores de temperatura e logs na tela.
 *
 */

#pragma once
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o hardware do display e a biblioteca gráfica.
 * * Configura o barramento SPI, o driver do painel ILI9341 e o adaptador
 * do LVGL (Port). Deve ser chamado antes de qualquer outra função de display.
 * * @return ESP_OK se sucesso, ou código de erro (ESP_FAIL, ESP_ERR_NO_MEM) caso contrário.
 */
esp_err_t display_init(void);

/**
 * @brief Exibe uma mensagem de boas-vindas simples no centro da tela.
 * Útil para testes iniciais de funcionamento do LCD.
 * @param txt Texto a ser exibido (padrão "Hello" se NULL).
 */
void display_hello(const char *txt);

/**
 * @brief Controla o Backlight (Luz de fundo) do LCD.
 * @param on true para ligar, false para desligar.
 */
void display_set_backlight(bool on);

/**
 * @brief Atualiza o valor numérico da temperatura na tela principal.
 * * Esta função é Thread-Safe (segura para chamadas de múltiplas tasks),
 * pois utiliza o mecanismo de travamento (Lock) do LVGL.
 * * @param temp Valor da temperatura em graus Celsius (float).
 */
void display_update_temp(float temp);

/**
 * @brief Retorna o handle interno do display LVGL.
 * Útil se for necessário adicionar objetos gráficos avançados externamente.
 * @return Ponteiro para o objeto lv_display_t.
 */
lv_display_t * display_get_lv_display(void);

// --- Funções Auxiliares de Visualização ---

/**
 * @brief Limpa todos os elementos da tela atual (Remove todos os widgets).
 * Reseta também os ponteiros internos de Labels e Logs.
 */
void display_clear(void);

/**
 * @brief Imprime um texto formatado em uma posição específica (X, Y).
 * Funciona como um 'printf' posicional.
 * @param x Posição horizontal em pixels.
 * @param y Posição vertical em pixels.
 * @param fmt String de formatação (ex: "Valor: %d").
 * @param ... Argumentos variáveis.
 */
void display_printf_at(int x, int y, const char *fmt, ...);

// --- Área de Log (Terminal na Tela) ---

/**
 * @brief Adiciona uma linha de texto na área de log visual.
 * * Funciona como um buffer circular visual. Quando a tela enche,
 * as linhas antigas são roladas para cima.
 * * @param txt String a ser adicionada.
 */
void display_log_add(const char *txt);

/**
 * @brief Limpa apenas a área de log, mantendo outros elementos se existirem.
 */
void display_log_clear(void);

#ifdef __cplusplus
}
#endif