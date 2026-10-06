idf_component_register(
    SRCS "app_main.cpp" "matter_commissioning.cpp"
    INCLUDE_DIRS "."
    REQUIRES json bootloader_support esp_hw_support web_server esp_matter esp_matter_bridge nvs_flash spa_core bestway_bus scheduler mqtt_manager status_led
)
