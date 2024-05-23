/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/* I2S Digital Microphone Recording Example */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "i2s_mic.h"


void init_microphone(i2s_chan_handle_t *rx_handle)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, rx_handle));

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(44100),
        /* The default mono slot is the left slot (whose 'select pin' of the PDM microphone is pulled down) */
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = CONFIG_EXAMPLE_I2S_CLK_GPIO,
            .din = CONFIG_EXAMPLE_I2S_DATA_GPIO,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(*rx_handle, &pdm_rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(*rx_handle));
}



void scale_audio_samples(int16_t *samples, size_t num_samples, float scale_factor)
{
    for (size_t i = 0; i < num_samples; ++i) {
        samples[i] = (int16_t)(samples[i] * scale_factor);
    }
}