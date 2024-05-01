#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_wake_stub.h"
#include "esp_cpu.h"
#include "esp_rom_sys.h"
#include "driver/rtc_io.h"
#include "hal/gpio_types.h"
#include "My_mcu_sleep_init.h"

#define ALL_IN_DEEP_SLEEP_MODE 0
#define SOMETHING_IN_DEEP_SLEEP_MODE 1
#define EXT1 0

static unsigned int sleep_count = 0;
static unsigned int wakeup_time = 0;
static esp_sleep_wakeup_cause_t wakeup_cause;
unsigned char sleep_flag = true;

void My_wake_stub(void)
{
    // Get wakeup time.
    wakeup_time = esp_cpu_get_cycle_count() / esp_rom_get_cpu_ticks_per_us();
    wakeup_cause = esp_sleep_get_wakeup_cause();
    sleep_count++;
    if (sleep_count > 5)
    {
        sleep_flag = false;
#if !EXT1
        rtc_gpio_deinit(GPIO_NUM_5);
#endif
        return;
    }

    ESP_RTC_LOGI("wake stub: wakeup count is %d,\n wakeup cause is %d,\n wakeup cost %ld us",
                 sleep_count, wakeup_cause, wakeup_time);

    // 苏醒后call苏醒函数, 再度配置苏醒函数后sleep
    esp_wake_stub_sleep(&My_wake_stub);
}

void sleep_init(void)
{

    // esp_set_deep_sleep_wake_stub(&My_wake_stub);
#if ALL_IN_DEEP_SLEEP_MODE
    esp_sleep_pd_config(ESP_PD_DOMAIN_MAX, ESP_PD_OPTION_AUTO);
#endif
#if SOMETHING_IN_DEEP_SLEEP_MODE
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
#endif

#if EXT1
    // ext1係多个RTC GPIO唤醒逻辑
    esp_sleep_enable_ext1_wakeup(GPIO_NUM_5,
                                 ESP_EXT1_WAKEUP_ANY_HIGH);
#elif !EXT1
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_5, true);
#endif
#if !SOC_PM_SUPPORT_RTC_PERIPH_PD // 不支持RTC控制器
    gpio_pullup_dis(GPIO_NUM_5);
    gpio_pulldown_en(GPIO_NUM_5);
#else // 支持RTC控制器, 关闭RTC控制域下仅维持GPIO5的电源
    rtc_gpio_isolate(GPIO_NUM_5);
    rtc_gpio_pullup_dis(GPIO_NUM_5);
    rtc_gpio_pulldown_en(GPIO_NUM_5);
#endif
}
