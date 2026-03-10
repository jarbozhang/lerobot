/**
 * @file uart_controller.h
 * @brief UART 通信控制模块
 */

#ifndef UART_CONTROLLER_H
#define UART_CONTROLLER_H

#include "esp_err.h"

// UART 配置（从 Kconfig 获取）
#define UART_TXD_PIN        (CONFIG_CONTROLLER_UART_TXD)
#define UART_RXD_PIN        (CONFIG_CONTROLLER_UART_RXD)
#define UART_PORT_NUM       (CONFIG_CONTROLLER_UART_PORT_NUM)
#define UART_BAUD_RATE      (CONFIG_CONTROLLER_UART_BAUD_RATE)
#define UART_BUF_SIZE       (1024)

/**
 * @brief 初始化 UART 控制器并启动接收任务
 * 
 * @return ESP_OK 成功
 */
esp_err_t uart_controller_init(void);

#endif /* UART_CONTROLLER_H */
