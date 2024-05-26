#include "util.h"


// for silence detection in audio recording 
float calculate_short_time_energy(int16_t *samples, size_t num_samples) {
    if (num_samples == 0) return -INFINITY;  // Avoid division by zero and log(0)

    float energy = 0.0f;
    int16_t max_amplitude = 0;

    // Find the maximum amplitude to use for normalization
    for (size_t i = 0; i < num_samples; ++i) {
        if (abs(samples[i]) > max_amplitude) {
            max_amplitude = abs(samples[i]);
        }
    }

    // Calculate energy, normalized by the maximum amplitude
    for (size_t i = 0; i < num_samples; ++i) {
        float normalized_sample = (max_amplitude != 0) ? (samples[i] / (float)max_amplitude) : 0;
        energy += normalized_sample * normalized_sample;
    }

    float rms = sqrt(energy / num_samples);

    // Convert RMS to decibels (dB)
    float db = 20.0 * log10(rms + 1e-6); // Adding a small value to avoid log(0)
    return -db;
}