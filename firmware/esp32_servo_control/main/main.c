#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "servo.h"
#include "espnow_sender.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Servo + ESP-NOW sender ===");

    // NVS 初始化（WiFi 依赖）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 舵机初始化
    servo_t servo_a, servo_b;
    ESP_ERROR_CHECK(servo_init(&servo_a, &servo_b));

    // ESP-NOW 初始化 + 开始配对
    espnow_sender_wifi_init();
    ESP_ERROR_CHECK(espnow_sender_init());

    // 等待配对完成
    ESP_LOGI(TAG, "Waiting for ESP-NOW pairing...");
    while (!espnow_is_paired()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "Paired! Starting main loop.");

    // 主循环：舵机转3s → 等10s → 发命令 → 等10s
    while (1) {
        ESP_LOGI(TAG, "Servo A: 100%% for 3s");
        servo_set_speed(&servo_a, 100);
        vTaskDelay(pdMS_TO_TICKS(3000));

        ESP_LOGI(TAG, "Servo stop");
        servo_set_speed(&servo_a, 0);
        vTaskDelay(pdMS_TO_TICKS(10000));

        ESP_LOGI(TAG, "Sending command: 美式");
        espnow_send_command("美式");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
