#ifndef _MY_HTTPS_REQUEST_H_
#define _MY_HTTPS_REQUEST_H_
#include "esp_err.h"

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "www.howsmyssl.com"
#define WEB_PORT "443"
#define WEB_URL "https://www.howsmyssl.com/a/check"
//#define WEB_URL "https://cn.bing.com/"
/****************************************************/
#define SERVER_URL_MAX_SZ 256

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

esp_err_t fetch_and_store_time_in_nvs(void *args);
esp_err_t update_time_from_nvs(void);
void https_request_task(void *pvparameters);
void https_request_init(void);

#endif