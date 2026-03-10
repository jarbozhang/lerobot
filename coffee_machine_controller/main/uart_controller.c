/**
 * @file uart_controller.c
 * @brief UART 通信控制模块实现
 */

#include "uart_controller.h"
#include "command_handler.h"
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "uart";

static void uart_task(void *arg)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TXD_PIN, UART_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // 开启内部上拉，减少噪声
    gpio_pullup_en(UART_RXD_PIN);

    uint8_t *data = (uint8_t *)malloc(UART_BUF_SIZE);

    ESP_LOGI(TAG, "UART initialized: port=%d, baud=%d, RXD=%d, TXD=%d",
             UART_PORT_NUM, UART_BAUD_RATE, UART_RXD_PIN, UART_TXD_PIN);

    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM, data, (UART_BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = '\0';
            char *recv_str = (char *)data;
            str_rstrip(recv_str);

            ESP_LOGI(TAG, "Recv %d bytes: %s", len, recv_str);
            ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_DEBUG);

            // 处理命令
            command_process(recv_str);
        }
    }
}

esp_err_t uart_controller_init(void)
{
    BaseType_t ret = xTaskCreate(uart_task, "uart_task", CONFIG_CONTROLLER_TASK_STACK_SIZE, NULL, 10, NULL);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}
