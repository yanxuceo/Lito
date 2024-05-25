#ifndef _GPIO_H
#define _GPIO_H

#include "driver/gpio.h"
#include "soc/gpio_num.h"


#define GPIO_INPUT_PIN_RECORDER_CONTROL    GPIO_NUM_1
#define GPIO_INPUT_PIN_2                   GPIO_NUM_2

#define GPIO_ON_BOARD_STATUS_LED           GPIO_NUM_21


void gpio_init();

#endif