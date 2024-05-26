idf_component_register(SRCS "util.c" "wifi.c" "gpio.c" "main.c" "sd_card.c" "i2s_mic.c" "gpio.c" "util.c" "wifi.c"
                    PRIV_REQUIRES esp_driver_i2s fatfs
                    INCLUDE_DIRS "." "$ENV{IDF_PATH}/examples/peripherals/i2s/common" "$ENV{IDF_PATH}/components/soc/esp32s3/include"
                    REQUIRES nvs_flash esp_wifi esp_http_server)


