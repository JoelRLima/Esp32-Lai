idf_component_register(SRCS "main.c" "ds18b20_sensor.c" "display.c" "wifi_connect.c"
                    PRIV_REQUIRES
                        driver
                        esp_driver_gpio
                        esp_driver_rmt
                        nvs_flash
                        espressif__ds18b20
                        espressif__onewire_bus
                        esp_lcd lvgl esp_lvgl_port
                        esp_lcd_ili9341
                        esp_wifi
                        esp_event
                        esp_netif
                        lwip
                        esp_http_client
                        mqtt
)
