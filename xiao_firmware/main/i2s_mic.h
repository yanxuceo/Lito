#ifndef I2S_MIC_H
#define I2S_MIC_H

#include "driver/i2s_pdm.h"
#include "format_wav.h"


// do not change for best
#define SAMPLE_RATE     16000U
#define SAMPLE_BITS     16
#define WAV_HEADER_SIZE 44
#define VOLUME_GAIN     2


void init_microphone(i2s_chan_handle_t *rx_handle);
void scale_audio_samples(int16_t *samples, size_t num_samples, int gain);

#endif