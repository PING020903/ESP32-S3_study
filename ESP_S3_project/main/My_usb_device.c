#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "tusb_console.h"

#include "esp_log.h"
#include "esp_check.h"
#include "My_usb_device.h"

static const char *TAG = "My_usb_device";

#define STRINGIFY(s) STRINGIFY2(s)
#define STRINGIFY2(s) #s
#define VFS_TUSB_PATH_DEFAULT "/dev/tusb_cdc"


typedef struct
{
    FILE *in;
    FILE *out;
    FILE *err;
} console_handle_t;

static console_handle_t con;

/**
 * @brief Reopen standard streams using a new path
 *
 * @param f_in - pointer to a pointer holding a file for in or NULL to don't change stdin
 * @param f_out - pointer to a pointer holding a file for out or NULL to don't change stdout
 * @param f_err - pointer to a pointer holding a file for err or NULL to don't change stderr
 * @param path - mount point
 * @return esp_err_t ESP_FAIL or ESP_OK
 */
static esp_err_t redirect_std_streams_to(FILE **f_in, FILE **f_out, FILE **f_err, const char *path)
{
    if (f_in) {
        *f_in = freopen(path, "r", stdin);
        if (*f_in == NULL) {
            ESP_LOGE(TAG, "Failed to reopen in!");
            return ESP_FAIL;
        }
    }
    if (f_out) {
        *f_out = freopen(path, "w", stdout);
        if (*f_out == NULL) {
            ESP_LOGE(TAG, "Failed to reopen out!");
            return ESP_FAIL;
        }
    }
    if (f_err) {
        *f_err = freopen(path, "w", stderr);
        if (*f_err == NULL) {
            ESP_LOGE(TAG, "Failed to reopen err!");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

/**
 * @brief Restore output to default
 *
 * @param f_in - pointer to a pointer of an in file updated with `redirect_std_streams_to` or NULL to don't change stdin
 * @param f_out - pointer to a pointer of an out file updated with `redirect_std_streams_to` or NULL to don't change stdout
 * @param f_err - pointer to a pointer of an err file updated with `redirect_std_streams_to` or NULL to don't change stderr
 * @return esp_err_t ESP_FAIL or ESP_OK
 */
static esp_err_t restore_std_streams(FILE **f_in, FILE **f_out, FILE **f_err)
{
    const char *default_uart_dev = "/dev/uart/" STRINGIFY(CONFIG_ESP_CONSOLE_UART_NUM);
    if (f_in) {
        stdin = freopen(default_uart_dev, "r", *f_in);
        if (stdin == NULL) {
            ESP_LOGE(TAG, "Failed to reopen stdin!");
            return ESP_FAIL;
        }
    }
    if (f_out) {
        stdout = freopen(default_uart_dev, "w", *f_out);
        if (stdout == NULL) {
            ESP_LOGE(TAG, "Failed to reopen stdout!");
            return ESP_FAIL;
        }
    }
    if (f_err) {
        stderr = freopen(default_uart_dev, "w", *f_err);
        if (stderr == NULL) {
            ESP_LOGE(TAG, "Failed to reopen stderr!");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t My_tusb_streams_change(int mode)
{
    switch (mode)
    {
    case 0:
    {
        ESP_RETURN_ON_ERROR(redirect_std_streams_to(&con.in, &con.out, &con.err, VFS_TUSB_PATH_DEFAULT),
                            TAG,
                            "Failed to redirect STD streams");
    }
        return ESP_OK;
    case 1:
    {
        ESP_RETURN_ON_ERROR(restore_std_streams(&con.in, &con.out, &con.err),
                            TAG,
                            "Failed to restore STD streams");
    }
        return ESP_OK;

    default:
        return ESP_FAIL;
    }
}

void My_tusb_cdcacm_callback(int itf, cdcacm_event_t *event)
{
    ESP_LOGI(TAG, "USB event: %d", event->type);
    ESP_LOGI(TAG, "USB char: %d", event->rx_wanted_char_data.wanted_char);
    ESP_LOGI(TAG, "USB DTR: %d, RTS: %d", event->line_state_changed_data.dtr,
             event->line_state_changed_data.rts);
#if 0   // 一旦访问这个成员的信息, OS就会reset
    ESP_LOGI(TAG, "USB Baudrate: %ld, data_bits: %d, parity: %d, stop_bits: %d",
             event->line_coding_changed_data.p_line_coding->bit_rate,
             event->line_coding_changed_data.p_line_coding->data_bits,
             event->line_coding_changed_data.p_line_coding->parity,
             event->line_coding_changed_data.p_line_coding->stop_bits);
#endif
}

void My_usb_device_init(void) 
{
    /* Setting TinyUSB up */
    ESP_LOGI(TAG, "USB initialization");

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false, // In the most cases you need to use a `false` value
        .configuration_descriptor = NULL,
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_config_cdcacm_t acm_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .usb_dev = TINYUSB_USBDEV_0,
        .callback_rx = &My_tusb_cdcacm_callback,
    }; // the configuration uses default values
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
    ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(TINYUSB_CDC_ACM_0,
                                                     CDC_EVENT_RX,
                                                     &My_tusb_cdcacm_callback));

    ESP_LOGI(TAG, "USB initialization DONE");
    ESP_ERROR_CHECK(esp_tusb_init_console(TINYUSB_CDC_ACM_0)); // log to usb
    // 释放控制台USB串口控制权归还UART ( tusb_console多次注册释放后会引起内存泄漏 )
    // ESP_ERROR_CHECK(esp_tusb_deinit_console(TINYUSB_CDC_ACM_0));
    return;
}
