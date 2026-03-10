/**
 * @file controller_main.c
 * @brief 主控制器入口
 * 
 * 功能：
 * - ESP-NOW 无线通信（设备配对 + 命令收发）
 * - UART 串口通信
 * - PWM 电机控制
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "motor_controller.h"
#include "espnow_controller.h"
#include "uart_controller.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Controller Starting ===");

    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化电机控制
    motor_controller_init();

    // 初始化 WiFi 和 ESP-NOW
    espnow_wifi_init();
    espnow_controller_init();

    // 初始化 UART
    uart_controller_init();

    ESP_LOGI(TAG, "=== Controller Ready ===");
}
