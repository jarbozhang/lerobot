idf_component_register(SRCS "main.c" "servo.c" "espnow_sender.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_mcpwm esp_wifi esp_netif nvs_flash esp_event)
