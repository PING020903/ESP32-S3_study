/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

// DESCRIPTION:
// This example contains minimal code to make ESP32-S2 based device
// recognizable by USB-host devices as a USB Serial Device printing output from
// the application.

#include <stdio.h>
#include <stdlib.h>
#include <sys/reent.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "My_timer_init.h"
#include "My_gpio_init.h"
#include "My_usb_device.h"
#include "My_WIFI_init.h"
#include "My_https_request.h"
#include "My_LED_init.h"
#include "My_mcu_sleep_init.h"
#include "sdkconfig.h"

#define USER_USB_INIT 1
#define ENABLE_D_SLEEP 0
#define TEST 1
#define UART_USB_SWITCH 0
#define ENABLE_USB_BUT_USE_UART_LOG 1

// 任务队列(指针)
extern QueueSetHandle_t task_evt_queue;
// 日志标签
static const char *TAG = "USER_app";
// 计数变量
static volatile unsigned long long P_conut = 0;

extern void wifi_init_sta(void);

void My_main_task(void *arg)
{
    int gpio_level[2];
    esp_err_t tusb_ret;
    uint32_t task_mark;
    TickType_t count;
    uint32_t led_level = 0;
    int64_t start_time;
    unsigned int output = 0;

    while (1)
    {
        UBaseType_t msg_cnt = uxQueueMessagesWaiting(task_evt_queue); // 先查询队列
        if (msg_cnt)
        {
            /* 该函数是阻塞的 */
            BaseType_t ret = xQueueReceive(task_evt_queue, &task_mark, portMAX_DELAY);
            P_conut++;
        }
        else
        {
            task_mark = NOTHING_TASK;
        }

        // UBaseType_t ab = uxQueueMessagesWaiting(task_evt_queue);
        switch (task_mark)
        {
        case MAIN_TASK:
        {

            start_time = esp_timer_get_time();

            ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_48, led_level));
            led_level = !led_level;
            count = xTaskGetTickCount();
            gpio_level[0] = gpio_get_level(GPIO_NUM_5);
            gpio_level[1] = gpio_get_level(GPIO_NUM_1);

            if (P_conut % 100 == 0) // 若1s打印一次, 定时器10ms发一次队列指令, 1000/10 = 100
            {
#if ENABLE_USB_BUT_USE_UART_LOG
                while (output < 2)
                {
                    ESP_ERROR_CHECK(My_tusb_streams_change(output));
                    if (output == 0)
                        break;
                    if (output == 1)
                        output += 2;
                }
                if (output < 2)
                    output++;
#endif

                ESP_LOGI(TAG, "GPIO1: %d, GPIO5: %d", gpio_level[1], gpio_level[0]);
                ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
                ESP_LOGI(TAG, "Largest free block size: %d", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
                esp_sleep_wakeup_cause_t info = esp_sleep_get_wakeup_cause();
                ESP_LOGI(TAG, "Wakeup reason: %d, IO: 0x%llx",
                         info, esp_sleep_get_ext1_wakeup_status());

#if USER_USB_INIT

                // tusb_ret =  esp_tusb_init_console(TINYUSB_CDC_ACM_0); // log to usb
                if (!output)
                {
#endif
                    ESP_LOGI(TAG, "log -> USB");
                    ESP_LOGW(TAG, "log -> USB");
                    ESP_LOGE(TAG, "log -> USB\n");
#if USER_USB_INIT
                }
                else
                {
                    // esp_tusb_deinit_console(TINYUSB_CDC_ACM_0); // log to uart
                    // My_tusb_streams_change(1); // 更改控制台USB串口控制权归还UART
#endif
                    ESP_LOGI(TAG, "log -> uart");
                    ESP_LOGW(TAG, "log -> uart");
                    ESP_LOGE(TAG, "log -> uart\n");
#if USER_USB_INIT
                }
#endif
            }
            else
                vTaskDelay(1);
        }
        break;
        case MY_HTTPS_REQUEST_TASK:
        {
#if HTTPS_REQUEST_TIMER
            only_https_request_init();
#endif
        }
        case MCU_DEEP_SLEEP:
        {
#if ENABLE_D_SLEEP
            sleep_init();
#endif
#if ENABLE_D_SLEEP
            ESP_LOGW(TAG, "mcu will deep sleep...");
#if TEST
            /* 尝试停止其他定时器导致深度休眠复苏 */
            My_timer_stop(MAIN_TASK);
            My_timer_stop(MY_HTTPS_REQUEST_TASK);

            My_wifi_stop();
#endif
            esp_deep_sleep_start();
#endif
        }
        break;
        case CHANGE_LOG_PORT:
        {

#if UART_USB_SWITCH
            output = !output; // 0: USB, 1: UART
#endif
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
}

void My_task_init(void)
{
    // 该函数实际上是调用xTaskCreatePinnedToCore()函数
    // xTaskCreate(My_main_task, "My_main_task", 2048, NULL, 8, NULL);

    // 任务创建(函数入口地址, 任务名称, 任务堆栈大小, 任务参数, 任务优先级, 任务句柄, 任务创建的CPU核心编号)
    xTaskCreatePinnedToCore(My_main_task, "My_main_task", 4096, NULL, 8, NULL, 1);

    // ps: 经过测试, 同优先级下, 先打印输出了Core_0, 再打印输出了Core_1
    // xTaskCreatePinnedToCore(My_main_task_2, "My_main_task_2", 4096, NULL, 8, NULL, 0);

    return;
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP-IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "heap: %d", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    My_gpio_init();
    My_LED_init();
    wifi_init_sta();
    https_request_init();

#if USER_USB_INIT
    My_usb_device_init();
#endif
    My_timer_init();
    My_task_init();
}
