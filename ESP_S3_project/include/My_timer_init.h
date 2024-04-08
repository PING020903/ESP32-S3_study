#ifndef _MY_TIMER_INIT_H_
#define _MY_TIMER_INIT_H_

#define ESP_INTR_FLAG_DEFAULT 0

void My_timer_callback(void *arg);
void My_timer_init();
#endif