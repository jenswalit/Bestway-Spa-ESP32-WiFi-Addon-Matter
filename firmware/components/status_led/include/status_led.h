#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STATUS_LED_GPIO 8

typedef enum {
    STATUS_LED_STATE_STARTING = 0,
    STATUS_LED_STATE_OK,
    STATUS_LED_STATE_MQTT_OFFLINE,
    STATUS_LED_STATE_SPA_OFFLINE,
    STATUS_LED_STATE_WIFI_OFFLINE,
    STATUS_LED_STATE_RECOVERY_AP,
    STATUS_LED_STATE_OTA,
    STATUS_LED_STATE_ERROR,
    STATUS_LED_STATE_MATTER_BOOT,
} status_led_state_t;

esp_err_t status_led_init(void);

void status_led_update(bool wifi_connected,
                       bool mqtt_connected,
                       bool spa_connected,
                       bool spa_data_valid,
                       int spa_error_code,
                       bool matter_active,
                       bool recovery_ap_active);

void status_led_set_ota_active(bool active);

bool status_led_available(void);
status_led_state_t status_led_state(void);
const char *status_led_state_name(void);
const char *status_led_color_name(void);
bool status_led_is_blinking(void);
uint8_t status_led_gpio(void);

#ifdef __cplusplus
}
#endif
