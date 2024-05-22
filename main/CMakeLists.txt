idf_component_register(SRCS "main.c" "sd_card.c" "i2s_mic.c"
                    PRIV_REQUIRES esp_driver_i2s fatfs
                    INCLUDE_DIRS "." "$ENV{IDF_PATH}/examples/peripherals/i2s/common")


