#pragma once

#include "driver/mcpwm_prelude.h"

#define SERVO_A_GPIO        2
#define SERVO_B_GPIO        3
#define SERVO_PULSE_STOP_US 1500
#define SERVO_PULSE_MIN_US  1000
#define SERVO_PULSE_MAX_US  2000

typedef struct {
    mcpwm_cmpr_handle_t comparator;
    int gpio_num;
} servo_t;

esp_err_t servo_init(servo_t *servo_a, servo_t *servo_b);
esp_err_t servo_set_speed(servo_t *servo, int speed_percent);
