#include "gpio.h"


void gpio_init()
{
    // Configure the GPIO pin as input with pull-up resistor
    gpio_config_t recorder_pin_io_conf;
    recorder_pin_io_conf.intr_type = GPIO_INTR_DISABLE;         // Disable interrupt
    recorder_pin_io_conf.mode = GPIO_MODE_INPUT;                // Set as input mode
    recorder_pin_io_conf.pin_bit_mask = (1ULL << GPIO_INPUT_PIN_RECORDER_CONTROL); // Set the GPIO pin number
    recorder_pin_io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;  // Disable pull-down mode
    recorder_pin_io_conf.pull_up_en = GPIO_PULLUP_ENABLE;       // Enable pull-up mode
    gpio_config(&recorder_pin_io_conf);


    // Configure the GPIO pin as input with pull-up resistor
    gpio_config_t D2_Pin_io_conf;
    D2_Pin_io_conf.intr_type = GPIO_INTR_DISABLE;               // Disable interrupt
    D2_Pin_io_conf.mode = GPIO_MODE_INPUT;                      // Set as input mode
    D2_Pin_io_conf.pin_bit_mask = (1ULL << GPIO_INPUT_PIN_2);  // Set the GPIO pin number
    D2_Pin_io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;        // Disable pull-down mode
    D2_Pin_io_conf.pull_up_en = GPIO_PULLUP_ENABLE;             // Enable pull-up mode
    gpio_config(&D2_Pin_io_conf);

    
    // LED
    //gpio_set_direction(GPIO_ON_BOARD_STATUS_LED, GPIO_MODE_OUTPUT);
}

 