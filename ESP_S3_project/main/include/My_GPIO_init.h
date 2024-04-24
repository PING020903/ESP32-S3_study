#ifndef _MY_GPIO_INIT_H_
#define _MY_GPIO_INIT_H_


#define ESP_INTR_FLAG_DEFAULT 0

void gpio_isr_handler(void *arg);
void gpio_isr_handler_receive_task(void *arg);
void My_gpio_init();

#endif