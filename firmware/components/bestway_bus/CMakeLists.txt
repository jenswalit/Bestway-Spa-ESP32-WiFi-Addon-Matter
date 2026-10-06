idf_component_register(
    SRCS "bestway_bus.cpp"
    INCLUDE_DIRS "include"
    REQUIRES spa_core log nvs_flash esp_driver_uart esp_driver_gpio esp_driver_ledc
)
