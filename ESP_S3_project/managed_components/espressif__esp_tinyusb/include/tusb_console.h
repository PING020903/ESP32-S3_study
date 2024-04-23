/*
 * SPDX-FileCopyrightText: 2020-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/**
 * @brief Redirect output to the USB serial
 * @param cdc_intf - interface number of TinyUSB's CDC
 *
 * @return esp_err_t - ESP_OK, ESP_FAIL or an error code
 */
esp_err_t esp_tusb_init_console(int cdc_intf);

/**
 * @brief Switch log to the default output
 * @param cdc_intf - interface number of TinyUSB's CDC
 *
 * @return esp_err_t
 */
esp_err_t esp_tusb_deinit_console(int cdc_intf);

/**
 * @brief 更改串口输出模式
 * @param mode - 串口输出模式, 0: USB输出模式, 1: 串口输出模式
 *
 * @return esp_err_t
 */
esp_err_t My_tusb_streams_change(int mode);

#ifdef __cplusplus
}
#endif
