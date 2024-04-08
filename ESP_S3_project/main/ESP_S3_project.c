#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_idf_version.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
#include "xtensa_perfmon_access.h" // 任务性能计数器头文件
#include "MY_GPIO_init.h"
#include "MY_timer_init.h"
#include "MY_WIFI_init.h"
#include "MY_https_request.h"

// 任务队列(指针)
extern QueueSetHandle_t task_evt_queue;
// 日志标签
static const char *TAG = "USER_app";
// 计数变量
static volatile unsigned long long P_conut = 0;

// static const char raw_buffer[] = {"hello, this is info, send form ESP-S3 ! ! !"};

#if 0
static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    return;
}
#endif

/*
    用户主任务
*/
void My_main_task(void *arg)
{
    int gpio_level;
    uint32_t task_mark;
    TickType_t count = 0;
    unsigned int led_level = 0;
    int64_t start_time = esp_timer_get_time();

    while (1)
    {
        P_conut++;
        /* 该函数是阻塞的 */
        BaseType_t ret = xQueueReceive(task_evt_queue, &task_mark, portMAX_DELAY);
        // UBaseType_t ab = uxQueueMessagesWaiting(task_evt_queue);
        switch (task_mark)
        {
        case 1:
        {
            start_time = esp_timer_get_time();

            ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_48, led_level));
            led_level = !led_level;
            count = xTaskGetTickCount();
            gpio_level = gpio_get_level(GPIO_NUM_5);
#if 0
            if (P_conut % 1000 == 0)
                ESP_ERROR_CHECK(esp_wifi_connect());
#endif

            if (P_conut % 20 == 0) // 若1s打印一次, 定时器1ms发一次队列指令, 1000/1 = 1000
            {
                // ESP_LOGI(TAG, "RUN \"esp_wifi_80211_tx\"...");

                /* 该函数目前一旦调用就导致OS重启 */
                // ESP_ERROR_CHECK(esp_wifi_80211_tx(WIFI_IF_STA, raw_buffer, 44U, true));

                ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
                printf("sizeof(int)%d, sizeof(long)%d, start_time:%lld\n", sizeof(int), sizeof(long long), start_time);
                ESP_LOGI(TAG, "'%s' MY log ... %lu   led: %u\n", __func__, count, led_level);
                if (gpio_level)
                    ESP_LOGI(TAG, "GPIO 5 is high");
                else
                    ESP_LOGI(TAG, "GPIO 5 is low");
            }
            else
                vTaskDelay(1);
        }
        break;

        default:
        {
            vTaskDelay(1);
        }
        break;
        }
#if 0
        vTaskDelay(1000); // 10000ms延时
        ESP_LOGI(TAG, "MY task running on core_1 ... \n");
#endif
    }
    return;
}
void My_main_task_2(void *arg)
{
    while (1)
    {
        vTaskDelay(100);
        ESP_LOGI(TAG, "MY task running on core_0 ... \n");
    }
    return;
}

/*
    用户主任务初始化
*/
void My_task_init(void)
{
    // 该函数实际上是调用xTaskCreatePinnedToCore()函数
    // xTaskCreate(My_main_task, "My_main_task", 2048, NULL, 8, NULL);
    
    // 任务创建(函数入口地址, 任务名称, 任务堆栈大小, 任务参数, 任务优先级, 任务句柄, 任务创建的CPU核心编号)
    xTaskCreatePinnedToCore(My_main_task, "My_main_task", 2048, NULL, 8, NULL, 1);

    // ps: 经过测试, 同优先级下, 先打印输出了Core_0, 再打印输出了Core_1
    // xTaskCreatePinnedToCore(My_main_task_2, "My_main_task_2", 4096, NULL, 8, NULL, 0);

    return;
}

void app_main(void)
{
    // char task_report[200] = {0};
    ESP_LOGI(TAG, "ESP-IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
    My_gpio_init();
    My_timer_init();
    wifi_init_sta();
    https_request_init();
#if MY_ESP_NOW
    ESP_ERROR_CHECK(ESP_NOW_init());
#endif
    My_task_init();// 用户任务保持放在最后, 可减少任务堆栈的占用, 否则只能设置到4096启动, 不然OS_restart
    
    //  ESP_ERROR_CHECK(esp_timer_init());
    // uxTaskGetSnapshotAll
    
    // vTaskList(task_report);
    // printf("%s", task_report);
#if 0
        if (handler_count >= 2)
        {
            gpio_isr_handler_remove(GPIO_NUM_5);
            printf("remove gpio interrupt handler remove ! ! !\n");
        }
#endif
}
