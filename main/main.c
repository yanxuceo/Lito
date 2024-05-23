#include <stdio.h>
#include <math.h>
#include <inttypes.h>           // Add this header for PRIu32
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <sys/unistd.h>
#include "sd_card.h"
#include "esp_log.h"
#include "i2s_mic.h"



static const char *TAG = "app";

#define BUFFER_SIZE     4096           // Size of each segment in the circular buffer
#define NUM_SEGMENTS    20             // Number of segments in the circular buffer
#define TOTAL_BUFFER_SIZE (BUFFER_SIZE * NUM_SEGMENTS)

#define FILE_PREFIX         "/sdcard/record_"
#define FILE_EXTENSION      ".wav"
#define ENERGY_THRESHOLD    5000       // Adjust this value based on your microphone sensitivity

static int16_t circular_buffer[TOTAL_BUFFER_SIZE];
static volatile int write_index = 0;  // Index where new data is written
static volatile int read_index = 0;   // Index where data is read for writing to SD

SemaphoreHandle_t bufferReadySemaphore;
SemaphoreHandle_t bufferEmptySemaphore;

static int file_index = 0;

i2s_chan_handle_t rx_handle = NULL;
volatile bool is_recording = true;
volatile bool stop_recording = false;  // Flag to stop recording


#define BUILTIN_LED GPIO_NUM_21
SemaphoreHandle_t sdCardSemaphore;

SemaphoreHandle_t sdWriteSemaphore;
SemaphoreHandle_t startCaptureSemaphore;

static FILE *current_file = NULL;
static size_t current_file_size = 0;

#define MAX_FILE_SIZE_MB    2       // Max file size in megabytes
#define MAX_FILE_SIZE       (MAX_FILE_SIZE_MB * 1024 * 1024)  // Max file size in bytes

#define MAX_PATH_LENGTH 255


float calculate_short_time_energy(int16_t *samples, size_t num_samples) {
    float energy = 0.0f;
    for (size_t i = 0; i < num_samples; ++i) {
        energy += samples[i] * samples[i];
    }
    float rms = sqrt(energy / num_samples);

    // Convert RMS to decibels (dB)
    float db = 20.0 * log10(rms + 1e-6); // Adding a small value to avoid log(0)
    return db;
}


void open_new_file() {
    char file_name[MAX_PATH_LENGTH];
    snprintf(file_name, sizeof(file_name), "%s/%03d%s", MOUNT_POINT, file_index++, FILE_EXTENSION);

    if (current_file != NULL) {
        fclose(current_file);
    }

    current_file = fopen(file_name, "a");  // Open file in append binary mode
    if (current_file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", file_name);
        return;
    }

    ESP_LOGI(TAG, "Opened new file: %s", file_name);
    current_file_size = 0;

    // Write the WAV header
    wav_header_t wav_header = WAV_HEADER_PCM_DEFAULT(MAX_FILE_SIZE, 16, CONFIG_EXAMPLE_SAMPLE_RATE, 1);
    fwrite(&wav_header, sizeof(wav_header), 1, current_file);
}


void capture_audio_task(void *param) {
    i2s_chan_handle_t *rx_handle = (i2s_chan_handle_t *)param;
    size_t bytes_read;

    if (xSemaphoreTake(startCaptureSemaphore, portMAX_DELAY) == pdTRUE) {

        while (is_recording) {
            int16_t *write_ptr = &circular_buffer[write_index];

            // Wait for space to be available in the buffer
            xSemaphoreTake(bufferEmptySemaphore, portMAX_DELAY);

            // Read audio data into the circular buffer
            esp_err_t result = i2s_channel_read(*rx_handle, (char *)write_ptr, BUFFER_SIZE * sizeof(int16_t), &bytes_read, 1000);
            if (result == ESP_OK && bytes_read == BUFFER_SIZE * sizeof(int16_t)) {
                ESP_LOGE(TAG, "reading audio from buffer is okay.");
                write_index = (write_index + BUFFER_SIZE) % TOTAL_BUFFER_SIZE;  // Update write index

                // Signal the write task that new data is available
                xSemaphoreGive(bufferReadySemaphore);

                // Check if the buffer is full
                if (write_index == read_index) {
                    ESP_LOGE(TAG, "Buffer overflow! Write index has caught up to read index.");
                    is_recording = false;  // Stop recording
                    break;  // Exit the loop
                }
            } else {
                ESP_LOGE(TAG, "Failed to read data from I2S or incomplete read");
            }

            vTaskDelay(1 / portTICK_PERIOD_MS);  // Yield to other tasks
        }

        // Signal to stop the write task
        stop_recording = true;

        // Disable the I2S channel when stopping
        i2s_channel_disable(*rx_handle);
    }

    // To prevent the task from returning, enter an infinite loop
    while (1) {
        vTaskDelay(portMAX_DELAY);  // Block forever
    }
}


void write_to_sd_task(void *param) {
    if (xSemaphoreTake(sdWriteSemaphore, portMAX_DELAY) == pdTRUE) {
        open_new_file();  // Open the initial file

        while (!stop_recording) {
            // Wait for data to be available in the buffer
            xSemaphoreTake(bufferReadySemaphore, portMAX_DELAY);

            if (write_index != read_index) {
                int16_t *read_ptr = &circular_buffer[read_index];

                // Calculate energy of the buffer segment
                float energy = calculate_short_time_energy(read_ptr, BUFFER_SIZE);
                ESP_LOGI(TAG, "Calculated energy: %f", energy);

                //if (energy >= ENERGY_THRESHOLD) {
                    // Check if the current file size exceeds the maximum size
                    // Write the audio data
                    fwrite(read_ptr, sizeof(int16_t), BUFFER_SIZE, current_file);
                  
                    current_file_size += BUFFER_SIZE * sizeof(int16_t);
                    ESP_LOGI(TAG, "Written %d bytes to file", current_file_size);

                    if (current_file_size >= MAX_FILE_SIZE) {
                        ESP_LOGI(TAG, "Closing file 1");
                        open_new_file();
                    }
                    
                //}

                // Update read index after writing
                read_index = (read_index + BUFFER_SIZE) % TOTAL_BUFFER_SIZE;

                // Signal the capture task that space is available in the buffer
                xSemaphoreGive(bufferEmptySemaphore);
            }

            //vTaskDelay(1 / portTICK_PERIOD_MS);  // Yield to other tasks
        }

        // Close the file when done
        if (current_file != NULL) {
            ESP_LOGI(TAG, "Closing file 2");
            fclose(current_file);
        }
    }

    // To prevent the task from returning, enter an infinite loop
    while (1) {
        vTaskDelay(portMAX_DELAY);  // Block forever
    }
}


void wait_sd_init(void) {
    xSemaphoreGive(sdWriteSemaphore);
}

void wait_audio_to_record(void) {
    xSemaphoreGive(startCaptureSemaphore);
}


void app_main(void)
{
    printf("PDM microphone recording example start\n--------------------------------------\n");

    // Initialize the SD card
    if (sd_card_init() != ESP_OK) {
        printf("Failed to initialize SD card\n");
        return;
    }

    init_microphone(&rx_handle);
    
    // Create semaphores for buffer synchronization
    bufferReadySemaphore = xSemaphoreCreateBinary();
    bufferEmptySemaphore = xSemaphoreCreateCounting(NUM_SEGMENTS, NUM_SEGMENTS);  // Initialize as available
    sdWriteSemaphore = xSemaphoreCreateBinary();
    startCaptureSemaphore = xSemaphoreCreateBinary();

    
    // Initialize the recording flag
    is_recording = true;
    
    // Create tasks for capturing audio and writing to SD card
    xTaskCreate(capture_audio_task, "capture_audio_task", 4096, &rx_handle, 2, NULL);  // Highest priority
    xTaskCreate(write_to_sd_task, "write_to_sd_task", 4096, NULL, 1, NULL);            // Lower priority


    // Simulate triggering SD card write after some delay
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    wait_sd_init();
    wait_audio_to_record();

    // Optionally blink LED to indicate recording has started
    gpio_set_direction(BUILTIN_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(BUILTIN_LED, 1);
}