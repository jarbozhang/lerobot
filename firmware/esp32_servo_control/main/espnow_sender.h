#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief 初始化 WiFi（ESP-NOW 依赖）
 */
void espnow_sender_wifi_init(void);

/**
 * @brief 初始化 ESP-NOW 发送端，启动配对
 */
esp_err_t espnow_sender_init(void);

/**
 * @brief 发送字符串命令到已配对的对端
 */
esp_err_t espnow_send_command(const char *cmd);

/**
 * @brief 检查是否已完成配对
 */
bool espnow_is_paired(void);
