idf_component_register(
    SRCS 
        "controller_main.c"
        "motor_controller.c"
        "command_handler.c"
        "espnow_controller.c"
        "uart_controller.c"
    REQUIRES 
        esp_driver_uart 
        esp_driver_gpio 
        esp_driver_ledc 
        esp_wifi 
        esp_netif 
        nvs_flash 
        esp_event
    INCLUDE_DIRS "."
)
