#ifndef _MY_GPIO_INIT_H_
#define _MY_GPIO_INIT_H_
#include "driver/gpio.h"
#include "driver/gpio_filter.h"

#define ESP_INTR_FLAG_DEFAULT 0

void gpio_isr_handler(void *arg);
void gpio_isr_handler_receive_task(void *arg);
void My_gpio_init();

#endif