#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_tls.h" // https相关头文件
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h" // nvs相关头文件
#include "esp_sntp.h"

#include "My_https_request.h"


static const char *TAG = "My_https_client";
static int err_temp[2] = {0};
extern uint8_t https_request_flag;

/**** 貌似係借用最终生成的二进制文件中的https证书 ****/
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[] asm("_binary_server_root_cert_pem_end");

extern const uint8_t local_server_cert_pem_start[] asm("_binary_local_server_cert_pem_start");
extern const uint8_t local_server_cert_pem_end[] asm("_binary_local_server_cert_pem_end");
/**************************************************************/

static const char HOWSMYSSL_REQUEST[] = "GET " WEB_URL " HTTP/1.1\r\n"
                                        "Host: " WEB_SERVER "\r\n"
                                        "User-Agent: esp-idf/1.0 esp32\r\n"
                                        "\r\n";

/* 打印SNTP服务器列表 */
static void print_servers(void)
{
    ESP_LOGI(TAG, "List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i)
    {
        if (esp_sntp_getservername(i))
        {
            ESP_LOGI(TAG, "server %d: %s", i, esp_sntp_getservername(i));
        }
        else
        {
            // we have either IPv4 or IPv6 address, let's print it
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = esp_sntp_getserver(i);
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
                ESP_LOGI(TAG, "server %d: %s", i, buff);
        }
    }
}

/* SNTP时间同步回调函数 */
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

/* 初始化SNTP */
static void init_sntp()
{
    ESP_LOGI(TAG, "initializing SNTP");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
                                                                      ESP_SNTP_SERVER_LIST("time.windows.com",
                                                                                           "pool.ntp.org"));
    config.sync_cb = time_sync_notification_cb; // Note: This is only needed if we want
    esp_netif_sntp_init(&config);
    print_servers();
    return;
}

/* 获取时间 */
static esp_err_t obtain_time(void)
{
    // wait for time to be set
    int retry = 0;
    const int retry_count = 20;

    /* maybe waitting 14s */
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(1000)) != ESP_OK &&
           ++retry < retry_count)
    {
        ESP_LOGI(TAG, "waitting for system time to be set...(%d/%d)",
                 retry, retry_count);
    }

    if (retry >= retry_count)
        return ESP_FAIL;

    ESP_LOGI(TAG, "SNTP sync OK");
    return ESP_OK;
}

/* 获取时间并写入NVS */
esp_err_t fetch_and_store_time_in_nvs(void *args)
{
    nvs_handle_t my_handle = 0;
    esp_err_t err;

    init_sntp();
    if (obtain_time() != ESP_OK)
    {
        err = ESP_FAIL;
        err_temp[1] = 4;
        goto exit_1;
    }

    time_t now;
    time(&now);

    // open
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        err_temp[1] = 1;
        goto exit_1;
    }

    // write
    err = nvs_set_i64(my_handle, "time_stamp", now);
    if (err != ESP_OK)
    {
        err_temp[1] = 2;
        goto exit_1;
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        err_temp[1] = 3;
        goto exit_1;
    }

exit_1:
    if (my_handle != 0)
        nvs_close(my_handle);

    esp_netif_sntp_deinit();

    if (err != ESP_OK)
    {
        /* 此处打印LOG不能带有参数, 一旦带参数就编译错误了 */
        ESP_LOGE(TAG, "error updating time in NVS");
        err_temp[0] = err;
    }
    else
        ESP_LOGI(TAG, "time updated in NVS");

    return err;
}

/* 从NVS更新时间 */
esp_err_t update_time_from_nvs(void)
{
    nvs_handle_t my_handle = 0; // NVS句柄
    esp_err_t err;

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "error opening NVS");
        goto exit_2;
    }

    int64_t time_stamp = 0;
#if GET_TIME_FROM_NVS
    err = nvs_get_i64(my_handle, "time_stamp", &time_stamp); // 获取时间戳
    if (err == ESP_ERR_NVS_NOT_FOUND)                        // 这个选项一般情况是不会进入的
    {
        ESP_LOGI(TAG, "time out found in NVS. syncing time from SNTP server.");
        if (fetch_and_store_time_in_nvs(NULL) == ESP_OK)
            err = ESP_FAIL;
        else
            err = ESP_OK;
    }
    else if (err == ESP_OK)
    {
        struct timeval get_nvs_time;
        get_nvs_time.tv_sec = time_stamp;
        settimeofday(&get_nvs_time, NULL);
        ESP_LOGI(TAG, "time: %lld", time_stamp);
    }
#endif
#if GET_TIME_FROM_SNTP
    if (fetch_and_store_time_in_nvs(NULL) == ESP_OK)
        err = ESP_OK;
    else
        err = ESP_FAIL;

    if (err)
    {
        ESP_LOGE(TAG, "error syncing time from SNTP server");
        goto exit_2;
    }
    else
    {
        err = nvs_get_i64(my_handle, "time_stamp", &time_stamp);
        if (err >= ESP_ERR_NVS_BASE)
        {
            ESP_LOGE(TAG, "error getting time from NVS");
            goto exit_2;
        }
        struct timeval get_nvs_time;
        get_nvs_time.tv_sec = time_stamp;
        settimeofday(&get_nvs_time, NULL);
        ESP_LOGI(TAG, "time: %lld", time_stamp);
    }
#endif

exit_2:
    if (my_handle != 0)
        nvs_close(my_handle);

    return err;
}

/* https 获得请求 */
static void https_get_request(esp_tls_cfg_t cfg,
                              const char *WEB_SERVER_URL,
                              const char *REQUEST)
{
    char buf[512];
    int ret, len;

    esp_tls_t *tls = esp_tls_init();
    if (!tls)
    {
        ESP_LOGE(TAG, "failed to allocate esp_tls handle! ! !");
        goto exit_3;
    }

    if (esp_tls_conn_http_new_sync(WEB_SERVER_URL, &cfg, tls) == 1)
    {
        ESP_LOGI(TAG, "---- connection established... ----");
    }
    else
    {
        ESP_LOGE(TAG, "connection failed...");
        int esp_tls_code = 0, esp_tls_flags = 0;
        esp_tls_error_handle_t tls_err = NULL;

        esp_tls_get_error_handle(tls, tls_err);
        /* Try to get TLS stack level error and certificate failure flags, if any */
        ret = esp_tls_get_and_clear_last_error(tls_err,
                                               &esp_tls_code,
                                               &esp_tls_flags);
        if (ret == ESP_OK)
            ESP_LOGE(TAG, "TSL error = -0x%x, TLS flags = -0x%x",
                     esp_tls_code, esp_tls_flags);
        goto cleanup;
    }

    size_t written_bytes = 0;
    do
    {
        ret = esp_tls_conn_write(tls,
                                 REQUEST + written_bytes,
                                 strlen(REQUEST) - written_bytes);
        if (ret >= 0)
        {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        }
        else if (ret != ESP_TLS_ERR_SSL_WANT_READ &&
                 ret != ESP_TLS_ERR_SSL_WANT_WRITE)
        {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned: [0x%02X](%s)",
                     ret, esp_err_to_name(ret));
            goto cleanup;
        }
    } while (written_bytes < strlen(REQUEST));

    ESP_LOGI(TAG, "reading HTTP response...");
    do
    {
        len = sizeof(buf) - 1;
        memset(buf, 0, sizeof(buf));
        ret = esp_tls_conn_read(tls, (char *)buf, len);

        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE ||
            ret == ESP_TLS_ERR_SSL_WANT_READ)
        {
            continue;
        }
        else if (ret < 0)
        {
            ESP_LOGE(TAG, "esp_tls_conn_read returned [-0x%02X](%s)",
                     -ret, esp_err_to_name(ret));
            break;
        }
        else if (ret == 0)
        {
            ESP_LOGI(TAG, "connection closed");
            break;
        }

        len = ret;
        ESP_LOGD(TAG, "%d bytes read", len);
        /* Print response directly to stdout as it is read */
        for (size_t i = 0; i < len; i++)
        {
            putchar(buf[i]);
        }
        putchar('\n'); // JSON output doesn't have a newline at end
    } while (1);

cleanup:
    int destroy_ret = esp_tls_conn_destroy(tls); // 释放esp_tls
exit_3:
    for (int countdown = 10; countdown >= 0; countdown--)
    {
        ESP_LOGI(TAG, "%d...", countdown);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    return;
}

/**/
static void https_get_request_using_cacert_buf(void)
{
    ESP_LOGI(TAG, "https_request using cacert_buf");
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *)server_root_cert_pem_start,
        .cacert_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
    };
    https_get_request(cfg, WEB_URL, HOWSMYSSL_REQUEST);
}

static void https_get_request_using_global_ca_store(void)
{
    esp_err_t esp_ret = ESP_FAIL;
    ESP_LOGI(TAG, "https_request using global ca_store");
    esp_ret = esp_tls_set_global_ca_store(server_root_cert_pem_start,
                                          server_root_cert_pem_end - server_root_cert_pem_start);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "error in setting the global ca store: [%02X] (%s), could not complete the https_request using global_ca_store",
                 esp_ret, esp_err_to_name(esp_ret));
        return;
    }

    esp_tls_cfg_t cfg = {
        .use_global_ca_store = true,
    };
    https_get_request(cfg, WEB_URL, HOWSMYSSL_REQUEST);
    esp_tls_free_global_ca_store();
}

static void https_get_request_using_specified_ciphersuites(void)
{
#if CONFIG_EXAMPLE_USING_ESP_TLS_MBEDTLS

    ESP_LOGI(TAG, "https_request using server supported ciphersuites");
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *)server_root_cert_pem_start,
        .cacert_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
        .ciphersuites_list = server_supported_ciphersuites,
    };

    https_get_request(cfg, WEB_URL, HOWSMYSSL_REQUEST);

    ESP_LOGI(TAG, "https_request using server unsupported ciphersuites");

    cfg.ciphersuites_list = server_unsupported_ciphersuites;

    https_get_request(cfg, WEB_URL, HOWSMYSSL_REQUEST);
#endif
}

void https_request_task(void *pvparameters)
{
    /* 等待https请求标志 */
    while (!https_request_flag)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "https_request_task is waitting for flag...");
    }

    ESP_LOGI(TAG, "start https_request example");
    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes",
             esp_get_minimum_free_heap_size());
    https_get_request_using_cacert_buf();
    https_get_request_using_global_ca_store();
    https_get_request_using_specified_ciphersuites();
    ESP_LOGI(TAG, "finish https_request example");
    ESP_LOGI(TAG, "NVS re_err:%x, NVS status:%d", err_temp[0], err_temp[1]);
    vTaskDelete(NULL); // 删除任务
}

/* https请求任务初始化 */
void https_request_init(void)
{
    if (esp_reset_reason() == ESP_RST_POWERON)
    {
        ESP_LOGI(TAG, "updating time from NVS");
        ESP_ERROR_CHECK(update_time_from_nvs());
        //update_time_from_nvs();
    }
    xTaskCreatePinnedToCore(https_request_task, "https_request_task", 8192, NULL, 5, NULL, 1);
}
