#include <stdio.h>
#include <math.h>
#include <inttypes.h>         
#include <sys/unistd.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "sd_card.h"
#include "i2s_mic.h"

#include "config.h"
#include "gpio.h"
#include "wifi.h"
#include "util.h"

#include "esp_log.h"

static const char *TAG = "app";

static int16_t circular_buffer[TOTAL_BUFFER_SIZE];
static volatile int write_index = 0;                // Index where new data is written
static volatile int read_index = 0;                 // Index where data is read for writing to SD

static int file_index = 0;
i2s_chan_handle_t rx_handle = NULL;

static FILE *current_file = NULL;
static size_t current_file_size = 0;

volatile bool is_capturing = true;
volatile bool stop_recording = false;               // Flag to stop recording


SemaphoreHandle_t bufferReadySemaphore;
SemaphoreHandle_t bufferEmptySemaphore;


// WiFi task
void handleWiFiRequests(void *param) {
    start_wifi();
    start_http_server();

    while (1) {
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}


// TODO: AUDIO_DIR should be named with current date 
void open_new_file() {
    char dir_path[MAX_PATH_LENGTH];
    snprintf(dir_path, sizeof(dir_path), "%s%s", MOUNT_POINT, AUDIO_DIR);

    // Create the directory if it does not exist
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
        if (mkdir(dir_path, 0700) != 0) {
            ESP_LOGE(TAG, "Failed to create directory: %s", dir_path);
            return;
        }
    }

    char file_name[255];
    snprintf(file_name, sizeof(file_name), "%s/%04d%s", dir_path, file_index++, FILE_EXTENSION);

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
    wav_header_t wav_header = WAV_HEADER_PCM_DEFAULT(MAX_FILE_SIZE, 16, SAMPLE_RATE, 1);
    if (fwrite(&wav_header, sizeof(wav_header), 1, current_file) != 1) {
        ESP_LOGE(TAG, "Failed to write WAV header to file: %s", file_name);
        fclose(current_file);
        current_file = NULL;
        return;
    }
}


void capture_audio_task(void *param) 
{
    i2s_chan_handle_t *rx_handle = (i2s_chan_handle_t *)param;
    size_t bytes_read;

    while (is_capturing) {
        int16_t *write_ptr = &circular_buffer[write_index];

        // Wait for space to be available in the buffer
        if (xSemaphoreTake(bufferEmptySemaphore, portMAX_DELAY) == pdFALSE) {
            break;  // Exit if semaphore is not given
        }

        // Read audio data into the circular buffer
        esp_err_t result = i2s_channel_read(*rx_handle, (char *)write_ptr, BUFFER_SIZE * sizeof(int16_t), &bytes_read, 1000);
        if (result == ESP_OK && bytes_read == BUFFER_SIZE * sizeof(int16_t)) {
            ESP_LOGI(TAG, "Reading audio from buffer is okay.");

            write_index = (write_index + BUFFER_SIZE) % TOTAL_BUFFER_SIZE;  // Update write index
            scale_audio_samples(write_ptr, BUFFER_SIZE, VOLUME_GAIN);

            // Signal the write task that new data is available
            xSemaphoreGive(bufferReadySemaphore);

            // Check if the buffer is full
            if (write_index == read_index) {
                ESP_LOGE(TAG, "Buffer overflow! Write index has caught up to read index.");
                break;  // Stop recording due to overflow
            }
        } else {
            ESP_LOGE(TAG, "Failed to read data from I2S or incomplete read");
        }
    }

    // Disable the I2S channel when stopping
    i2s_channel_disable(*rx_handle);
    ESP_LOGI(TAG, "Audio capture task deactivated.");
    vTaskDelete(NULL);  // Delete the task
}


void write_to_sd_task(void *param) {
    open_new_file();    // Open the initial file

    while (is_capturing) {
        // Wait for data to be available in the buffer
        if (xSemaphoreTake(bufferReadySemaphore, portMAX_DELAY) == pdFALSE) {
            break;      // Exit if semaphore is not given
        }

        if (write_index != read_index) {
            int16_t *read_ptr = &circular_buffer[read_index];

            // Calculate energy of the buffer segment
            float energy = calculate_short_time_energy(read_ptr, BUFFER_SIZE);
            ESP_LOGI(TAG, "Calculated energy: %f", energy);

            // silence detection and skip saving
            if (energy >= ENERGY_THRESHOLD) {
                fwrite(read_ptr, sizeof(int16_t), BUFFER_SIZE, current_file);
            
                current_file_size += BUFFER_SIZE * sizeof(int16_t);
                ESP_LOGI(TAG, "Written %d bytes to file", current_file_size);
            }

            if (current_file_size >= MAX_FILE_SIZE) {
                ESP_LOGI(TAG, "Closing file 1");
                open_new_file();
            }

            // Update read index after writing
            read_index = (read_index + BUFFER_SIZE) % TOTAL_BUFFER_SIZE;

            // Signal the capture task that space is available in the buffer
            xSemaphoreGive(bufferEmptySemaphore);
        }
    }

    // Close the file when done
    if (current_file != NULL) {
        fclose(current_file);
        current_file = NULL;
    }
    ESP_LOGI(TAG, "SD writing task completed.");
    vTaskDelete(NULL);  // Delete the task
}


void gpio_task(void *param) {
    while (1) {
        bool button_status = gpio_get_level(GPIO_INPUT_PIN_RECORDER_CONTROL);

        if (button_status && !is_capturing) {
            ESP_LOGI(TAG, "Switched on, start recording....");

            is_capturing = true;
            i2s_channel_enable(rx_handle);

            xTaskCreate(capture_audio_task, "capture_audio_task", 4096, &rx_handle, 2, NULL);
            xTaskCreate(write_to_sd_task, "write_to_sd_task", 4096, NULL, 1, NULL);
        } else if (!button_status && is_capturing) {
            ESP_LOGI(TAG, "Switched off, strop recording....");
            is_capturing = false; 
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);  // Debounce delay
    }
}

void app_main(void)
{
    // Initialize the SD card
    if (sd_card_init() != ESP_OK) {
        printf("Failed to initialize SD card\n");
        return;
    }

    // Initialized i2s for micphone
    init_microphone(&rx_handle);

    // Simulate triggering SD card write after some delay
    vTaskDelay(2000 / portTICK_PERIOD_MS);
   
    // Initialized GPIO INPUT/OUTPUT
    gpio_init();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create semaphores for buffer synchronization
    bufferReadySemaphore = xSemaphoreCreateBinary();
    bufferEmptySemaphore = xSemaphoreCreateCounting(NUM_SEGMENTS, NUM_SEGMENTS);       // Initialize as available
    
    // Create tasks for capturing audio and writing to SD card
    xTaskCreate(capture_audio_task, "capture_audio_task", 4096, &rx_handle, 2, NULL);  
    xTaskCreate(write_to_sd_task, "write_to_sd_task", 4096, NULL, 1, NULL);            
    
    // Create task for monitoring GPIO and controlling LED
    xTaskCreate(gpio_task, "gpio_task", 4096, NULL, 1, NULL);                         

    // Create task for handling WiFi requests
    xTaskCreate(handleWiFiRequests, "handleWiFiRequests", 4096, NULL, 1, NULL);
}
