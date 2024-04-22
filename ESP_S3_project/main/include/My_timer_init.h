#ifndef _MY_TIMER_INIT_H_
#define _MY_TIMER_INIT_H_
#include "esp_timer.h"

#define ESP_INTR_FLAG_DEFAULT 0

enum
{
    MAIN_TASK = 1,
    SUB_TASK,
};

void My_timer_callback(void *arg);
void My_timer_init();
#endif