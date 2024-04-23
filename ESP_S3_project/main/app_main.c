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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MEM_LEAK 0
#include "My_timer_init.h"
#include "My_gpio_init.h"
#include "My_usb_device.h"
#include "My_LED_init.h"
#include "sdkconfig.h"

#if MEM_LEAK
#include "esp_heap_caps.h"
#endif

#define USER_USB_INIT 1
#define TEST 1

// 任务队列(指针)
extern QueueSetHandle_t task_evt_queue;
// 日志标签
static const char *TAG = "USER_app";
// 计数变量
static volatile unsigned long long P_conut = 0;

#if MEM_LEAK
void esp_heap_trace_alloc_hook(void *ptr, size_t size, uint32_t caps)
{
    printf("alloc_hook: %p, size: %d, caps: %ld\n", ptr, size, caps);
}

void esp_heap_trace_free_hook(void *ptr)
{
    printf("free_hook: %p\n", ptr);
}
#endif

void My_main_task(void *arg)
{
    int gpio_level;
    esp_err_t tusb_ret;
    uint32_t task_mark;
    TickType_t count = 0;
    uint32_t led_level = 0;
    int64_t start_time = esp_timer_get_time();

    while (1)
    {
        P_conut++;
        /* 该函数是阻塞的 */
        BaseType_t ret = xQueueReceive(task_evt_queue, &task_mark, portMAX_DELAY);
        // UBaseType_t ab = uxQueueMessagesWaiting(task_evt_queue);
        switch (task_mark)
        {
        case MAIN_TASK:
        {

            start_time = esp_timer_get_time();

            ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_48, led_level));
            led_level = !led_level;
            count = xTaskGetTickCount();
            gpio_level = gpio_get_level(GPIO_NUM_5);

            if (P_conut % 20 == 0) // 若1s打印一次, 定时器1ms发一次队列指令, 1000/1 = 1000
            {
                if (gpio_level)
                    ESP_LOGI(TAG, "GPIO 5 is high");
                else
                    ESP_LOGI(TAG, "GPIO 5 is low");
                ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
                ESP_LOGI(TAG, "Largest free block size: %d", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
#if USER_USB_INIT
                tusb_ret = ESP_OK;
                #if TEST
                My_tusb_streams_change(0);  // 更改控制台USB串口控制权
                #endif
                if (tusb_ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "esp_tusb_init_console failed: %d", tusb_ret);
                    break;
                }
                else
                {
                    ESP_LOGI(TAG, "log -> USB");
                    ESP_LOGW(TAG, "log -> USB");
                    ESP_LOGE(TAG, "log -> USB\n");
                }
                #if TEST
                My_tusb_streams_change(1);  // 更改控制台USB串口控制权归还UART
                #endif
#endif
                ESP_LOGI(TAG, "log -> uart");
                ESP_LOGW(TAG, "log -> uart");
                ESP_LOGE(TAG, "log -> uart\n");
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
#if !MEM_LEAK
    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
#else
    ESP_LOGI(TAG, "Minimum free heap size: %ld bytes\n", esp_get_minimum_free_heap_size());
#endif

#if USER_USB_INIT
    My_usb_device_init();
#endif
    My_gpio_init();
    My_timer_init();
    My_LED_init();
    My_task_init();
#if 0 // test tiny usb function memory lesk
    while (1) {
        ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
        ESP_LOGI(TAG, "log -> UART");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        fprintf(stdout, "example: print -> stdout\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        fprintf(stderr, "example: print -> stderr\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        esp_tusb_init_console(TINYUSB_CDC_ACM_0); // log to usb
        ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
        ESP_LOGI(TAG, "log -> USB");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        fprintf(stdout, "example: print -> stdout\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        fprintf(stderr, "example: print -> stderr\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_tusb_deinit_console(TINYUSB_CDC_ACM_0); // log to uart
    }
#endif
}
