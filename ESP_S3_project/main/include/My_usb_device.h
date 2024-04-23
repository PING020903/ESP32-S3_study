#ifndef __MY_USB_DEVICE_H__
#define __MY_USB_DEVICE_H__

#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "tusb_console.h"

/**
 * @brief 更改串口输出模式
 * @param mode - 串口输出模式, 0: USB输出模式, 1: 串口输出模式
 *
 * @return esp_err_t
 */
esp_err_t My_tusb_streams_change(int mode);

void My_usb_device_init(void);
#endif