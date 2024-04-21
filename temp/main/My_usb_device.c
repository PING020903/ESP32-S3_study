#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "My_usb_device.h"

static const char *TAG = "My_usb_device";

void My_usb_device_init(void) 
{
    tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_config_cdcacm_t cdc_cfg = {0};
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&cdc_cfg));

    ESP_LOGI(TAG, "USB initialization DONE");
    return;
}
