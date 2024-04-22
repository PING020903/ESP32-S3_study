#ifndef _MY_WIFI_INIT_H_
#define _MY_WIFI_INIT_H_
#include <esp_now.h>
#include "esp_wifi.h"

#define MY_ESP_NOW 0
#define MY_STA_ESP_ETH 0
#define EXAMPLE_ESP_WIFI_SSID "Ping"
#define EXAMPLE_ESP_WIFI_PASS ""
#define EXAMPLE_ESP_WIFI_PASS2 "PINGping"
#define EXAMPLE_DEST_WIFI_MAC "D4:DA:21:32:85:B6"
#define EXAMPLE_ESP_MAXIMUM_RETRY (5)

/********select wifi mode define********/
#define CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK 1
#define CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT 0
#define CONFIG_ESP_WPA3_SAE_PWE_BOTH 0

#define CONFIG_ESP_WIFI_AUTH_OPEN 1
#define CONFIG_ESP_WIFI_AUTH_WEP 0
#define CONFIG_ESP_WIFI_AUTH_WPA_PSK 0
#define CONFIG_ESP_WIFI_AUTH_WPA2_PSK 0
#define CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK 0
#define CONFIG_ESP_WIFI_AUTH_WPA3_PSK 0
#define CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK 0
#define CONFIG_ESP_WIFI_AUTH_WAPI_PSK 0

#define CONFIG_ESPNOW_ENABLE_LONG_RANGE 0
/******************************************************************************/

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif
/******************************************************************************/
#if MY_ESP_NOW
#define ESPNOW_QUEUE_SIZE 6
#define IS_BROADCAST_ADDR(addr, broadcast_mac) (memcmp(addr, broadcast_mac, ESP_NOW_ETH_ALEN) == 0)
/******************************************************************************/
typedef enum
{
    ESPNOW_SEND_CB,
    ESPNOW_RECV_CB,
} espnow_event_id_t;

typedef struct
{
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} espnow_event_send_cb_t;

typedef struct
{
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} espnow_event_recv_cb_t;

typedef union
{
    espnow_event_send_cb_t send_cb;
    espnow_event_recv_cb_t recv_cb;
} espnow_event_info_t;

/* When ESPNOW sending or receiving callback function is called, post event to ESPNOW task. */
typedef struct
{
    espnow_event_id_t id;
    espnow_event_info_t info;
} espnow_event_t;

enum
{
    ESPNOW_DATA_BROADCAST,
    ESPNOW_DATA_UNICAST,
    ESPNOW_DATA_MAX,
};

/* User defined field of ESPNOW data in this example. */
typedef struct
{
    uint8_t type;       // Broadcast or unicast ESPNOW data.
    uint8_t state;      // Indicate that if has received broadcast ESPNOW data or not.
    uint16_t seq_num;   // Sequence number of ESPNOW data.
    uint16_t crc;       // CRC16 value of ESPNOW data.
    uint32_t magic;     // Magic number which is used to determine which device to send unicast ESPNOW data.
    uint8_t payload[0]; // Real payload of ESPNOW data.
} __attribute__((packed)) espnow_data_t;

/* Parameters of sending ESPNOW data. */
typedef struct
{
    bool unicast;                       // Send unicast ESPNOW data.
    bool broadcast;                     // Send broadcast ESPNOW data.
    uint8_t state;                      // Indicate that if has received broadcast ESPNOW data or not.
    uint32_t magic;                     // Magic number which is used to determine which device to send unicast ESPNOW data.
    uint16_t count;                     // Total count of unicast ESPNOW data to be sent.
    uint16_t delay;                     // Delay between sending two ESPNOW data, unit: ms.
    int len;                            // Length of ESPNOW data to be sent, unit: byte.
    uint8_t *buffer;                    // Buffer pointing to ESPNOW data.
    uint8_t dest_mac[ESP_NOW_ETH_ALEN]; // MAC address of destination device.
} espnow_send_param_t;
#endif

#if MY_STA_ESP_ETH
#define FLOW_CONTROL_QUEUE_TIMEOU_MS (100)
#define FLOW_CONTROL_QUEUE_LENGTH (40)
#define FLOW_CONTROL_WIFI_SEND_TIMEOUT_MS (100)

/**
 * Set this to 1 to runtime update HW addresses in DHCP messages
 * (this is needed if the client uses 61 option and the DHCP server applies strict rules on assigning addresses)
 */
#define MODIFY_DHCP_MSGS        0

#define IP_V4 0x40
#define IP_PROTO_UDP 0x11
#define DHCP_PORT_IN 0x43
#define DHCP_PORT_OUT 0x44
#define DHCP_MACIG_COOKIE_OFFSET (8 + 236)
#define DHCP_HW_ADDRESS_OFFSET (36)
#define MIN_DHCP_PACKET_SIZE (285)
#define IP_HEADER_SIZE (20)
#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_COOKIE_WITH_PKT_TYPE(type) {0x63, 0x82, 0x53, 0x63, 0x35, 1, type};

typedef struct
{
    void *packet;
    uint16_t length;
} flow_control_msg_t;

typedef enum
{
    FROM_WIRED,
    TO_WIRED
} mac_spoof_direction_t;

#endif
/******************************************************************************/


void wifi_init_sta(void);
#if MY_ESP_NOW
int espnow_data_parse(uint8_t *data,
                      uint16_t data_len,
                      uint8_t *state,
                      uint16_t *seq,
                      int *magic);
void espnow_data_prepare(espnow_send_param_t *send_param);
esp_err_t ESP_NOW_init(void);
void espnow_deinit(espnow_send_param_t *send_parm);
void espnow_task(void *pvParmeter);
#endif
#endif