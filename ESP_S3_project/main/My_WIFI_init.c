/* WiFi initialization ＆ net initialization */
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"  // wifi相关头文件
#include "esp_event.h" // wifi事件头文件
#include "esp_log.h"
#include "esp_check.h"
#include "esp_now.h" // espnow相关头文件
#include "esp_crc.h"
#include "esp_mac.h"
#include "esp_tls.h" // https相关头文件
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h" // nvs相关头文件

#include <wifi_provisioning/manager.h>

#include <protocomm.h>
#include <protocomm_console.h>

// lwip相关头文件, 即TCP/IP协议栈
#include "lwip/err.h"
#include "lwip/sys.h"

#include "My_WIFI_init.h"
#include "My_https_request.h"

static const char *TAG = "My_wifi_STA";
static EventGroupHandle_t wifi_event_group; // wifi事件组
static int s_retry_num = 0;                 // 重试次数
static wifi_ap_record_t ap_info;            // AP信息
esp_netif_t *esp_netif_sta;                 // esp_netif 句柄
uint8_t https_request_flag = false;         // 允许https请求标志

#if MY_STA_ESP_ETH
static bool s_ethernet_is_connected = false;

static EventGroupHandle_t s_event_flags;
static bool s_wifi_is_connected = false;
static uint8_t s_eth_mac[6];

const int CONNECTED_BIT = BIT0;
const int DISCONNECTED_BIT = BIT1;
const int RECONFIGURE_BIT = BIT2;
const int PROV_SUCCESS_BIT = BIT3;
const int PROV_FAIL_BIT = BIT4;
#endif

#if MY_ESP_NOW
static QueueHandle_t s_espnow_queue; // espnow队列句柄
static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint16_t s_espnow_seq[ESPNOW_DATA_MAX] = {0, 0};
static uint8_t lmk[16] = {0};
#endif

/*
事件组允许每个事件有多个位，但我们只关心两个事件：
 * - 我们通过 IP 连接到 AP
 * - 重试次数达到最大次数后，我们无法连接
*/
static void WIFI_STA_event_handler(void *arg,
                                   esp_event_base_t event_base,
                                   int32_t event_id,
                                   void *event_data)
{

    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
        {
#if 1
            uint8_t *mac = (unsigned char *)EXAMPLE_DEST_WIFI_MAC;
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
            wifi_scan_config_t scan_config = {
                .ssid = NULL,
                .bssid = mac,
                .show_hidden = true,
            };
            ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true)); // 扫描AP
#endif
            ESP_ERROR_CHECK(esp_wifi_connect());
        }
        break;
        case WIFI_EVENT_STA_CONNECTED:
        {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED ! ! !");
        }
        break;
        case WIFI_EVENT_STA_DISCONNECTED:
        {
            ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED");
            ESP_LOGI(TAG, "retry to connect to the AP");
            if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) // 尝试重连
            {
                esp_err_t ret = esp_wifi_connect();
                switch (ret)
                {
                case ESP_OK:
                    ESP_LOGI(TAG, "retry connect succeed !");
                    break;

                case ESP_ERR_WIFI_NOT_INIT:
                    ESP_LOGW(TAG, "WIFI NOT INIT...");
                    break;

                case ESP_ERR_WIFI_NOT_STARTED:
                    ESP_LOGW(TAG, "WIFI NOT STARTED...");
                    wifi_init_sta();
                    break;

                case ESP_ERR_WIFI_CONN:
                    ESP_LOGW(TAG, "WIFI CONN ERROR...");
                    break;

                case ESP_ERR_WIFI_SSID:
                    ESP_LOGW(TAG, "WIFI SSID ERROR...");
                    break;

                default:
                    break;
                }
                s_retry_num++;
            }
            else
            {
                xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT); // 连接失败
            }
            ESP_LOGI(TAG, "connect to the AP fail");
        }
        break;
        case WIFI_EVENT_SCAN_DONE:
        {
            ESP_LOGI(TAG, "wifi scan done");
        }
        break;
        case WIFI_EVENT_STA_BEACON_TIMEOUT:
        {
            ESP_LOGE(TAG, "wifi timeout");
        }
        break;
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
        {
            ESP_LOGI(TAG, "wifi auth mode change");
        }
        break;

        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT); // 连接成功
            s_retry_num = 0;
        }
        break;
        case IP_EVENT_STA_LOST_IP:
        {
            ESP_LOGI(TAG, "wifi event: sta lost IP...");
        }
        break;

        default:
            break;
        }
    }
    return;
}

#if MY_ESP_NOW
/* espnow 接收回调函数 */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info,
                           const uint8_t *data, int len)
{
    espnow_event_t evt;
    espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    uint8_t *mac_addr = recv_info->src_addr;
    uint8_t *des_addr = recv_info->des_addr;

    if (mac_addr == NULL || des_addr == NULL || len <= 0)
    {
        ESP_LOGE(TAG, "Recieve callback arg error");
        return;
    }

    if (IS_BROADCAST_ADDR(des_addr, broadcast_mac))
    {
        /* If added a peer with encryption before, the receive packets may be
         * encrypted as peer-to-peer message or unencrypted over the broadcast channel.
         * Users can check the destination address to distinguish it.
         */
        ESP_LOGD(TAG, "Receive broadcast ESPNOW data");
    }
    else
    {
        ESP_LOGD(TAG, "Receive unicast ESPNOW data");
    }

    evt.id = ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL)
    {
        ESP_LOGE(TAG, "malloc receive queue fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(s_espnow_queue, &evt, portMAX_DELAY) != pdPASS)
    {
        ESP_LOGE(TAG, "send receive queue fail");
        free(recv_cb->data);
    }

    return;
}

/* espnow 发送回调函数 */
static void espnow_send_cb(const uint8_t *mac_addr,
                           esp_now_send_status_t status)
{
    espnow_event_t evt;
    espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL)
    {
        ESP_LOGE(TAG, "Send callback arg error");
        return;
    }

    evt.id = ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(s_espnow_queue, &evt, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "send send queue fail");
    }
}

/* Parse received ESPNOW data. */
int espnow_data_parse(uint8_t *data,
                      uint16_t data_len,
                      uint8_t *state,
                      uint16_t *seq,
                      int *magic)
{
    espnow_data_t *buf = (espnow_data_t *)data;
    uint16_t crc, crc_cal = 0;

    if (data_len < sizeof(espnow_data_t))
    {
        ESP_LOGE(TAG, "receive ESPNOW data too short, len: %d", data_len);
        return -1;
    }

    *state = buf->state;
    *seq = buf->seq_num;
    *magic = buf->magic;
    crc = buf->crc;
    buf->crc = 0;
    crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);
    if (crc_cal == crc)
    {
        return buf->type;
    }
    return -1;
}

/* Prepare ESPNOW data to be sent. 数据头准备*/
void espnow_data_prepare(espnow_send_param_t *send_param)
{
    espnow_data_t *buf = (espnow_data_t *)send_param->buffer;
    assert(send_param->len >= sizeof(espnow_data_t));

    buf->type = IS_BROADCAST_ADDR(send_param->dest_mac, broadcast_mac) ? ESPNOW_DATA_BROADCAST : ESPNOW_DATA_UNICAST;
    buf->state = send_param->state;
    buf->seq_num = s_espnow_seq[buf->type]++;
    buf->crc = 0;
    buf->magic = send_param->magic;

    /* Fill all remaining bytes after the data with random values */
    esp_fill_random(buf->payload, send_param->len - sizeof(espnow_data_t));
    buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);

    return;
}
#endif

#if MY_STA_ESP_ETH
static esp_err_t wired_recv(esp_eth_handler_t)

    void mac_spoof(mac_spoof_direction_t direction,
                   uint8_t *buffer,
                   uint16_t len,
                   uint8_t own_mac[6])
{
    if (!s_ethernet_is_connected)
        return;

    static uint8_t eth_nic_mac[6] = {};
    static bool eth_nic_mac_found = false;
#if !ETH_BRIDGE_PROMISCUOUS
    static uint8_t ap_mac[6] = {};
    static bool ap_mac_found = false;
#endif
    uint8_t *dest_mac = buffer;
    uint8_t *src_mac = buffer + 6;
    uint8_t *eth_type = buffer + 12;

    if (eth_type[0] == 0x08) // support only IPv4
    {
        // try to find NIC HW address (look for DHCP discovery packet)
        if ((!eth_nic_mac_found || (MODIFY_DHCP_MSGS)) &&
            direction == FROM_WIRED &&
            eth_type[1] == 0x00)
        {
            uint8_t *ip_header = eth_type + 2;
            if (len > MIN_DHCP_PACKET_SIZE &&
                (ip_header[0] & 0xf0) == IP_V4 &&
                ip_header[9] == IP_PROTO_UDP)
            {
                uint8_t *udp_header = ip_header + IP_HEADER_SIZE;
                const uint8_t dhcp_ports[] = {0, DHCP_PORT_OUT, 0, DHCP_PORT_IN};
                if (memcmp(udp_header, dhcp_ports, sizeof(dhcp_ports)) == 0)
                {
                    uint8_t *dhcp_magic = udp_header + DHCP_MACIG_COOKIE_OFFSET;
                    const uint8_t dhcp_type[] = DHCP_COOKIE_WITH_PKT_TYPE(DHCP_DISCOVER);
                    if (!eth_nic_mac_found &&
                        memcmp(dhcp_magic, dhcp_type, sizeof(dhcp_type)) == 0)
                    {
                        eth_nic_mac_found = true;
                        memccpy(eth_nic_mac, src_mac, 6);
                    }

                } // DHCP
            }     // UDP/IP
        }
#if !ETH_BRIDGE_PROMISCOUS || MODIFY_DHCP_MSGS
        else if ((!ap_mac_found || (MODIFY_DHCP_MSGS)) &&
                 direction == TO_WIRED &&
                 eth_type[1] == 0x00)
        {
            uint8_t *ip_header = eth_type + 2;
            if (len > MIN_DHCP_PACKET_SIZE &&
                (ip_header[0] & 0xf0) == IP_V4 &&
                ip_header[9] == IP_PROTO_UDP)
            {
                uint8_t *udp_header = ip_header + IP_HEADER_SIZE;
                const uint8_t dhcp_ports[] = {0, DHCP_PORT_OUT, 0, DHCP_PORT_IN};
                if (memcmp(udp_header, dhcp_ports, sizeof(dhcp_ports)) == 0)
                {
                    uint8_t *dhcp_magic = udp_header + DHCP_MACIG_COOKIE_OFFSET;
                    const uint8_t dhcp_type[] = DHCP_COOKIE_WITH_PKT_TYPE(DHCP_OFFER);
                    if (!ap_mac_found &&
                        memcmp(dhcp_magic, dhcp_type, sizeof(dhcp_type)) == 0)
                    {
                        ap_mac_found = true;
                        memcpy(ap_mac, src_mac, 6);
                    }
                } // DHCP
            }     // UDP/IP
        }
#endif
        if (eth_type[1] == 0x06) // ARP
        {
            uint8_t *arp = eth_type + 2 + 8; // points to sender's HW address
            if (eth_nic_mac_found &&
                direction == FROM_WIRED &&
                memcmp(arp, eth_nic_mac, 6) == 0)
            {
                memccpy(arp, own_mac, 6);
#if !ETH_BRIDGE_PROMISCUOUS
            }
            else if (ap_mac_found &&
                     direction == TO_WIRED &&
                     memcmp(arp, ap_mac, 6) == 0)
            {
                memcpy(arp, s_eth_mac, 6);
#endif
            }
        }
// swap HW addresses in ETH frames
#if !ETH_BRIDGE_PROMISOUOUS
        if (ap_mac_found &&
            direction == FROM_WIRED &&
            memcmp(dest_mac, s_eth_mac, 6) == 0)
        {
            memcmp(dest_mac, ap_mac, 6);
        }
        if (ap_mac_found &&
            direction == TO_WIRED &&
            memcmp(src_mac, ap_mac, 6) == 0)
        {
            memcmp(src_mac, s_eth_mac, 6);
        }
#endif
        if (eth_nic_mac_found &&
            direction == FROM_WIRED &&
            memcmp(src_mac, eth_nic_mac, 6) == 0)
        {
            memccpy(src_mac, own_mac, 6);
        }
        if (eth_nic_mac_found &&
            direction == TO_WIRED &&
            memcmp(dest_mac, own_mac, 6) == 0)
        {
            memccpy(dest_mac, eth_nic_mac, 6);
        }
    } // IP4 section of eth-type (0x08) both ETH-IP4 and ETHARP
}

/* wifi -- wired packet path */
static esp_err_t wired_recv_callback(void *buffer,
                                     uint16_t len,
                                     void *ctx)
{
    if (s_wifi_is_connected)
    {
    }
}
#endif

/*
    初始化 wifi
    ps: 连接WiFi并非联网
*/
void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate(); // 创建事件组

    // Initialize NVS, be must componet ! ! !
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase()); // erase NVS partition(擦除NVS分区)
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());                     // 初始化网络层
    esp_err_t event_ret = esp_event_loop_create_default(); // 创建默认事件循环
event_loop_check:
    switch (event_ret)
    {
    case ESP_OK:
        break;

    case ESP_ERR_INVALID_STATE:
        ESP_LOGW(TAG, "Default event loop has already been created, will recreate event loop");
        ESP_ERROR_CHECK(esp_event_loop_delete_default());
        event_ret = esp_event_loop_create_default();
        goto event_loop_check;

    case ESP_ERR_NO_MEM:
        ESP_LOGE(TAG, "no memory create event loop! ! !\n deinit netif && deinit NVS");
        ESP_ERROR_CHECK(esp_netif_deinit());
        ESP_ERROR_CHECK(nvs_flash_deinit());
        return;

    case ESP_FAIL:
        ESP_LOGE(TAG, "unknown error create event loop! ! !\n deinit netif && deinit NVS");
        ESP_ERROR_CHECK(esp_netif_deinit());
        ESP_ERROR_CHECK(nvs_flash_deinit());
        return;

    default:
        ESP_LOGE(TAG, "unknown error create event loop! ! !\n deinit netif && deinit NVS");
        ESP_ERROR_CHECK(esp_netif_deinit());
        ESP_ERROR_CHECK(nvs_flash_deinit());
        return;
    }
    esp_netif_sta = esp_netif_create_default_wifi_sta(); // 创建 wifi STA 默认的网络接口
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // 初始化配置, 使用宏将WiFi硬件配置初始化
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));                // 初始化 wifi

    esp_event_handler_instance_t instance_any_id; // 事件处理句柄
    esp_event_handler_instance_t instance_got_ip;
    // 注册事件处理实例
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &WIFI_STA_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &WIFI_STA_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /*
如果密码与 WPA2 标准匹配，则 Authmode 阈值将重置为 WPA2 默认值 （pasword len => 8）。
* 如果要将设备连接到已弃用的 WEP/WPA 网络，请设置阈值
* 设置为 WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK，并将长度和格式匹配的密码设置为
* WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK标准。
*/
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));               // 设置模式为 station
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config)); // 设置配置
    ESP_ERROR_CHECK(esp_wifi_start());                               // 启动 wifi
#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    /*
                           bitmap(bin)
                            ____
        WIFI_PROTOCOL_11B   0001
        WIFI_PROTOCOL_11G   0010
        WIFI_PROTOCOL_11N   0100
        WIFI_PROTOCOL_LR    1000
    */
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
#endif

    /*
    等待，直到建立连接 (WIFI_CONNECTED_BIT) 或连接失败达到最大值
     * 重试次数 (WIFI_FAIL_BIT)。这些位由 event_handler() 设置
    */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    uint8_t protocol_bitmap_temp;
    if (bits & WIFI_CONNECTED_BIT)
    {
        /* 当支持802.11n时候, 才支持40MHz频段带宽 */
        ESP_ERROR_CHECK(esp_wifi_get_protocol(WIFI_IF_STA, &protocol_bitmap_temp));

        ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
#if 0
        esp_wifi_get_bandwidth();
        esp_wifi_get_mac();
#endif

        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s, bitmap:%x",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, protocol_bitmap_temp);
        https_request_flag = true;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT ! ! !");
    }

    return;
}

/* wifi 停止函数, 停止后会释放wifi资源, 需重新初始化 */
void My_wifi_stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP WIFI not initialized...");
        return;
    }
    else
        ESP_LOGI(TAG, "ESP WIFI stopped...");
    return;
}

#if MY_ESP_NOW
void espnow_task(void *pvParmeter)
{
    espnow_event_t evt;
    uint8_t recv_state = 0;
    uint16_t recv_seq = 0;
    int recv_magic = 0;
    bool is_broadcast = false;
    int ret;
    int send_ret;

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "start sending broadcast data");

    /* Start sending broadcast ESPNOW data. */
    espnow_send_param_t *send_param = (espnow_send_param_t *)pvParmeter;
    send_ret = esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len);
    if (send_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_now_send fail, send_ret: %x", send_ret);
        /* ret = ESP_ERR_ESPNOW_NOT_FOUND */
        espnow_deinit(send_param);
        vTaskDelete(NULL);
        return;
    }

    while (xQueueReceive(s_espnow_queue, &evt, portMAX_DELAY) == pdTRUE)
    {
        switch (evt.id)
        {
        case ESPNOW_SEND_CB:
        {
            espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
            is_broadcast = IS_BROADCAST_ADDR(send_cb->mac_addr, broadcast_mac);

            ESP_LOGD(TAG, "send data to " MACSTR ", status: %d",
                     MAC2STR(send_cb->mac_addr), send_cb->status);

            if (is_broadcast && (send_param->broadcast == false))
                break;

            if (!is_broadcast)
            {
                send_param->count--;
                if (send_param->count == 0)
                {
                    ESP_LOGI(TAG, "send done...");
                    espnow_deinit(send_param);
                    vTaskDelete(NULL);
                }
            }

            /* Delay a while before sending the next data. */
            if (send_param->delay > 0)
                vTaskDelay(send_param->delay / portTICK_PERIOD_MS);

            ESP_LOGI(TAG, "send data to " MACSTR "", MAC2STR(send_cb->mac_addr)); // <<----------- 一直打印这里, mac_addr: ff:ff:ff:ff:ff:ff

            memcpy(send_param->dest_mac, send_cb->mac_addr, ESP_NOW_ETH_ALEN);
            espnow_data_prepare(send_param);

            /* Send the next data after the previous data is sent. */
            if (esp_now_send(send_param->dest_mac,
                             send_param->buffer,
                             send_param->len) != ESP_OK)
            {
                ESP_LOGE(TAG, "send error ! ! !");
                espnow_deinit(send_param);
                vTaskDelete(NULL);
            }
        }
        break;
        case ESPNOW_RECV_CB:
        {
            espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
            ret = espnow_data_parse(recv_cb->data,
                                    recv_cb->data_len,
                                    &recv_state,
                                    &recv_seq,
                                    &recv_magic);
            free(recv_cb->data);
            if (ret == ESPNOW_DATA_BROADCAST)
            {
                ESP_LOGI(TAG, "recvive %dth broadcast data from: " MACSTR ", len: %d",
                         recv_seq,
                         MAC2STR(recv_cb->mac_addr),
                         recv_cb->data_len);

                /* If MAC address does not exist in peer list, add it to peer list. */
                if (esp_now_is_peer_exist(recv_cb->mac_addr) == false)
                {
                    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
                    if (peer == NULL)
                    {
                        ESP_LOGE(TAG, "malloc peer information fail");
                        espnow_deinit(send_param);
                        vTaskDelete(NULL);
                    }

                    memset(peer, 0, sizeof(esp_now_peer_info_t));
                    peer->channel = ap_info.primary;
                    peer->ifidx = WIFI_IF_STA;
                    peer->encrypt = false;
                    /* lmk, 本地主密钥 (LMKs, 每个配对设备拥有一个 LMK) */
                    if (peer->encrypt) // data是否加密
                        memcpy(peer->lmk, lmk, ESP_NOW_KEY_LEN);
                    else
                        memcpy(peer->lmk, lmk, ESP_NOW_KEY_LEN);
                    memcpy(peer->peer_addr, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                    ESP_ERROR_CHECK(esp_now_add_peer(peer));
                    free(peer);
                }

                /* Indicates that the device has received broadcast ESPNOW data. */
                if (send_param->state == 0)
                    send_param->state = 1;

                /* If receive broadcast ESPNOW data which indicates that the other device has received
                 * broadcast ESPNOW data and the local magic number is bigger than that in the received
                 * broadcast ESPNOW data, stop sending broadcast ESPNOW data and start sending unicast
                 * ESPNOW data.
                 */
                if (recv_state == 1)
                {
                    /* The device which has the bigger magic number sends ESPNOW data, the other one
                     * receives ESPNOW data.
                     */
                    if (send_param->unicast == false && send_param->magic >= recv_magic)
                    {
                        ESP_LOGI(TAG, "start sending unicast data");
                        ESP_LOGI(TAG, "send data to " MACSTR "", MAC2STR(recv_cb->mac_addr));

                        /* Start sending unicast ESPNOW data. */
                        memcpy(send_param->dest_mac, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                        espnow_data_prepare(send_param);
                        if (esp_now_send(send_param->dest_mac,
                                         send_param->buffer,
                                         send_param->len) != ESP_OK)
                        {
                            ESP_LOGE(TAG, "send error");
                            espnow_deinit(send_param);
                            vTaskDelete(NULL);
                        }
                        else
                        {
                            send_param->broadcast = false;
                            send_param->unicast = true;
                        }
                    }
                }
            }
            else if (ret == ESPNOW_DATA_UNICAST)
            {
                ESP_LOGI(TAG, "receive %dth unicast data from: " MACSTR ", len: %d",
                         recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);
                send_param->broadcast = false;
            }
            else
            {
                ESP_LOGI(TAG, "receive error data from " MACSTR "", MAC2STR(recv_cb->mac_addr));
            }
        }
        break;

        default:
            ESP_LOGE(TAG, "callback type error: %d", evt.id);
            break;
        }
    }
}

/*
    初始化 communication protocol(ESP-NOW)
*/
esp_err_t ESP_NOW_init(void)
{
    espnow_send_param_t *send_param;

    s_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    if (s_espnow_queue == NULL)
    {
        ESP_LOGE(TAG, "Create mutex fail");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
#if CONFIG_ESPNOW_POWER_SAVE
    ESP_ERROR_CHECK(esp_now_set_wake_window(ESPNOW_WAKE_WINDOW));
    ESP_ERROR_CHECK(esp_wifi_connectionless_module_set_wake_interval(ESPNOW_WAKE_INTERVAL));
#endif
    /* Set primary master key.  */
    /* PMK, pairwise master key, 初始主密钥 */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)lmk)); // 数据包加密

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL)
    {
        ESP_LOGE(TAG, "Malloc peer infomation fail");
        vSemaphoreDelete(s_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = ap_info.primary; // 连接后获取所连接的channel填入该值
    peer->ifidx = WIFI_IF_STA;
    peer->encrypt = false; // data是否加密
    memcpy(peer->peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);

    /* Initialize sending parameters. */
    send_param = malloc(sizeof(espnow_send_param_t));
    if (send_param == NULL)
    {
        ESP_LOGE(TAG, "Malloc send parameter fail");
        vSemaphoreDelete(s_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(send_param, 0, sizeof(espnow_send_param_t));
    send_param->unicast = false;                  // 单播
    send_param->broadcast = true;                 // 广播
    send_param->state = 0;                        // 状态
    send_param->magic = esp_random();             // 用于确认接收端
    send_param->count = 6;                        // 要发送单播的总数
    send_param->delay = 100;                      // 每次发送之间的延时, unit: ms
    send_param->len = 88;                         // 单次数据长度, unit: byte
    send_param->buffer = malloc(send_param->len); // data缓冲区
    if (send_param->buffer == NULL)
    {
        ESP_LOGE(TAG, "malloc send buffer fail");
        free(send_param);
        vSemaphoreDelete(s_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memcpy(broadcast_mac, ap_info.bssid, ESP_NOW_ETH_ALEN);
    memcpy(send_param->dest_mac, broadcast_mac, ESP_NOW_ETH_ALEN);
    espnow_data_prepare(send_param);

    xTaskCreatePinnedToCore(espnow_task, "espnow_task", 2048, send_param, 4, NULL, 1);
    return ESP_OK;
}

void espnow_deinit(espnow_send_param_t *send_param)
{
    free(send_param->buffer);
    free(send_param);
    vSemaphoreDelete(s_espnow_queue);
    esp_now_deinit();
}
#endif