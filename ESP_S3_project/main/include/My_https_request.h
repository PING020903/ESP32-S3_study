#ifndef _MY_HTTPS_REQUEST_H_
#define _MY_HTTPS_REQUEST_H_


#define HTTP_TEST 1
#define GET_TIME_FROM_NVS 0
#define GET_TIME_FROM_SNTP 1

#define HTTPS_SSL_TLS_TASK 0    // https_get_task_2
#define HTTPS_REQUEST_TASK 1    // https_request_task
/* Constants that aren't configurable in menuconfig */

#define WEB_PORT "443"
#if HTTP_TEST
#define WEB_SERVER "www.howsmyssl.com"
#define WEB_URL "https://www.howsmyssl.com/a/check"
#else
#define WEB_SERVER "www.bilibili.com"
#define WEB_URL "https://www.bilibili.com/"
#endif
/****************************************************/
#define SERVER_URL_MAX_SZ 256

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

esp_err_t fetch_and_store_time_in_nvs(void *args);
esp_err_t update_time_from_nvs(void);
void https_request_task(void *pvparameters);
void https_request_init(void);
void only_https_request_init(void);

#endif