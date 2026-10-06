idf_component_register(
    SRCS "status_led.cpp"
    INCLUDE_DIRS "include"
    REQUIRES led_strip esp_timer log driver
)
