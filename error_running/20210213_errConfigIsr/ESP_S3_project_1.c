#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"

static volatile unsigned char handler_count = 0;        // 中断触发次数
static QueueSetHandle_t gpio_evt_queue = NULL; // 中断队列(指针)
static const char* TAG = "Info";
//static int Main_timer;

gpio_config_t My_GPIO_structture = {};
//void My_timer_callback(TimerHandle_t xTimer);

/*
#define IRAM_ATTR _SECTION_ATTR_IMPL(".iram1", __COUNTER__)
Forces code into IRAM instead of flash
扩展到:

__attribute__((section(".iram1" "." "29")))
//GCC的一种编译器命令，用来指示编译器执行实现某些高级操作
//section控制变量或函数在编译时的段名。在嵌入式软件开发时用的非常多，比如有外扩Flash或RAM时，需要将变量或函数放置到外扩存储空间，可以在链接脚本中指定段名来操作。在使用MPU(存储保护)的MCU编程时，需要对存储器划分区域，将变量或代码放置到对应的区域，通常也是通过段操作来实现。

//const int identifier[3] __attribute__ ((section ("ident"))) = { 1,2,3 };
//void myfunction (void) __attribute__ ((section ("ext_function")))

//上述代码分别在编译后，数组和函数所在的段分别为“indent”和“ext_function”。
*/
static void IRAM_ATTR gpio_isr_handler( void* arg ) // 中断处理
{
    // ESP_LOGI(TAG, " gpio isr handler, count = %d", (uint32_t)arg);
    handler_count++;
    uint32_t gpio_num = ( uint32_t ) arg;
    xQueueSendFromISR( gpio_evt_queue, &gpio_num, NULL );
    return;
}

/*static void MY_timer_init()
{
    Main_timer = xTimerCreate("Main_timer", 1000 / portTICK_PERIOD_MS, pdTRUE, NULL, My_timer_callback);//定时器
    xTimerStart(Main_timer, 0);
}
static void My_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, " %s, timer cb", __func__);
    uint32_t gpio_num = 0;
    uint32_t gpio_status = 0;
    while (xQueueReceive(gpio_evt_queue, &gpio_num, 0) == pdTRUE)
    {
        gpio_status = gpio_get_level(gpio_num);
    }
}*/

static void My_gpio_init()
{
    gpio_evt_queue = xQueueCreate( 10, sizeof( uint32_t ) );

    gpio_install_isr_service( 0 );                                               // 安装中断服务
    gpio_isr_handler_add( GPIO_NUM_5, gpio_isr_handler, ( void* ) handler_count ); // 注册中断处理函数

    My_GPIO_structture.pin_bit_mask = (1ULL << 5); // GPIO5, 1左移5bit
    My_GPIO_structture.mode = GPIO_MODE_INPUT;
    My_GPIO_structture.pull_up_en = GPIO_PULLUP_DISABLE;
    My_GPIO_structture.pull_down_en = GPIO_PULLDOWN_ENABLE;
    My_GPIO_structture.intr_type = GPIO_INTR_NEGEDGE; // 下降中断, falling edge
    // gpio_config(&My_GPIO_structture);//配置 GPIO5
    ESP_ERROR_CHECK( gpio_config( &My_GPIO_structture ) ); // 配置 GPIO5且使用ESP_ERROR检查

    My_GPIO_structture.pin_bit_mask = (1ULL << 6);
    My_GPIO_structture.mode = GPIO_MODE_OUTPUT;
    My_GPIO_structture.pull_up_en = GPIO_PULLUP_ENABLE;
    My_GPIO_structture.pull_down_en = GPIO_PULLDOWN_DISABLE;
    My_GPIO_structture.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK( gpio_config( &My_GPIO_structture ) ); // 配置 GPIO6且使用ESP_ERROR检查

    return;
}

static void configure_led( void )
{
    ESP_LOGI( TAG, "Example configured to blink GPIO LED!" );
    return;
}
void app_main( void )
{

    int gpio_level;
    uint32_t io_num = 0;
    // EE.ANDQ qa, qx, qy
    My_gpio_init();
    configure_led();
    unsigned int count = 0;
    while ( 1 )
    {
        vTaskDelay( 100 );
        if ( xQueueReceive( gpio_evt_queue, &io_num, portMAX_DELAY ) )
        {
            printf( "GPIO[%" PRIu32 "] intr, val: %d, handler count: %d\n", io_num, gpio_get_level( GPIO_NUM_5 ), handler_count );
        }
        ESP_LOGI( TAG, "'%s' MY log ! %d, GPIO 6 is high...", __func__, count++ );
        gpio_level = gpio_get_level( GPIO_NUM_5 );
        if ( gpio_level )
            ESP_LOGI( TAG, "GPIO 5 is high" );
        else
            ESP_LOGI( TAG, "GPIO 5 is low" );

        if ( handler_count >= 2 )
        {
            gpio_isr_handler_remove( GPIO_NUM_5 );
            printf( "remove gpio interrupt handler remove ! ! !\n" );
        }
    }
}
