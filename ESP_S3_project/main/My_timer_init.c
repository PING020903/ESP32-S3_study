#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "sdkconfig.h"
#include "My_timer_init.h"
#include "My_https_request.h"

#define TIME_PERIOD (86400000000ULL)
#define SNTP_REQUEST_TASK HTTPS_REQUEST_TASK
#define MY_DEEP_SLEEP 1
#define LOG_PORT 0
// 任务队列(指针)
QueueSetHandle_t task_evt_queue = NULL;
// 定时器句柄

static esp_timer_handle_t My_timer_dispatch;   // 主任务
static esp_timer_handle_t nvs_update_timer;    // SNTP
static esp_timer_handle_t https_request_timer; // HTTPS_REQUEST
#if MY_DEEP_SLEEP
static esp_timer_handle_t deep_sleep_timer; // 休眠
#endif
#if LOG_PORT
static esp_timer_handle_t log_port_timer; // change LOG output port
#endif

// 日志标签
static const char *TAG = "USER_timer";

char *My_timer_mode_str(int My_task_mode)
{
    switch (My_task_mode)
    {
    case MAIN_TASK:
        return "MAIN_TASK";

    case MY_HTTPS_REQUEST_TASK:
        return "MY_HTTPS_REQUEST_TASK";

    case SNTP_SYNC:
        return "SNTP_SYNC";

    case MCU_DEEP_SLEEP:
        return "MCU_DEEP_SLEEP";
#if LOG_PORT
    case CHANGE_LOG_PORT:
        return "CHANGE_LOG_PORT";
#endif

    default:
        return "UNKNOWN";
    }
}

esp_timer_handle_t My_timer_select(int My_task_mode)
{
    esp_timer_handle_t timer;
    switch (My_task_mode)
    {
    case MAIN_TASK:
        timer = My_timer_dispatch;
        break;

    case MY_HTTPS_REQUEST_TASK:
        timer = https_request_timer;
        break;

    case SNTP_SYNC:
        timer = nvs_update_timer;
        break;

    case MCU_DEEP_SLEEP:
        timer = deep_sleep_timer;
        break;
#if LOG_PORT
    case CHANGE_LOG_PORT:
        timer = log_port_timer;
        break;
#endif

    default:
        ESP_LOGW(TAG, "arg fail");
        timer = NULL;
        break;
    }

    return timer;
}

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

/*
    HTTP请求任务定时器回调函数
*/
void https_request_timer_callback(void *arg)
{
    // ESP_LOGI(TAG, " %s, timer cb", __func__);
    uint32_t task_mark = MY_HTTPS_REQUEST_TASK;
    xQueueSend(task_evt_queue, &task_mark, 0);
    return;
}

void deep_sleep_timer_callback(void *arg)
{
    uint32_t task_mark = MCU_DEEP_SLEEP;
    xQueueSend(task_evt_queue, &task_mark, 0);
    return;
}

#if LOG_PORT
void change_LogPort_timer_callback(void *arg)
{
    uint32_t task_mark = CHANGE_LOG_PORT;
    xQueueSend(task_evt_queue, &task_mark, 0);
    return;
}
#endif
/*
    定时器初始化
*/
void My_timer_init()
{
    // 创建任务队列, 定义保留数据项的大小
    task_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // 初始化ESP定时器参数

    // 主任务定时器参数
    const esp_timer_create_args_t My_timer_args = {
        .callback = &My_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "My_main_timer",
        .skip_unhandled_events = false,
    };

#if HTTPS_REQUEST_TIMER
    // HTTPS请求定时器参数
    const esp_timer_create_args_t https_request_args = {
        .callback = &https_request_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "https_request_timer",
        .skip_unhandled_events = true, // 多次超时不重复响应callback
    };
#endif

#if SNTP_REQUEST_TASK
    // SNTP请求定时器参数
    const esp_timer_create_args_t nvs_update_timer_args = {
        .callback = (void *)&fetch_and_store_time_in_nvs,
        .name = "update time from NVS",
        .arg = NULL,
        .skip_unhandled_events = false,
        .dispatch_method = ESP_TIMER_TASK,
    };
#endif
#if MY_DEEP_SLEEP
    // 深度休眠定时器参数
    const esp_timer_create_args_t Deep_sleep_timer_args = {
        .callback = deep_sleep_timer_callback,
        .name = "Deep sleep timer",
        .arg = NULL,
        .skip_unhandled_events = false,
        .dispatch_method = ESP_TIMER_TASK,
    };
#endif
#if LOG_PORT
    // 更改串口输出端口定时器参数
    const esp_timer_create_args_t change_LogPort_tiemr_args = {
        .callback = change_LogPort_timer_callback,
        .name = "Change LogPort timer",
        .arg = NULL,
        .skip_unhandled_events = false,
        .dispatch_method = ESP_TIMER_TASK,
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
#if MY_DEEP_SLEEP
    ESP_ERROR_CHECK(esp_timer_create(&Deep_sleep_timer_args, &deep_sleep_timer));
#endif
#if LOG_PORT
    ESP_ERROR_CHECK(esp_timer_create(&change_LogPort_tiemr_args, &log_port_timer));
#endif
    ESP_LOGI(TAG, "Timer create ...");

    // 启动定时器(us)
    ESP_ERROR_CHECK(esp_timer_start_periodic(My_timer_dispatch, 10000ULL)); // 10ms
#if HTTPS_REQUEST_TIMER
    ESP_ERROR_CHECK(esp_timer_start_periodic(https_request_timer, 30000000ULL)); // 30s
#endif
#if SNTP_REQUEST_TASK
    ESP_ERROR_CHECK(esp_timer_start_periodic(nvs_update_timer, TIME_PERIOD)); // 24hours, update NVS time
#endif
#if MY_DEEP_SLEEP
    ESP_ERROR_CHECK(esp_timer_start_once(deep_sleep_timer, 40000000ULL)); // 40s
#endif
#if LOG_PORT
    ESP_ERROR_CHECK(esp_timer_start_once(log_port_timer, 5000000ULL)); // 5s
#endif
    ESP_LOGI(TAG, "Timer start ...");
    return;
}

void My_timer_delete(int My_task_mode)
{
    esp_err_t ret;
    esp_timer_handle_t timer = My_timer_select(My_task_mode);
    ret = esp_timer_delete(timer); // 已经停止的定时器将会被直接删除
    if (ret != ESP_OK)
    {
        ret = esp_timer_stop(timer); // 未停止的定时器先停止再删除
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "%s Timer stop error!",
                     My_timer_mode_str(My_task_mode));
            return;
        }
        else
        {
            ret = esp_timer_delete(timer);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, " %s Timer delete error!\n%s",
                         My_timer_mode_str(My_task_mode),
                         esp_err_to_name(ret));
                return;
            }
            else
                return;
        }
    }
    else
    {
        ESP_LOGI(TAG, "%s Timer delete ...", My_timer_mode_str(My_task_mode));
        return;
    }

    return;
}

void My_timer_stop(int My_task_mode)
{
    esp_timer_handle_t timer = My_timer_select(My_task_mode);

    esp_err_t ret = esp_timer_stop(timer);
    switch (ret)
    {
    case ESP_ERR_INVALID_STATE:
        ESP_LOGW(TAG, "%s timer is not running",
                 My_timer_mode_str(My_task_mode));
        break;

    case ESP_OK:
        ESP_LOGI(TAG, "%s Timer stop ...",
                 My_timer_mode_str(My_task_mode));
        break;

    default:
        ESP_LOGW(TAG, "%s Timer stop failed! !\n%s",
                 My_timer_mode_str(My_task_mode),
                 esp_err_to_name(ret));
        break;
    }

    return;
}
