/**
 * @file command_handler.c
 * @brief 命令处理模块实现
 */

#include "command_handler.h"
#include "motor_controller.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "command";

void str_rstrip(char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0) {
        char c = s[n - 1];
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') {
            s[n - 1] = '\0';
            n--;
            continue;
        }
        break;
    }
}

void command_process(const char *cmd)
{
    if (cmd == NULL || strlen(cmd) == 0) {
        return;
    }

    ESP_LOGI(TAG, "Processing: %s", cmd);

    if (strcmp(cmd, "浓缩咖啡") == 0) {
        motor_run_for_ms("motor2", MOTOR2_CHANNEL, 50, 5000);
    } else if (strcmp(cmd, "冰美式") == 0) {
        motor_run_for_ms("motor2", MOTOR2_CHANNEL, 30, 4000);
    } else if (strcmp(cmd, "热美式") == 0) {
        motor_run_for_ms("motor2", MOTOR2_CHANNEL, 40, 4000);
    } else if (strcmp(cmd, "美式") == 0) {
        motor_run_for_ms("motor2", MOTOR2_CHANNEL, 50, 4000);
    } else if (strcmp(cmd, "拿铁") == 0) {
        motor_run_for_ms("motor2", MOTOR2_CHANNEL, 60, 3000);
    } else if (strcmp(cmd, "卡布奇诺") == 0) {
        motor_run_for_ms("motor2", MOTOR2_CHANNEL, 70, 2000);
    } else if (strcmp(cmd, "热巧克力") == 0) {
        motor_run_for_ms("motor2", MOTOR2_CHANNEL, 80, 1000);
    } else {
        ESP_LOGW(TAG, "Unknown command: %s", cmd);
        motor_run_for_ms("motor2", MOTOR2_CHANNEL, 100, 5000);
    }
}
