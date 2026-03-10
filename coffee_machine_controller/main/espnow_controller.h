/**
 * @file espnow_controller.h
 * @brief ESP-NOW 通信控制模块
 */

#ifndef ESPNOW_CONTROLLER_H
#define ESPNOW_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_now.h"
#include "esp_wifi.h"

// ESP-NOW 配置
#define ESPNOW_QUEUE_SIZE       6
#define ESPNOW_MAXDELAY         512

// WiFi 模式配置
#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_AP
#endif

// ESP-NOW 事件类型
typedef enum {
    ESPNOW_EVENT_SEND_CB,
    ESPNOW_EVENT_RECV_CB,
} espnow_event_id_t;

// 发送回调信息
typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} espnow_event_send_cb_t;

// 接收回调信息
typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} espnow_event_recv_cb_t;

// 事件信息联合体
typedef union {
    espnow_event_send_cb_t send_cb;
    espnow_event_recv_cb_t recv_cb;
} espnow_event_info_t;

// ESP-NOW 事件
typedef struct {
    espnow_event_id_t id;
    espnow_event_info_t info;
} espnow_event_t;

// 广播/单播数据类型
enum {
    ESPNOW_DATA_BROADCAST,
    ESPNOW_DATA_UNICAST,
    ESPNOW_DATA_MAX,
};

// ESP-NOW 数据包格式（用于匹配阶段）
typedef struct {
    uint8_t type;                         // 广播或单播
    uint8_t state;                        // 是否已收到广播
    uint16_t seq_num;                     // 序列号
    uint16_t crc;                         // CRC16
    uint32_t magic;                       // 魔数（用于确定发送方）
    uint8_t payload[0];                   // 数据载荷
} __attribute__((packed)) espnow_data_t;

// 发送参数
typedef struct {
    bool unicast;                         // 单播标志
    bool broadcast;                       // 广播标志
    uint8_t state;                        // 状态
    uint32_t magic;                       // 魔数
    uint16_t count;                       // 发送计数
    uint16_t delay;                       // 发送延迟
    int len;                              // 数据长度
    uint8_t *buffer;                      // 数据缓冲区
    uint8_t dest_mac[ESP_NOW_ETH_ALEN];   // 目标MAC
} espnow_send_param_t;

/**
 * @brief 初始化 WiFi（ESP-NOW 依赖）
 */
void espnow_wifi_init(void);

/**
 * @brief 初始化 ESP-NOW
 * 
 * @return ESP_OK 成功
 */
esp_err_t espnow_controller_init(void);

/**
 * @brief 反初始化 ESP-NOW
 */
void espnow_controller_deinit(void);

/**
 * @brief 发送字符串命令
 * 
 * @param cmd 命令字符串
 * @return ESP_OK 成功
 */
esp_err_t espnow_send_command(const char *cmd);

/**
 * @brief 检查是否已匹配
 * 
 * @return true 已匹配
 */
bool espnow_is_peer_matched(void);

/**
 * @brief 获取匹配的对端 MAC 地址
 * 
 * @param mac 输出缓冲区
 */
void espnow_get_peer_mac(uint8_t *mac);

#endif /* ESPNOW_CONTROLLER_H */
