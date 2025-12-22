/**
 * @file display.c
 * @brief Implementação dos drivers de vídeo e interface gráfica (UI).
 * * Utiliza o componente 'esp_lcd' para comunicação de baixo nível (SPI)
 * e 'lvgl' (Light and Versatile Graphics Library) para renderização de widgets.
 */

#include "display.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include <stdarg.h>
#include <math.h>

static const char *TAG = "display_comp";

//PINAGEM
#define LCD_HOST    SPI2_HOST
#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   15
#define PIN_NUM_DC   2
#define PIN_NUM_RST  -1
#define PIN_NUM_BL   27

#define LCD_HRES 320
#define LCD_VRES 240

static esp_lcd_panel_handle_t s_panel = NULL;
static lv_display_t * s_lv_display = NULL;
static lv_obj_t * s_label_temp = NULL;

// Protótipo interno
static bool _create_lvgl_label_if_needed(void);

/**
 * @brief Inicializa o subsistema de vídeo, SPI e LVGL.
 */
esp_err_t display_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "display_init: enabling backlight if available");
    if (PIN_NUM_BL != -1) {
        gpio_set_direction(PIN_NUM_BL, GPIO_MODE_OUTPUT);
        gpio_set_level(PIN_NUM_BL, 1);
    }

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_CLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_HRES * 40 * sizeof(uint16_t),
    };
    ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = 26 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ret = esp_lcd_new_panel_io_spi(LCD_HOST, &io_cfg, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };

    ret = esp_lcd_new_panel_ili9341(io_handle, &panel_cfg, &s_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_ili9341 failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_mirror(s_panel, true, false);  // flip X (ajuste se necessário)
    esp_lcd_panel_swap_xy(s_panel, false);
    esp_lcd_panel_disp_on_off(s_panel, true);

    ESP_LOGI(TAG, "Inicializando LVGL port...");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = s_panel,
        .buffer_size = 320 * 40,
        .double_buffer = false,
        .hres = LCD_HRES,
        .vres = LCD_VRES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
        }
    };

    s_lv_display = lvgl_port_add_disp(&disp_cfg);
    if (s_lv_display == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp returned NULL");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "display_init: OK");
    return ESP_OK;
}

/**
 * @brief Exibe texto de teste ("Hello World").
 */
void display_hello(const char *txt)
{
    if (!s_lv_display) {
        ESP_LOGW(TAG, "display_hello: display not initialized");
        return;
    }
    if (!txt) txt = "Hello";

    if (lvgl_port_lock(0)) {
        lv_obj_t *scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

        lv_obj_t *lbl = lv_label_create(scr);
        lv_label_set_text(lbl, txt);
        lv_obj_set_style_text_font(lbl, LV_FONT_DEFAULT, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x00FF00), 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

        lvgl_port_unlock();
    } else {
        ESP_LOGW(TAG, "display_hello: failed to lock LVGL");
    }
}

void display_set_backlight(bool on)
{
    if (PIN_NUM_BL == -1) return;
    gpio_set_level(PIN_NUM_BL, on ? 1 : 0);
}

/**
 * @brief Atualiza o label de temperatura.
 * Implementa proteção thread-safe com lvgl_port_lock.
 */
void display_update_temp(float temp)
{
    if (!s_lv_display) return;                // display não inicializado
    if (!_create_lvgl_label_if_needed()) {    // cria labels caso não existam
        ESP_LOGW(TAG, "display_update_temp: label não criado");
        return;
    }

    char buf[32];
    // Formata a string com segurança
    snprintf(buf, sizeof(buf), "%.2f C", temp);

    if (lvgl_port_lock(0)) {
        // Atualiza o label que mostra só o valor (s_label_temp)
        lv_label_set_text(s_label_temp, buf);
        lvgl_port_unlock();
    } else {
        ESP_LOGW(TAG, "display_update_temp: não conseguiu obter lock LVGL");
    }
}

lv_display_t * display_get_lv_display(void)
{
    return s_lv_display;
}

/**
 * @brief Função interna para criar estrutura visual da temperatura.
 */
static bool _create_lvgl_label_if_needed(void)
{
    if (s_label_temp) return true;
    if (!s_lv_display) return false;

    if (!lvgl_port_lock(0)) return false;

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    // Título fixo
    lv_obj_t *lbl_title = lv_label_create(scr);
    lv_label_set_text(lbl_title, "Temperatura:");
    lv_obj_set_style_text_font(lbl_title, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, -30);

    // Label de valor (apenas o número), guardado em s_label_temp
    s_label_temp = lv_label_create(scr);
    lv_label_set_text(s_label_temp, "--.-- C"); // texto inicial
    lv_obj_set_style_text_font(s_label_temp, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(s_label_temp, lv_color_hex(0x00FF00), 0);
    lv_obj_align(s_label_temp, LV_ALIGN_CENTER, 0, 10);

    lvgl_port_unlock();
    return true;
}

#define LOG_AREA_MAX_LINES 10
#define LOG_AREA_LINE_HEIGHT 20

/* internals para área de log */
static lv_obj_t *s_log_container = NULL;
static lv_obj_t *s_log_lines[LOG_AREA_MAX_LINES] = {0};
static int s_log_next_index = 0; // próximo slot circular

/* limpa toda a tela (remove filhos da tela) */
void display_clear(void)
{
    if (!s_lv_display) return;
    if (!lvgl_port_lock(0)) return;
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr); // remove todos os filhos
    // reseta referências internas relacionadas à UI
    s_label_temp = NULL;
    s_log_container = NULL;
    for (int i=0;i<LOG_AREA_MAX_LINES;i++) s_log_lines[i] = NULL;
    s_log_next_index = 0;
    lvgl_port_unlock();
}

/* imprime string formatada em (x,y) absoluto (px) */
void display_printf_at(int x, int y, const char *fmt, ...)
{
    if (!s_lv_display) return;
    char buf[128];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (!lvgl_port_lock(0)) return;
    lv_obj_t *scr = lv_scr_act();
    // cria um label temporário
    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_font(lbl, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(lbl, x, y);
    lvgl_port_unlock();
}

/* cria a área de log se necessario */
static void _ensure_log_container(void)
{
    if (s_log_container) return;
    if (!lvgl_port_lock(0)) return;
    lv_obj_t *scr = lv_scr_act();
    // cria um contêiner simples sem borda
    s_log_container = lv_obj_create(scr);
    lv_obj_set_size(s_log_container, LCD_HRES - 8, LOG_AREA_LINE_HEIGHT * LOG_AREA_MAX_LINES);
    lv_obj_align(s_log_container, LV_ALIGN_TOP_LEFT, 4, 4); // margens
    lv_obj_set_style_bg_opa(s_log_container, LV_OPA_TRANSP, 0);
    // criar linhas vazias
    for (int i=0;i<LOG_AREA_MAX_LINES;i++) {
        s_log_lines[i] = lv_label_create(s_log_container);
        lv_label_set_text(s_log_lines[i], "");
        lv_obj_set_style_text_font(s_log_lines[i], LV_FONT_DEFAULT, 0);
        lv_obj_set_style_text_color(s_log_lines[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_pos(s_log_lines[i], 0, i * LOG_AREA_LINE_HEIGHT);
    }
    s_log_next_index = 0;
    lvgl_port_unlock();
}

/* adiciona uma linha ao log area (rotaciona se cheio) */
void display_log_add(const char *txt)
{
    if (!s_lv_display) return;
    _ensure_log_container();
    if (!s_log_container) return;

    if (!lvgl_port_lock(0)) return;

    // escrever na próxima linha (circular)
    lv_label_set_text(s_log_lines[s_log_next_index], txt);

    // Incrementa index e, quando encher, desloca textos para cima
    s_log_next_index++;
    if (s_log_next_index >= LOG_AREA_MAX_LINES) {
        // roll: move todos textos 1 para cima
        for (int i = 0; i < LOG_AREA_MAX_LINES - 1; ++i) {
            const char *t = lv_label_get_text(s_log_lines[i+1]);
            lv_label_set_text(s_log_lines[i], t ? t : "");
        }
        // limpa última linha e coloca index no fim
        lv_label_set_text(s_log_lines[LOG_AREA_MAX_LINES - 1], "");
        s_log_next_index = LOG_AREA_MAX_LINES - 1;
    }

    lvgl_port_unlock();
}

/* limpa a area de log */
void display_log_clear(void)
{
    if (!s_lv_display) return;
    if (!lvgl_port_lock(0)) return;
    if (s_log_container) {
        for (int i=0;i<LOG_AREA_MAX_LINES;i++) {
            lv_label_set_text(s_log_lines[i], "");
        }
        s_log_next_index = 0;
    }
    lvgl_port_unlock();
}