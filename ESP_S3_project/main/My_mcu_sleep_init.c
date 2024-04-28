#include "soc/soc_caps.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "hal/gpio_types.h"
#include "My_mcu_sleep_init.h"

#define ALL_IN_DEEP_SLEEP_MODE 0
#define SOMETHING_IN_DEEP_SLEEP_MODE 1

void sleep_init(void)
{
#if ALL_IN_DEEP_SLEEP_MODE
    esp_sleep_pd_config(ESP_PD_DOMAIN_MAX, ESP_PD_OPTION_AUTO);
#endif
#if SOMETHING_IN_DEEP_SLEEP_MODE
    esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_MODEM, ESP_PD_OPTION_AUTO);
    esp_sleep_pd_config(ESP_PD_DOMAIN_CPU, ESP_PD_OPTION_AUTO);
#endif

    esp_sleep_enable_ext1_wakeup(GPIO_NUM_5,
                                 ESP_EXT1_WAKEUP_ANY_HIGH);
#if !SOC_PM_SUPPORT_RTC_PERIPH_PD // 不支持RTC控制器
    gpio_pullup_dis(GPIO_NUM_5);
    gpio_pulldown_en(GPIO_NUM_5);
#else // 支持RTC控制器, 关闭RTC控制域下仅维持GPIO5的电源
    rtc_gpio_isolate(GPIO_NUM_5);
    rtc_gpio_pullup_dis(GPIO_NUM_5);
    rtc_gpio_pulldown_en(GPIO_NUM_5);
#endif
}
