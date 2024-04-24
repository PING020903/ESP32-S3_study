#ifndef _MY_TIMER_INIT_H_
#define _MY_TIMER_INIT_H_


#define ESP_INTR_FLAG_DEFAULT 0
#define HTTPS_REQUEST_TIMER 1

enum
{
    MAIN_TASK = 1,
    MY_HTTPS_REQUEST_TASK,
};

void My_timer_callback(void *arg);
void My_timer_init();
#endif