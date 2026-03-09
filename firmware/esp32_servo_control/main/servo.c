#include "servo.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "servo";

static uint32_t speed_to_pulsewidth(int speed_percent)
{
    if (speed_percent < -100) speed_percent = -100;
    if (speed_percent > 100) speed_percent = 100;
    // -100 -> 1000us, 0 -> 1500us, +100 -> 2000us
    return SERVO_PULSE_STOP_US + (speed_percent * (SERVO_PULSE_MAX_US - SERVO_PULSE_STOP_US)) / 100;
}

esp_err_t servo_init(servo_t *servo_a, servo_t *servo_b)
{
    servo_a->gpio_num = SERVO_A_GPIO;
    servo_b->gpio_num = SERVO_B_GPIO;

    // Create timer
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // 1MHz
        .period_ticks = 20000,    // 20ms -> 50Hz
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_RETURN_ON_ERROR(mcpwm_new_timer(&timer_config, &timer), TAG, "create timer failed");

    servo_t *servos[2] = {servo_a, servo_b};
    int gpios[2] = {SERVO_A_GPIO, SERVO_B_GPIO};

    for (int i = 0; i < 2; i++) {
        // Create operator
        mcpwm_oper_handle_t oper = NULL;
        mcpwm_operator_config_t oper_config = {
            .group_id = 0,
        };
        ESP_RETURN_ON_ERROR(mcpwm_new_operator(&oper_config, &oper), TAG, "create operator %d failed", i);
        ESP_RETURN_ON_ERROR(mcpwm_operator_connect_timer(oper, timer), TAG, "connect timer %d failed", i);

        // Create comparator
        mcpwm_cmpr_handle_t cmpr = NULL;
        mcpwm_comparator_config_t cmpr_config = {
            .flags.update_cmp_on_tez = true,
        };
        ESP_RETURN_ON_ERROR(mcpwm_new_comparator(oper, &cmpr_config, &cmpr), TAG, "create comparator %d failed", i);
        servos[i]->comparator = cmpr;

        // Create generator
        mcpwm_gen_handle_t gen = NULL;
        mcpwm_generator_config_t gen_config = {
            .gen_gpio_num = gpios[i],
        };
        ESP_RETURN_ON_ERROR(mcpwm_new_generator(oper, &gen_config, &gen), TAG, "create generator %d failed", i);

        // Set actions: high on timer empty, low on compare match
        ESP_RETURN_ON_ERROR(mcpwm_generator_set_action_on_timer_event(gen,
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)),
            TAG, "set timer action %d failed", i);
        ESP_RETURN_ON_ERROR(mcpwm_generator_set_action_on_compare_event(gen,
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmpr, MCPWM_GEN_ACTION_LOW)),
            TAG, "set compare action %d failed", i);

        // Set initial stop position
        ESP_RETURN_ON_ERROR(mcpwm_comparator_set_compare_value(cmpr, SERVO_PULSE_STOP_US),
            TAG, "set initial compare %d failed", i);
    }

    // Enable and start timer
    ESP_RETURN_ON_ERROR(mcpwm_timer_enable(timer), TAG, "enable timer failed");
    ESP_RETURN_ON_ERROR(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP), TAG, "start timer failed");

    ESP_LOGI(TAG, "servos initialized on GPIO %d and GPIO %d", SERVO_A_GPIO, SERVO_B_GPIO);
    return ESP_OK;
}

esp_err_t servo_set_speed(servo_t *servo, int speed_percent)
{
    uint32_t pw = speed_to_pulsewidth(speed_percent);
    ESP_RETURN_ON_ERROR(mcpwm_comparator_set_compare_value(servo->comparator, pw),
        TAG, "set speed failed on GPIO %d", servo->gpio_num);
    return ESP_OK;
}
