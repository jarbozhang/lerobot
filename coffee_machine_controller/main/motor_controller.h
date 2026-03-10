/**
 * @file motor_controller.h
 * @brief 电机控制模块 - PWM驱动
 */

#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <stdint.h>
#include "driver/ledc.h"

// PWM 配置
#define MOTOR_PWM_FREQ_HZ       2000
#define MOTOR_PWM_DUTY_RES      LEDC_TIMER_8_BIT
#define MOTOR_PWM_MAX_DUTY      ((1U << MOTOR_PWM_DUTY_RES) - 1U)

// 电机 GPIO 配置
#define MOTOR1_GPIO             (GPIO_NUM_35)
#define MOTOR1_CHANNEL          LEDC_CHANNEL_0
#define MOTOR2_GPIO             (GPIO_NUM_36)
#define MOTOR2_CHANNEL          LEDC_CHANNEL_1

/**
 * @brief 初始化电机控制模块（PWM）
 */
void motor_controller_init(void);

/**
 * @brief 运行指定电机一段时间后自动停止
 * 
 * @param name 电机名称（用于日志）
 * @param channel LEDC通道
 * @param percent 占空比百分比 (0-100)
 * @param run_ms 运行时间（毫秒）
 */
void motor_run_for_ms(const char *name, ledc_channel_t channel, int percent, uint32_t run_ms);

/**
 * @brief 设置电机占空比
 * 
 * @param channel LEDC通道
 * @param percent 占空比百分比 (0-100)
 */
void motor_set_percent(ledc_channel_t channel, int percent);

/**
 * @brief 停止电机
 * 
 * @param channel LEDC通道
 */
void motor_stop(ledc_channel_t channel);

#endif /* MOTOR_CONTROLLER_H */
