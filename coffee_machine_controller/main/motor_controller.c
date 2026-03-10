/**
 * @file motor_controller.c
 * @brief 电机控制模块实现
 */

#include "motor_controller.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "motor";

// 低电平有效的负载配置
#define DRIVE_LOW_ACTIVE    1
#define PWM_SPEED_MODE      LEDC_LOW_SPEED_MODE
#define PWM_TIMER           LEDC_TIMER_0

// 电机任务句柄
static TaskHandle_t s_motor1_task = NULL;
static TaskHandle_t s_motor2_task = NULL;

// 电机运行上下文
typedef struct {
    const char *name;
    ledc_channel_t channel;
    int percent;
    uint32_t run_ms;
    TaskHandle_t *slot;
} motor_run_ctx_t;

static inline uint32_t duty_from_percent(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return (uint32_t)(MOTOR_PWM_MAX_DUTY * percent / 100);
}

void motor_set_percent(ledc_channel_t channel, int percent)
{
    const uint32_t duty = duty_from_percent(percent);
    ESP_ERROR_CHECK(ledc_set_duty(PWM_SPEED_MODE, channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(PWM_SPEED_MODE, channel));
}

void motor_stop(ledc_channel_t channel)
{
    motor_set_percent(channel, 0);
}

void motor_controller_init(void)
{
    const ledc_timer_config_t timer_conf = {
        .speed_mode = PWM_SPEED_MODE,
        .timer_num = PWM_TIMER,
        .duty_resolution = MOTOR_PWM_DUTY_RES,
        .freq_hz = MOTOR_PWM_FREQ_HZ,
        .clk_cfg = LEDC_USE_APB_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t ch1 = {
        .gpio_num = MOTOR1_GPIO,
        .speed_mode = PWM_SPEED_MODE,
        .channel = MOTOR1_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ch1.flags.output_invert = DRIVE_LOW_ACTIVE;
    ESP_ERROR_CHECK(ledc_channel_config(&ch1));

    ledc_channel_config_t ch2 = {
        .gpio_num = MOTOR2_GPIO,
        .speed_mode = PWM_SPEED_MODE,
        .channel = MOTOR2_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ch2.flags.output_invert = DRIVE_LOW_ACTIVE;
    ESP_ERROR_CHECK(ledc_channel_config(&ch2));

    // 初始化为停止状态
    motor_stop(MOTOR1_CHANNEL);
    motor_stop(MOTOR2_CHANNEL);

    ESP_LOGI(TAG, "Motor controller init: freq=%dHz, res=8-bit, max_duty=%u, invert=%d",
             MOTOR_PWM_FREQ_HZ, (unsigned)MOTOR_PWM_MAX_DUTY, DRIVE_LOW_ACTIVE);
    ESP_LOGI(TAG, "MOTOR1_GPIO: %d, MOTOR2_GPIO: %d", MOTOR1_GPIO, MOTOR2_GPIO);
}

static void motor_run_task(void *arg)
{
    motor_run_ctx_t *ctx = (motor_run_ctx_t *)arg;
    ESP_LOGI(TAG, "%s start: duty=%u (%d%%) for %u ms",
             ctx->name, (unsigned)duty_from_percent(ctx->percent), ctx->percent, (unsigned)ctx->run_ms);

    motor_set_percent(ctx->channel, ctx->percent);
    vTaskDelay(pdMS_TO_TICKS(ctx->run_ms));
    motor_stop(ctx->channel);

    ESP_LOGI(TAG, "%s stop", ctx->name);
    if (ctx->slot) *(ctx->slot) = NULL;
    free(ctx);
    vTaskDelete(NULL);
}

void motor_run_for_ms(const char *name, ledc_channel_t channel, int percent, uint32_t run_ms)
{
    // 获取对应的任务槽
    TaskHandle_t *slot = NULL;
    if (channel == MOTOR1_CHANNEL) {
        slot = &s_motor1_task;
    } else if (channel == MOTOR2_CHANNEL) {
        slot = &s_motor2_task;
    }

    // 如果同一路电机已有 task 在跑，忽略本次命令
    if (slot && *slot) {
        ESP_LOGW(TAG, "%s is busy, ignore command", name);
        return;
    }

    motor_run_ctx_t *ctx = (motor_run_ctx_t *)calloc(1, sizeof(motor_run_ctx_t));
    if (!ctx) {
        ESP_LOGE(TAG, "%s: OOM, cannot start", name);
        return;
    }
    ctx->name = name;
    ctx->channel = channel;
    ctx->percent = percent;
    ctx->run_ms = run_ms;
    ctx->slot = slot;

    TaskHandle_t h = NULL;
    BaseType_t ok = xTaskCreate(motor_run_task, name, 2048, ctx, 10, &h);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "%s: xTaskCreate failed", name);
        free(ctx);
        return;
    }
    if (slot) *slot = h;
}
