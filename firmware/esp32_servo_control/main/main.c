#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "servo.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Servo A 100%% constant ===");

    servo_t servo_a, servo_b;
    ESP_ERROR_CHECK(servo_init(&servo_a, &servo_b));
    ESP_LOGI(TAG, "Init OK, starting in 3s...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Servo A: 100%%");
    servo_set_speed(&servo_a, 100);

    while (1) {
        ESP_LOGI(TAG, "running");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
