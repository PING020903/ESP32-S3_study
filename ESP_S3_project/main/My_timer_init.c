#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "My_timer_init.h"
#include "My_https_request.h"

#define TIME_PERIOD (86400000000ULL)
#define SNTP_REQUEST_TASK HTTPS_REQUEST_TASK
// 任务队列(指针)
QueueSetHandle_t task_evt_queue = NULL;
// 定时器句柄
static esp_timer_handle_t My_timer_dispatch;
static esp_timer_handle_t nvs_update_timer;
static esp_timer_handle_t https_request_timer;

// 日志标签
static const char *TAG = "USER_timer";

/*
    定时器回调函数
*/
void My_timer_callback(void *arg)
{
    // ESP_LOGI(TAG, " %s, timer cb", __func__);
    uint32_t task_mark = MAIN_TASK;
    xQueueSend(task_evt_queue, &task_mark, 0);
    return;
}

void https_request_timer_callback(void *arg)
{
    // ESP_LOGI(TAG, " %s, timer cb", __func__);
    uint32_t task_mark = MY_HTTPS_REQUEST_TASK;
    xQueueSend(task_evt_queue, &task_mark, 0);
    return;
}

/*
    定时器初始化
*/
void My_timer_init()
{
    // 创建任务队列, 定义保留数据项的大小
    task_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // 初始化ESP定时器参数
    const esp_timer_create_args_t My_timer_args = {
        .callback = &My_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_INTR_FLAG_DEFAULT,
        .name = "My_main_timer",
        .skip_unhandled_events = false,
    };

#if HTTPS_REQUEST_TIMER
    const esp_timer_create_args_t https_request_args = {
        .callback = &https_request_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_INTR_FLAG_DEFAULT,
        .name = "https_request_timer",
        .skip_unhandled_events = false,
    };
#endif

#if SNTP_REQUEST_TASK
    const esp_timer_create_args_t nvs_update_timer_args = {
        .callback = (void *)&fetch_and_store_time_in_nvs,
        .name = "update time from NVS",
        .arg = NULL,
        .skip_unhandled_events = false,
        .dispatch_method = ESP_INTR_FLAG_DEFAULT,
    };
#endif
    ESP_LOGI(TAG, "Timer structture init ...");

    // 创建定时器(参数, 句柄)
    ESP_ERROR_CHECK(esp_timer_create(&My_timer_args, &My_timer_dispatch));
#if HTTPS_REQUEST_TIMER
    ESP_ERROR_CHECK(esp_timer_create(&https_request_args, &https_request_timer));
#endif
#if SNTP_REQUEST_TASK
    ESP_ERROR_CHECK(esp_timer_create(&nvs_update_timer_args, &nvs_update_timer));
#endif
    ESP_LOGI(TAG, "Timer create ...");

    // 启动定时器(us)
    ESP_ERROR_CHECK(esp_timer_start_periodic(My_timer_dispatch, 100000ULL)); // 100ms
    #if HTTPS_REQUEST_TIMER
    ESP_ERROR_CHECK(esp_timer_start_periodic(https_request_timer, 30000000ULL)); // 30s
#endif
#if SNTP_REQUEST_TASK
    ESP_ERROR_CHECK(esp_timer_start_periodic(nvs_update_timer, TIME_PERIOD)); // 24hours, update NVS time
#endif
    ESP_LOGI(TAG, "Timer start ...");
    return;
}