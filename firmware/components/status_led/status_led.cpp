#include "status_led.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <led_strip.h>
#include <led_strip_rmt.h>

static const char *TAG = "status_led";
static led_strip_handle_t s_strip = nullptr;
static bool s_available = false;
static bool s_ota_active = false;
static bool s_matter_pulse_started = false;
static int64_t s_matter_pulse_until_us = 0;
static int64_t s_last_render_us = 0;
static status_led_state_t s_state = STATUS_LED_STATE_STARTING;
static bool s_blinking = false;
static bool s_last_visible = false;
static uint8_t s_last_r = 0xff;
static uint8_t s_last_g = 0xff;
static uint8_t s_last_b = 0xff;

struct LedVisual {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool blinking;
    uint32_t blink_period_ms;
};

static LedVisual visual_for_state(status_led_state_t state)
{
    switch (state) {
        case STATUS_LED_STATE_OK:           return {0, 28, 0, false, 0};
        case STATUS_LED_STATE_MQTT_OFFLINE: return {0, 0, 32, false, 0};
        case STATUS_LED_STATE_SPA_OFFLINE:  return {32, 18, 0, false, 0};
        case STATUS_LED_STATE_WIFI_OFFLINE: return {0, 0, 32, true, 900};
        case STATUS_LED_STATE_RECOVERY_AP:  return {0, 0, 32, true, 1200};
        case STATUS_LED_STATE_OTA:          return {24, 24, 24, true, 300};
        case STATUS_LED_STATE_ERROR:        return {32, 0, 0, false, 0};
        case STATUS_LED_STATE_MATTER_BOOT:  return {22, 0, 32, false, 0};
        default:                            return {8, 8, 8, false, 0};
    }
}

static void render(status_led_state_t state, int64_t now_us)
{
    if (!s_available || !s_strip) return;

    const LedVisual v = visual_for_state(state);

    bool visible = true;
    if (v.blinking && v.blink_period_ms) {
        uint64_t phase = (now_us / 1000ULL) % v.blink_period_ms;
        visible = phase < (v.blink_period_ms / 2);
    }

    if (state == s_state &&
        visible == s_last_visible &&
        (!visible || (v.r == s_last_r && v.g == s_last_g && v.b == s_last_b))) {
        return;
    }

    esp_err_t err;

    if (visible) {
        err = led_strip_set_pixel(s_strip, 0, v.r, v.g, v.b);
        if (err == ESP_OK) {
            err = led_strip_refresh(s_strip);
        }
    } else {
        err = led_strip_clear(s_strip);
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RGB status-led fout: %s", esp_err_to_name(err));
        return;
    }

    s_state = state;
    s_blinking = v.blinking;
    s_last_visible = visible;
    s_last_r = v.r;
    s_last_g = v.g;
    s_last_b = v.b;
}

esp_err_t status_led_init(void)
{
    if (s_available) return ESP_OK;

    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = STATUS_LED_GPIO;
    strip_config.max_leds = 1;
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.flags.invert_out = false;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_config.resolution_hz = 10 * 1000 * 1000;
    rmt_config.mem_block_symbols = 0;
    rmt_config.flags.with_dma = false;

    esp_err_t err = led_strip_new_rmt_device(
        &strip_config,
        &rmt_config,
        &s_strip
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RGB-led GPIO%d init mislukt: %s",
                 STATUS_LED_GPIO, esp_err_to_name(err));
        return err;
    }

    s_available = true;
    led_strip_clear(s_strip);

    ESP_LOGI(TAG, "RGB status-led actief op GPIO%d via led_strip 3.x",
             STATUS_LED_GPIO);

    render(STATUS_LED_STATE_STARTING, esp_timer_get_time());
    return ESP_OK;
}

void status_led_set_ota_active(bool active)
{
    s_ota_active = active;
}

void status_led_update(bool wifi_connected,
                       bool mqtt_connected,
                       bool spa_connected,
                       bool spa_data_valid,
                       int spa_error_code,
                       bool matter_active,
                       bool recovery_ap_active)
{
    if (!s_available) return;

    int64_t now_us = esp_timer_get_time();

    if (now_us - s_last_render_us < 100000) return;
    s_last_render_us = now_us;

    if (matter_active && !s_matter_pulse_started) {
        s_matter_pulse_started = true;
        s_matter_pulse_until_us = now_us + 1500000;
    }

    status_led_state_t wanted;

    if (s_ota_active)
        wanted = STATUS_LED_STATE_OTA;
    else if (spa_error_code != 0)
        wanted = STATUS_LED_STATE_ERROR;
    else if (recovery_ap_active)
        wanted = STATUS_LED_STATE_RECOVERY_AP;
    else if (!wifi_connected)
        wanted = STATUS_LED_STATE_WIFI_OFFLINE;
    else if (!spa_connected || !spa_data_valid)
        wanted = STATUS_LED_STATE_SPA_OFFLINE;
    else if (!mqtt_connected)
        wanted = STATUS_LED_STATE_MQTT_OFFLINE;
    else if (matter_active && now_us < s_matter_pulse_until_us)
        wanted = STATUS_LED_STATE_MATTER_BOOT;
    else
        wanted = STATUS_LED_STATE_OK;

    render(wanted, now_us);
}

bool status_led_available(void)
{
    return s_available;
}

status_led_state_t status_led_state(void)
{
    return s_state;
}

const char *status_led_state_name(void)
{
    switch (s_state) {
        case STATUS_LED_STATE_OK:           return "Alles OK";
        case STATUS_LED_STATE_MQTT_OFFLINE: return "MQTT niet verbonden";
        case STATUS_LED_STATE_SPA_OFFLINE:  return "Spa niet verbonden";
        case STATUS_LED_STATE_WIFI_OFFLINE: return "WiFi niet verbonden";
        case STATUS_LED_STATE_RECOVERY_AP:  return "Recovery AP actief";
        case STATUS_LED_STATE_OTA:          return "OTA bezig";
        case STATUS_LED_STATE_ERROR:        return "Spa-fout";
        case STATUS_LED_STATE_MATTER_BOOT:  return "Matter actief";
        default:                            return "Opstarten";
    }
}

const char *status_led_color_name(void)
{
    switch (s_state) {
        case STATUS_LED_STATE_OK:           return "Groen";
        case STATUS_LED_STATE_MQTT_OFFLINE: return "Blauw";
        case STATUS_LED_STATE_SPA_OFFLINE:  return "Geel";
        case STATUS_LED_STATE_WIFI_OFFLINE: return "Blauw";
        case STATUS_LED_STATE_RECOVERY_AP:  return "Blauw";
        case STATUS_LED_STATE_OTA:          return "Wit";
        case STATUS_LED_STATE_ERROR:        return "Rood";
        case STATUS_LED_STATE_MATTER_BOOT:  return "Paars";
        default:                            return "Wit";
    }
}

bool status_led_is_blinking(void)
{
    return s_blinking;
}

uint8_t status_led_gpio(void)
{
    return STATUS_LED_GPIO;
}
