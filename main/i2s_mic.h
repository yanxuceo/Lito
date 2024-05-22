
#ifndef I2S_MIC_H
#define I2S_MIC_H

#include "driver/i2s_pdm.h"
#include "format_wav.h"


#define NUM_CHANNELS        (1) // For mono recording only!
#define SAMPLE_SIZE         (CONFIG_EXAMPLE_BIT_SAMPLE * 1024)
#define BYTE_RATE           (CONFIG_EXAMPLE_SAMPLE_RATE * (CONFIG_EXAMPLE_BIT_SAMPLE / 8)) * NUM_CHANNELS



void init_microphone(i2s_chan_handle_t *rx_handle);


#endif