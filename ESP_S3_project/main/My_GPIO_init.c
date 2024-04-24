#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/gpio_filter.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "MY_GPIO_init.h"



// 中断触发次数
static volatile unsigned char handler_count = 0;
// 中断队列(指针)
static QueueSetHandle_t gpioISR_evt_queue = NULL;
// GPIO结构体
static gpio_config_t My_GPIO_structture = {};
// GPIO毛刺过滤器结构体
static gpio_pin_glitch_filter_config_t My_GPIO_filter_structture = {};
// GPIO毛刺过滤器句柄
static gpio_glitch_filter_handle_t My_GPIO_filter_handler;

/*
 // 中断处理
#define IRAM_ATTR _SECTION_ATTR_IMPL(".iram1", __COUNTER__)
Forces code into IRAM instead of flash
扩展到:

__attribute__((section(".iram1" "." "29")))
//GCC的一种编译器命令，用来指示编译器执行实现某些高级操作
//section控制变量或函数在编译时的段名。在嵌入式软件开发时用的非常多，
比如有外扩Flash或RAM时，需要将变量或函数放置到外扩存储空间，可以在链接脚本中指定段名来操作。
在使用MPU(存储保护)的MCU编程时，需要对存储器划分区域，将变量或代码放置到对应的区域，通常也是通过段操作来实现。

//const int identifier[3] __attribute__ ((section ("ident"))) = { 1,2,3 };
//void myfunction (void) __attribute__ ((section ("ext_function")))

//上述代码分别在编译后，数组和函数所在的段分别为“indent”和“ext_function”。
*/
void IRAM_ATTR gpio_isr_handler(void *arg)
{
    // 中断任务内部不执行打印
    // ESP_LOGI(TAG, " gpio isr handler, count = %d", (uint32_t)arg);
    handler_count++;
    uint32_t gpio_num = (uint32_t)arg;

    // 从中断任务中发送数据到任务队列, 从中断中使用比较安全
    xQueueSendFromISR(gpioISR_evt_queue, &gpio_num, NULL);
    return;
}

/*
    中断队列处理任务
*/
void gpio_isr_handler_receive_task(void *arg)
{
    uint32_t io_num = 0;
    while (1)
    {
        if (xQueueReceive(gpioISR_evt_queue, &io_num, portMAX_DELAY))
        {
            printf("%s GPIO[%" PRIu32 "] intr, val: %d, handler count: %d\n", __func__, io_num, gpio_get_level(io_num), handler_count);
        }
    }
}

/*
    GPIO初始化
*/
void My_gpio_init()
{
    // 创建中断队列
    gpioISR_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // 配置GPIO毛刺过滤器结构体
    My_GPIO_filter_structture.clk_src = 0;
    My_GPIO_filter_structture.gpio_num = GPIO_NUM_5;

    /*
    TaskHandle_t xTaskCreateStatic(
        TaskFunction_t pxTaskCode,
        const char * const pcName,
        const uint32_t ulStackDepth,
        void * const pvParameters,
        UBaseType_t uxPriority,
        StackType_t * const puxStackBuffer,
        StaticTask_t * const pxTaskBuffer);
    BaseType_t xTaskCreate(
        TaskFunction_t pxTaskCode,
        const char * const pcName,
        const configSTACK_DEPTH_TYPE usStackDepth,
        void * const pvParameters,
        UBaseType_t uxPriority,
        TaskHandle_t * const pxCreatedTask);
    */
    // start gpio_isr task, 创建gpio中断处理接收任务
    xTaskCreatePinnedToCore(gpio_isr_handler_receive_task, "gpio_isr_handler_task", 2048, NULL, 10, NULL, 1);

    // install gpio isr service, 安装gpio中断服务
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));

    // hook isr handler for specific gpio pin, 添加gpio中断处理函数
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_5, gpio_isr_handler, (void *)GPIO_NUM_5));

    My_GPIO_structture.pin_bit_mask = (1ULL << 5);       // GPIO5, 1左移5bit
    My_GPIO_structture.mode = GPIO_MODE_INPUT;           // 输入模式
    My_GPIO_structture.pull_up_en = GPIO_PULLUP_DISABLE; // 上拉电阻, pull up; 下拉电阻, pull down
    My_GPIO_structture.pull_down_en = GPIO_PULLDOWN_ENABLE;
    My_GPIO_structture.intr_type = GPIO_INTR_NEGEDGE; // 下降中断, falling edge(GPIO_INTR_NEGEDGE); 上升中断, rising edge(GPIO_INTR_POSEDGE)
    // gpio_config(&My_GPIO_structture);//配置 GPIO5
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 配置 GPIO5且使用ESP_ERROR检查

    /*
    //change gpio interrupt type for one pin
    gpio_set_intr_type(GPIO_NUM_5, GPIO_INTR_ANYEDGE);
    */

#if 0
    My_GPIO_structture.pin_bit_mask = (1ULL << 48); // GPIO48, 1左移48bit
    My_GPIO_structture.mode = GPIO_MODE_OUTPUT;
    My_GPIO_structture.pull_up_en = GPIO_PULLUP_DISABLE;
    My_GPIO_structture.pull_down_en = GPIO_PULLDOWN_ENABLE;
    My_GPIO_structture.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&My_GPIO_structture)); // 配置 GPIO48且使用ESP_ERROR检查
#endif

    // 为IO口配置输入去毛刺
    gpio_new_pin_glitch_filter(&My_GPIO_filter_structture, &My_GPIO_filter_handler);

    // 启用去毛刺   ps: 启用去毛刺后, 效果显著, 用手拔插杜邦线触发中断时不会太多次连续触发
    ESP_ERROR_CHECK(gpio_glitch_filter_enable(My_GPIO_filter_handler));

    return;
}