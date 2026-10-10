#include "web_server.h"
#include "spa_state.h"
#include "bestway_bus.h"
#include "scheduler_manager.h"
#include "mqtt_manager.h"
#include "status_led.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_ota_ops.h>
#include <esp_app_desc.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "esp_heap_caps.h"
#include <cmath>
#include <string>
#include <algorithm>
#include <vector>
#include <cctype>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include <ctime>
#include <cstdlib>
#include "esp_system.h"

static const char *TAG = "web_server";
static httpd_handle_t g_server = nullptr;

static web_matter_commissioned_cb_t g_matter_commissioned_cb = nullptr;
static web_matter_reset_cb_t g_matter_reset_cb = nullptr;
static web_matter_runtime_active_cb_t g_matter_runtime_active_cb = nullptr;
static web_matter_wifi_sync_cb_t g_matter_wifi_sync_cb = nullptr;

static web_matter_status_cb_t g_matter_status_cb = nullptr;
static web_matter_open_window_cb_t g_matter_open_window_cb = nullptr;

static constexpr const char *MATTER_CFG_NAMESPACE = "spa_matter";
static constexpr const char *MATTER_ENABLED_KEY = "enabled";

void web_matter_register_callbacks(web_matter_commissioned_cb_t commissioned_cb,
                                   web_matter_reset_cb_t reset_cb,
                                   web_matter_runtime_active_cb_t runtime_active_cb,
                                   web_matter_wifi_sync_cb_t wifi_sync_cb,
                                   web_matter_status_cb_t status_cb,
                                   web_matter_open_window_cb_t open_window_cb)
{
    g_matter_commissioned_cb = commissioned_cb;
    g_matter_reset_cb = reset_cb;
    g_matter_runtime_active_cb = runtime_active_cb;
    g_matter_wifi_sync_cb = wifi_sync_cb;
    g_matter_status_cb = status_cb;
    g_matter_open_window_cb = open_window_cb;
}

bool web_matter_enabled()
{
    nvs_handle_t nvs = 0;
    uint8_t enabled = 1;

    const esp_err_t open_err = nvs_open(MATTER_CFG_NAMESPACE, NVS_READONLY, &nvs);
    if (open_err == ESP_ERR_NVS_NOT_FOUND) {
        return true;
    }
    if (open_err != ESP_OK) {
        ESP_LOGW(TAG, "Matter instelling lezen mislukt: %s", esp_err_to_name(open_err));
        return true;
    }

    const esp_err_t get_err = nvs_get_u8(nvs, MATTER_ENABLED_KEY, &enabled);
    nvs_close(nvs);

    if (get_err != ESP_OK && get_err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Matter enabled lezen mislukt: %s", esp_err_to_name(get_err));
        return true;
    }

    return enabled != 0;
}

static esp_err_t save_matter_enabled(bool enabled)
{
    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(MATTER_CFG_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_u8(nvs, MATTER_ENABLED_KEY, enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

#define EMBED(name, sym) \
    extern const unsigned char sym##_start[] asm("_binary_" name "_start"); \
    extern const unsigned char sym##_end[]   asm("_binary_" name "_end")

EMBED("index_html", web_index_html);
EMBED("app_css", web_css_app_css);
EMBED("app_js", web_js_app_js);
EMBED("languages_json", web_lang_languages_json);
EMBED("nl_json", web_lang_nl_json);
EMBED("en_json", web_lang_en_json);
EMBED("de_json", web_lang_de_json);
EMBED("fr_json", web_lang_fr_json);
EMBED("dashboard_html", web_pages_dashboard_html);
EMBED("mqtt_html", web_pages_mqtt_html);
EMBED("settings_html", web_pages_settings_html);
EMBED("spa_html", web_pages_spa_html);
EMBED("system_html", web_pages_system_html);
EMBED("wifi_html", web_pages_wifi_html);
EMBED("manifest_json", web_manifest_json);
EMBED("sw_js", web_sw_js);
EMBED("icon_svg", web_icons_icon_svg);
EMBED("favicon_32_png", web_icons_favicon_32_png);
EMBED("icon_64_png", web_icons_icon_64_png);
EMBED("icon_192_png", web_icons_icon_192_png);
EMBED("icon_512_png", web_icons_icon_512_png);
EMBED("apple_touch_icon_png", web_icons_apple_touch_icon_png);
EMBED("ptw_logo_png", web_icons_ptw_logo_png);
EMBED("matter_qr_png", web_icons_matter_qr_png);

struct Asset {
    const char *uri;
    const char *mime;
    const unsigned char *start;
    const unsigned char *end;
};

static const Asset ASSETS[] = {
    {"/", "text/html; charset=utf-8", web_index_html_start, web_index_html_end},
    {"/index.html", "text/html; charset=utf-8", web_index_html_start, web_index_html_end},
    {"/css/app.css", "text/css; charset=utf-8", web_css_app_css_start, web_css_app_css_end},
    {"/js/app.js", "application/javascript; charset=utf-8", web_js_app_js_start, web_js_app_js_end},
    {"/lang/languages.json", "application/json; charset=utf-8", web_lang_languages_json_start, web_lang_languages_json_end},
    {"/lang/nl.json", "application/json; charset=utf-8", web_lang_nl_json_start, web_lang_nl_json_end},
    {"/lang/en.json", "application/json; charset=utf-8", web_lang_en_json_start, web_lang_en_json_end},
    {"/lang/de.json", "application/json; charset=utf-8", web_lang_de_json_start, web_lang_de_json_end},
    {"/lang/fr.json", "application/json; charset=utf-8", web_lang_fr_json_start, web_lang_fr_json_end},
    {"/pages/dashboard.html", "text/html; charset=utf-8", web_pages_dashboard_html_start, web_pages_dashboard_html_end},
    {"/pages/mqtt.html", "text/html; charset=utf-8", web_pages_mqtt_html_start, web_pages_mqtt_html_end},
    {"/pages/settings.html", "text/html; charset=utf-8", web_pages_settings_html_start, web_pages_settings_html_end},
    {"/pages/spa.html", "text/html; charset=utf-8", web_pages_spa_html_start, web_pages_spa_html_end},
    {"/pages/system.html", "text/html; charset=utf-8", web_pages_system_html_start, web_pages_system_html_end},
    {"/pages/wifi.html", "text/html; charset=utf-8", web_pages_wifi_html_start, web_pages_wifi_html_end},
    {"/manifest.json", "application/manifest+json", web_manifest_json_start, web_manifest_json_end},
    {"/sw.js", "application/javascript; charset=utf-8", web_sw_js_start, web_sw_js_end},
    {"/icons/icon.svg", "image/svg+xml", web_icons_icon_svg_start, web_icons_icon_svg_end},
    {"/icons/favicon-32.png", "image/png", web_icons_favicon_32_png_start, web_icons_favicon_32_png_end},
    {"/icons/icon-64.png", "image/png", web_icons_icon_64_png_start, web_icons_icon_64_png_end},
    {"/icons/icon-192.png", "image/png", web_icons_icon_192_png_start, web_icons_icon_192_png_end},
    {"/icons/icon-512.png", "image/png", web_icons_icon_512_png_start, web_icons_icon_512_png_end},
    {"/icons/apple-touch-icon.png", "image/png", web_icons_apple_touch_icon_png_start, web_icons_apple_touch_icon_png_end},
    {"/icons/ptw-logo.png", "image/png", web_icons_ptw_logo_png_start, web_icons_ptw_logo_png_end},
    {"/icons/matter-qr.png", "image/png", web_icons_matter_qr_png_start, web_icons_matter_qr_png_end},
};

static void send_json(httpd_req_t *req, const std::string &json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, json.c_str(), json.size());
}


/*
 * ============================================================
 * Historie
 * ============================================================
 *
 * 288 samples x 5 minuten = 24 uur.
 *
 * De frontend verwacht:
 *   timestamp, temperature, target,
 *   heater, filter, bubbles, jets
 *
 * We bewaren aanvullend:
 *   heaterActive, power
 *
 * Historie draait los van de webpagina zodat metingen ook
 * doorgaan wanneer er geen browser open staat.
 */

static constexpr size_t HISTORY_MAX_SAMPLES = 288;
static constexpr uint64_t HISTORY_INTERVAL_MS = 5ULL * 60ULL * 1000ULL;
static constexpr unsigned HISTORY_SAVE_EVERY = 3;

#pragma pack(push, 1)
struct HistorySample {
    int64_t timestamp;
    int16_t temperature;
    int16_t target;
    uint8_t flags;
};
#pragma pack(pop)

enum : uint8_t {
    HISTORY_HEATER         = 1U << 0,
    HISTORY_HEATER_ACTIVE  = 1U << 1,
    HISTORY_FILTER         = 1U << 2,
    HISTORY_BUBBLES        = 1U << 3,
    HISTORY_JETS           = 1U << 4,
    HISTORY_POWER          = 1U << 5,
};

static std::vector<HistorySample> g_history;
static SemaphoreHandle_t g_history_mutex = nullptr;
static TaskHandle_t g_history_task = nullptr;
static unsigned g_history_dirty_samples = 0;

static void history_lock()
{
    if (g_history_mutex) {
        xSemaphoreTake(g_history_mutex, portMAX_DELAY);
    }
}

static void history_unlock()
{
    if (g_history_mutex) {
        xSemaphoreGive(g_history_mutex);
    }
}

static void history_save()
{
    std::vector<HistorySample> snapshot;

    history_lock();
    snapshot = g_history;
    g_history_dirty_samples = 0;
    history_unlock();

    nvs_handle_t nvs = 0;

    if (nvs_open_from_partition("bridge_info", "spa_history", NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGW(TAG, "Historie NVS openen mislukt");
        return;
    }

    esp_err_t err = ESP_OK;

    if (snapshot.empty()) {
        err = nvs_erase_key(nvs, "samples");

        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    } else {
        err = nvs_set_blob(
            nvs,
            "samples",
            snapshot.data(),
            snapshot.size() * sizeof(HistorySample));
    }

    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Historie opslaan mislukt: %s",
            esp_err_to_name(err));
    }
}

static void history_load()
{
    g_history.clear();
    g_history.reserve(HISTORY_MAX_SAMPLES);

    nvs_handle_t nvs = 0;

    if (nvs_open_from_partition("bridge_info", "spa_history", NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }

    size_t size = 0;
    esp_err_t err =
        nvs_get_blob(nvs, "samples", nullptr, &size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs);
        return;
    }

    if (err != ESP_OK ||
        size == 0 ||
        (size % sizeof(HistorySample)) != 0) {

        nvs_close(nvs);
        ESP_LOGW(TAG, "Historie NVS bevat ongeldige data");
        return;
    }

    const size_t count =
        size / sizeof(HistorySample);

    if (count > HISTORY_MAX_SAMPLES) {
        nvs_close(nvs);
        ESP_LOGW(TAG, "Historie NVS bevat te veel samples");
        return;
    }

    std::vector<HistorySample> loaded(count);

    err = nvs_get_blob(
        nvs,
        "samples",
        loaded.data(),
        &size);

    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Historie laden mislukt: %s",
            esp_err_to_name(err));
        return;
    }

    history_lock();
    g_history = std::move(loaded);
    g_history_dirty_samples = 0;
    history_unlock();

    ESP_LOGI(
        TAG,
        "Historie geladen: %u samples",
        static_cast<unsigned>(count));
}

static void history_add_sample()
{
    /*
     * Alleen echte geldige spa-data opslaan.
     */
    if (!spa.connected || !spa.data_valid) {
        return;
    }

    const time_t epoch = time(nullptr);

    /*
     * Geen 1970-timestamps in de grafiek zetten.
     * Wacht totdat SNTP een echte datum heeft.
     */
    if (epoch < 1700000000) {
        return;
    }

    HistorySample sample = {};
    sample.timestamp =
        static_cast<int64_t>(epoch);

    sample.temperature =
        static_cast<int16_t>(spa.temperature_c);

    sample.target =
        static_cast<int16_t>(spa.target_temperature_c);

    if (spa.heater) {
        sample.flags |= HISTORY_HEATER;
    }

    if (spa.heater_active) {
        sample.flags |= HISTORY_HEATER_ACTIVE;
    }

    if (spa.filter) {
        sample.flags |= HISTORY_FILTER;
    }

    if (spa.bubbles) {
        sample.flags |= HISTORY_BUBBLES;
    }

    if (spa.jets) {
        sample.flags |= HISTORY_JETS;
    }

    if (spa.power) {
        sample.flags |= HISTORY_POWER;
    }

    bool save_now = false;

    history_lock();

    if (g_history.size() >= HISTORY_MAX_SAMPLES) {
        g_history.erase(g_history.begin());
    }

    g_history.push_back(sample);

    ++g_history_dirty_samples;

    if (g_history_dirty_samples >= HISTORY_SAVE_EVERY) {
        save_now = true;
    }

    history_unlock();

    ESP_LOGD(
        TAG,
        "Historie sample: T=%d target=%d count=%u",
        spa.temperature_c,
        spa.target_temperature_c,
        static_cast<unsigned>(g_history.size()));

    if (save_now) {
        history_save();
    }
}

static void history_task_fn(void *)
{
    /*
     * Eerste sample zodra:
     * - spa-data geldig is
     * - klok gesynchroniseerd is
     *
     * Daarna iedere 5 minuten.
     */
    uint64_t last_sample_ms = 0;

    while (true) {
        const uint64_t now =
            static_cast<uint64_t>(
                esp_timer_get_time() / 1000ULL);

        if (spa.connected &&
            spa.data_valid &&
            time(nullptr) >= 1700000000) {

            if (last_sample_ms == 0 ||
                (now - last_sample_ms) >=
                    HISTORY_INTERVAL_MS) {

                history_add_sample();
                last_sample_ms = now;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static esp_err_t api_history_get(httpd_req_t *req)
{
    size_t limit = HISTORY_MAX_SAMPLES;

    char query[64] = {};

    if (httpd_req_get_url_query_str(
            req,
            query,
            sizeof(query)) == ESP_OK) {

        char value[16] = {};

        if (httpd_query_key_value(
                query,
                "limit",
                value,
                sizeof(value)) == ESP_OK) {

            const long parsed =
                std::strtol(value, nullptr, 10);

            if (parsed > 0) {
                limit =
                    std::min(
                        static_cast<size_t>(parsed),
                        HISTORY_MAX_SAMPLES);
            }
        }
    }

    std::vector<HistorySample> samples;

    history_lock();

    const size_t count = g_history.size();
    const size_t start =
        count > limit
            ? count - limit
            : 0;

    samples.assign(
        g_history.begin() + start,
        g_history.end());

    history_unlock();

    std::string json;
    json.reserve(samples.size() * 150 + 2);
    json += "[";

    for (size_t i = 0; i < samples.size(); ++i) {
        const HistorySample &item =
            samples[i];

        if (i != 0) {
            json += ",";
        }

        char buf[256];

        std::snprintf(
            buf,
            sizeof(buf),
            "{\"timestamp\":%lld,"
            "\"temperature\":%d,"
            "\"target\":%d,"
            "\"heater\":%s,"
            "\"heaterActive\":%s,"
            "\"filter\":%s,"
            "\"bubbles\":%s,"
            "\"jets\":%s,"
            "\"power\":%s}",
            static_cast<long long>(item.timestamp),
            static_cast<int>(item.temperature),
            static_cast<int>(item.target),
            (item.flags & HISTORY_HEATER)
                ? "true" : "false",
            (item.flags & HISTORY_HEATER_ACTIVE)
                ? "true" : "false",
            (item.flags & HISTORY_FILTER)
                ? "true" : "false",
            (item.flags & HISTORY_BUBBLES)
                ? "true" : "false",
            (item.flags & HISTORY_JETS)
                ? "true" : "false",
            (item.flags & HISTORY_POWER)
                ? "true" : "false");

        json += buf;
    }

    json += "]";

    send_json(req, json);
    return ESP_OK;
}

static esp_err_t api_history_delete(httpd_req_t *req)
{
    history_lock();
    g_history.clear();
    g_history_dirty_samples = 0;
    history_unlock();

    /*
     * Wissen moet meteen persistent zijn.
     */
    history_save();

    send_json(
        req,
        "{\"ok\":true,\"cleared\":\"history\"}");

    return ESP_OK;
}


static void get_network_info(char *ip, size_t ip_len, int *rssi, bool *wifi)
{
    std::snprintf(ip, ip_len, "%s", "--");
    *rssi = 0;
    *wifi = false;

    wifi_ap_record_t ap = {};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        *wifi = true;
        *rssi = ap.rssi;
    }

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t info = {};
    if (netif && esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr != 0) {
        std::snprintf(ip, ip_len, IPSTR, IP2STR(&info.ip));
        *wifi = true;
    }
}

static std::string status_json()
{
    char ip[24];
    int rssi = 0;
    bool wifi = false;
    get_network_info(ip, sizeof(ip), &rssi, &wifi);

    const std::string up = spa.uptime();

    // Na reboot is de actuele watertemperatuur direct geldig zodra CIO deze
    // bevestigt. De target blijft bewust onbekend totdat de pomp hem echt
    // op het display heeft getoond na een UP/DOWN-actie.
    char target_json[16] = "null";
    if (spa.target_temperature_valid) {
        std::snprintf(target_json, sizeof(target_json), "%d", spa.target_temperature_c);
    }

    char buf[800];
    std::snprintf(buf, sizeof(buf),
        "{\"wifi\":%s,\"mqtt\":%s,\"dataValid\":%s,\"godMode\":%s,\"fullPower\":%s,"
        "\"temperature\":%d,\"target\":%s,\"targetValid\":%s,\"errorCode\":%d,\"heater\":%s,\"heaterActive\":%s,"
        "\"filter\":%s,\"bubbles\":%s,\"jets\":%s,\"power\":%s,"
        "\"locked\":%s,\"fahrenheit\":%s,\"timerActive\":%s,"
        "\"uptime\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"heap\":%u,"
        "\"chipId\":\"ESP32-C6\",\"busMode\":\"%s\",\"protocolFamily\":\"%s\"}",
        wifi ? "true" : "false",
        spa.mqtt_online ? "true" : "false",
        spa.data_valid ? "true" : "false",
        bestway_bus.godmode() ? "true" : "false",
        bestway_bus.godmode_full_power() ? "true" : "false",
        spa.temperature_c,
        target_json,
        spa.target_temperature_valid ? "true" : "false",
        spa.error_code,
        spa.heater ? "true" : "false", spa.heater_active ? "true" : "false",
        spa.filter ? "true" : "false", spa.bubbles ? "true" : "false",
        spa.jets ? "true" : "false", spa.power ? "true" : "false",
        spa.locked ? "true" : "false", spa.fahrenheit ? "true" : "false",
        spa.timer_active ? "true" : "false", up.c_str(), ip, rssi,
        static_cast<unsigned>(esp_get_free_heap_size()), bestway_bus.mode_name(),
        bestway_bus.model_info().family == BestwayProtocolFamily::Uart4Wire ? "4wire" :
        bestway_bus.model_info().family == BestwayProtocolFamily::SixWireType2 ? "type2" :
        "type1");
    return std::string(buf);
}


static std::string json_escape(const std::string &value);
static std::string form_value(const std::string &body,
                              const std::string &key);
static esp_err_t read_request_body(httpd_req_t *req,
                                   std::string &body,
                                   size_t max_len = 512);

struct HardwareConfig {
    uint8_t cio = 0;
    uint8_t dsp = 0;
    std::string pcb = "esp32c6";
    bool has_temp_sensor = false;
    uint8_t pins[8] = {23, 22, 21, 20, 19, 18, 11, 0};
};

static bool json_get_int(const std::string &body, const char *key, int &value)
{
    const std::string needle = std::string("\"") + key + "\"";
    size_t pos = body.find(needle);
    if (pos == std::string::npos) return false;

    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return false;
    ++pos;

    while (pos < body.size() &&
           std::isspace(static_cast<unsigned char>(body[pos]))) {
        ++pos;
    }

    char *end = nullptr;
    long parsed = std::strtol(body.c_str() + pos, &end, 10);
    if (end == body.c_str() + pos) return false;

    value = static_cast<int>(parsed);
    return true;
}

static bool json_get_string(const std::string &body,
                            const char *key,
                            std::string &value)
{
    const std::string needle = std::string("\"") + key + "\"";
    size_t pos = body.find(needle);
    if (pos == std::string::npos) return false;

    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return false;

    pos = body.find('"', pos + 1);
    if (pos == std::string::npos) return false;

    const size_t end = body.find('"', pos + 1);
    if (end == std::string::npos) return false;

    value = body.substr(pos + 1, end - pos - 1);
    return true;
}

static bool json_get_bool(
    const std::string &body,
    const char *key,
    bool &value)
{
    const std::string needle =
        std::string("\"") + key + "\"";

    size_t pos = body.find(needle);
    if (pos == std::string::npos) return false;

    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return false;

    ++pos;

    while (pos < body.size() &&
           std::isspace(static_cast<unsigned char>(body[pos]))) {
        ++pos;
    }

    if (body.compare(pos, 4, "true") == 0) {
        value = true;
        return true;
    }

    if (body.compare(pos, 5, "false") == 0) {
        value = false;
        return true;
    }

    if (pos < body.size() && body[pos] == '1') {
        value = true;
        return true;
    }

    if (pos < body.size() && body[pos] == '0') {
        value = false;
        return true;
    }

    return false;
}


static bool json_get_object(
    const std::string &body,
    const char *key,
    std::string &object)
{
    const std::string needle =
        std::string("\"") + key + "\"";

    size_t pos = body.find(needle);
    if (pos == std::string::npos) return false;

    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return false;

    pos = body.find('{', pos + 1);
    if (pos == std::string::npos) return false;

    const size_t start = pos;

    int depth = 0;
    bool in_string = false;
    bool escape = false;

    for (; pos < body.size(); ++pos) {
        const char c = body[pos];

        if (in_string) {
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                in_string = false;
            }

            continue;
        }

        if (c == '"') {
            in_string = true;
            continue;
        }

        if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;

            if (depth == 0) {
                object = body.substr(
                    start,
                    pos - start + 1);

                return true;
            }
        }
    }

    return false;
}


static bool json_get_pin_array(const std::string &body, uint8_t pins[8])
{
    size_t pos = body.find("\"pins\"");
    if (pos == std::string::npos) return false;

    pos = body.find('[', pos);
    if (pos == std::string::npos) return false;
    ++pos;

    for (int i = 0; i < 8; ++i) {
        while (pos < body.size() &&
               std::isspace(static_cast<unsigned char>(body[pos]))) {
            ++pos;
        }

        char *end = nullptr;
        long pin = std::strtol(body.c_str() + pos, &end, 10);

        if (end == body.c_str() + pos || pin < 0 || pin > 30) {
            return false;
        }

        pins[i] = static_cast<uint8_t>(pin);
        pos = static_cast<size_t>(end - body.c_str());

        while (pos < body.size() &&
               std::isspace(static_cast<unsigned char>(body[pos]))) {
            ++pos;
        }

        if (i < 7) {
            if (pos >= body.size() || body[pos] != ',') return false;
            ++pos;
        }
    }

    while (pos < body.size() &&
           std::isspace(static_cast<unsigned char>(body[pos]))) {
        ++pos;
    }

    return pos < body.size() && body[pos] == ']';
}

static void load_hardware_config(HardwareConfig &cfg)
{
    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open("spa_hw", NVS_READONLY, &nvs);

    if (err != ESP_OK) {
        return; // factory defaults
    }

    uint8_t value = 0;

    if (nvs_get_u8(nvs, "cio", &value) == ESP_OK && value <= 8) {
        cfg.cio = value;
    }

    if (nvs_get_u8(nvs, "dsp", &value) == ESP_OK && value <= 8) {
        cfg.dsp = value;
    }

    if (nvs_get_u8(nvs, "temp", &value) == ESP_OK) {
        cfg.has_temp_sensor = value != 0;
    }

    // Deze firmware is uitsluitend voor onze ESP32-C6 print.
    // Een eventuele oude pcb-keuze in NVS wordt bewust genegeerd.
    cfg.pcb = "esp32c6";

    uint8_t stored_pins[8] = {};
    size_t pins_len = sizeof(stored_pins);

    if (nvs_get_blob(nvs, "pins", stored_pins, &pins_len) == ESP_OK &&
        pins_len == sizeof(stored_pins)) {
        static constexpr uint8_t OLD_6WIRE_PINS[8] = {4, 5, 6, 7, 10, 11, 18, 19};
        static constexpr uint8_t NEW_6WIRE_PINS[8] = {23, 22, 21, 20, 19, 18, 11, 0};
        static constexpr uint8_t OLD_4WIRE_PINS[8] = {4, 5, 0, 7, 10, 0, 0, 19};
        static constexpr uint8_t NEW_4WIRE_PINS[8] = {23, 22, 0, 20, 19, 0, 0, 0};

        const bool old_6wire = std::memcmp(stored_pins, OLD_6WIRE_PINS, sizeof(OLD_6WIRE_PINS)) == 0;
        const bool old_4wire = std::memcmp(stored_pins, OLD_4WIRE_PINS, sizeof(OLD_4WIRE_PINS)) == 0;

        if (old_6wire || old_4wire) {
            const uint8_t *replacement = old_4wire ? NEW_4WIRE_PINS : NEW_6WIRE_PINS;
            std::memcpy(cfg.pins, replacement, sizeof(cfg.pins));
            ESP_LOGW(TAG, "Hardware API: oude ESP32-C6 GPIO-preset als nieuwe printmapping weergegeven");
        } else {
            std::memcpy(cfg.pins, stored_pins, sizeof(cfg.pins));
        }
    }

    nvs_close(nvs);
}

static esp_err_t save_hardware_config(const HardwareConfig &cfg)
{
    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open("spa_hw", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    if ((err = nvs_set_u8(nvs, "cio", cfg.cio)) != ESP_OK ||
        (err = nvs_set_u8(nvs, "dsp", cfg.dsp)) != ESP_OK ||
        (err = nvs_set_u8(nvs, "temp", cfg.has_temp_sensor ? 1 : 0)) != ESP_OK ||
        (err = nvs_set_str(nvs, "pcb", cfg.pcb.c_str())) != ESP_OK ||
        (err = nvs_set_blob(nvs, "pins", cfg.pins, sizeof(cfg.pins))) != ESP_OK ||
        (err = nvs_commit(nvs)) != ESP_OK) {
        nvs_close(nvs);
        return err;
    }

    nvs_close(nvs);
    return ESP_OK;
}

static std::string hardware_config_json(const HardwareConfig &cfg)
{
    char buf[512];

    std::snprintf(
        buf,
        sizeof(buf),
        "{\"ok\":true,"
        "\"cio\":%u,"
        "\"dsp\":%u,"
        "\"pcb\":\"%s\","
        "\"hasTempSensor\":\"%u\","
        "\"pins\":[%u,%u,%u,%u,%u,%u,%u,%u]}",
        static_cast<unsigned>(cfg.cio),
        static_cast<unsigned>(cfg.dsp),
        json_escape(cfg.pcb).c_str(),
        cfg.has_temp_sensor ? 1U : 0U,
        static_cast<unsigned>(cfg.pins[0]),
        static_cast<unsigned>(cfg.pins[1]),
        static_cast<unsigned>(cfg.pins[2]),
        static_cast<unsigned>(cfg.pins[3]),
        static_cast<unsigned>(cfg.pins[4]),
        static_cast<unsigned>(cfg.pins[5]),
        static_cast<unsigned>(cfg.pins[6]),
        static_cast<unsigned>(cfg.pins[7]));

    return std::string(buf);
}


static bool web_demo_mode_read()
{
    bool enabled =
        bestway_bus.mode() == BestwayBus::Mode::Simulation;

    nvs_handle_t nvs = 0;

    if (nvs_open("spa_demo", NVS_READONLY, &nvs) != ESP_OK) {
        return enabled;
    }

    uint8_t value = enabled ? 1 : 0;

    if (nvs_get_u8(nvs, "enabled", &value) == ESP_OK) {
        enabled = value != 0;
    }

    nvs_close(nvs);
    return enabled;
}


static esp_err_t web_demo_mode_write(bool enabled)
{
    nvs_handle_t nvs = 0;

    esp_err_t err =
        nvs_open("spa_demo", NVS_READWRITE, &nvs);

    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(
        nvs,
        "enabled",
        enabled ? 1 : 0);

    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);
    return err;
}


static esp_err_t api_demo_get(httpd_req_t *req)
{
    const bool enabled = web_demo_mode_read();

    send_json(
        req,
        enabled
            ? "{\"ok\":true,\"enabled\":true}"
            : "{\"ok\":true,\"enabled\":false}");

    return ESP_OK;
}


static esp_err_t api_demo_post(httpd_req_t *req)
{
    std::string body;

    if (read_request_body(req, body) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        send_json(
            req,
            "{\"ok\":false,\"error\":\"Ongeldige request\"}");
        return ESP_OK;
    }

    int enabled = -1;

    if (!json_get_int(body, "enabled", enabled) ||
        (enabled != 0 && enabled != 1)) {

        httpd_resp_set_status(req, "400 Bad Request");
        send_json(
            req,
            "{\"ok\":false,\"error\":\"Ongeldige demo-instelling\"}");
        return ESP_OK;
    }

    const esp_err_t err =
        web_demo_mode_write(enabled != 0);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Demo-modus opslaan mislukt: %s",
            esp_err_to_name(err));

        httpd_resp_set_status(
            req,
            "500 Internal Server Error");

        send_json(
            req,
            "{\"ok\":false,\"error\":\"Demo-modus kon niet worden opgeslagen\"}");

        return ESP_OK;
    }

    ESP_LOGW(
        TAG,
        "Demo-modus opgeslagen: %s",
        enabled ? "AAN" : "UIT");

    send_json(
        req,
        enabled
            ? "{\"ok\":true,\"enabled\":true,\"restartRequired\":true}"
            : "{\"ok\":true,\"enabled\":false,\"restartRequired\":true}");

    return ESP_OK;
}



static esp_err_t api_hardware_test(httpd_req_t *req)
{
    /*
     * Test alleen uitvoeren wanneer de echte spa niet actief
     * met de hardwarebus verbonden is.
     *
     * Simulation telt hier bewust niet als fysieke pompverbinding.
     */
    if (bestway_bus.mode() == BestwayBus::Mode::Hardware &&
        bestway_bus.is_connected()) {

        httpd_resp_set_status(req, "409 Conflict");

        send_json(
            req,
            "{\"ok\":false,\"error\":\"spaConnected\"}");

        return ESP_OK;
    }

    const auto result =
        bestway_bus.run_connector_test();

    if (!result.supported) {
        httpd_resp_set_status(req, "409 Conflict");

        send_json(
            req,
            "{\"ok\":false,\"error\":\"unsupportedProtocol\"}");

        return ESP_OK;
    }

    std::string json =
        "{\"ok\":true,\"pairs\":[";

    for (int i = 0; i < 3; ++i) {
        if (i) {
            json += ",";
        }

        const auto &p = result.pairs[i];

        char item[320];

        std::snprintf(
            item,
            sizeof(item),
            "{"
            "\"name\":\"%s\","
            "\"applicable\":%s,"
            "\"cioPin\":\"GPIO%u\","
            "\"dspPin\":\"GPIO%u\","
            "\"forwardErrors\":%d,"
            "\"reverseErrors\":%d,"
            "\"pass\":%s"
            "}",
            p.name,
            p.applicable ? "true" : "false",
            static_cast<unsigned>(p.cio_pin),
            static_cast<unsigned>(p.dsp_pin),
            p.forward_errors,
            p.reverse_errors,
            p.pass ? "true" : "false");

        json += item;
    }

    json += "],\"overallPass\":";
    json += result.overall_pass ? "true" : "false";
    json +=
        ",\"powerTested\":false,"
        "\"audioTested\":false}";

    send_json(req, json);
    return ESP_OK;
}


static esp_err_t api_hardware_get(httpd_req_t *req)
{
    HardwareConfig cfg;
    load_hardware_config(cfg);

    send_json(req, hardware_config_json(cfg));
    return ESP_OK;
}

static esp_err_t api_hardware_post(httpd_req_t *req)
{
    std::string body;

    if (read_request_body(req, body) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        send_json(req, "{\"ok\":false,\"error\":\"Ongeldige request\"}");
        return ESP_OK;
    }

    HardwareConfig cfg;
    load_hardware_config(cfg);

    int cio = -1;
    int dsp = -1;
    int temp = 0;

    if (!json_get_int(body, "cio", cio) ||
        !json_get_int(body, "dsp", dsp) ||
        cio < 0 || cio > 8 ||
        dsp < 0 || dsp > 8) {

        httpd_resp_set_status(req, "400 Bad Request");
        send_json(req,
                  "{\"ok\":false,\"error\":\"Ongeldig CIO/DSP-model\"}");
        return ESP_OK;
    }

    const auto &cio_info =
        bestway_model_info(static_cast<BestwayModel>(cio));
    const auto &dsp_info =
        bestway_model_info(static_cast<BestwayModel>(dsp));

    if (cio_info.family != dsp_info.family) {
        httpd_resp_set_status(req, "400 Bad Request");
        send_json(req,
                  "{\"ok\":false,\"error\":\"CIO en DSP gebruiken verschillende protocolfamilies\"}");
        return ESP_OK;
    }

    if (!json_get_pin_array(body, cfg.pins)) {
        httpd_resp_set_status(req, "400 Bad Request");
        send_json(req,
                  "{\"ok\":false,\"error\":\"Ongeldige GPIO-configuratie\"}");
        return ESP_OK;
    }

    // Frontend stuurt hasTempSensor momenteel als "0" of "1".
    const std::string temp_key = "\"hasTempSensor\"";
    const size_t temp_pos = body.find(temp_key);

    if (temp_pos != std::string::npos) {
        size_t colon = body.find(':', temp_pos + temp_key.size());
        if (colon != std::string::npos) {
            size_t one = body.find_first_not_of(" \t\r\n\"", colon + 1);
            if (one != std::string::npos) {
                temp = body[one] == '1' ? 1 : 0;
            }
        }
    }

    cfg.cio = static_cast<uint8_t>(cio);
    cfg.dsp = static_cast<uint8_t>(dsp);
    cfg.pcb = "esp32c6";
    cfg.has_temp_sensor = temp != 0;

    const esp_err_t err = save_hardware_config(cfg);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Hardwareconfig opslaan mislukt: %s",
                 esp_err_to_name(err));

        httpd_resp_set_status(req, "500 Internal Server Error");
        send_json(req,
                  "{\"ok\":false,\"error\":\"Hardwareconfig kon niet worden opgeslagen\"}");
        return ESP_OK;
    }

    ESP_LOGI(TAG,
             "Hardwareconfig opgeslagen: CIO=%u DSP=%u PCB=%s pins=%u,%u,%u,%u,%u,%u,%u,%u",
             static_cast<unsigned>(cfg.cio),
             static_cast<unsigned>(cfg.dsp),
             cfg.pcb.c_str(),
             static_cast<unsigned>(cfg.pins[0]),
             static_cast<unsigned>(cfg.pins[1]),
             static_cast<unsigned>(cfg.pins[2]),
             static_cast<unsigned>(cfg.pins[3]),
             static_cast<unsigned>(cfg.pins[4]),
             static_cast<unsigned>(cfg.pins[5]),
             static_cast<unsigned>(cfg.pins[6]),
             static_cast<unsigned>(cfg.pins[7]));

    send_json(req, hardware_config_json(cfg));
    return ESP_OK;
}

static esp_err_t api_status(httpd_req_t *req)
{
    send_json(req, status_json());
    return ESP_OK;
}

static bool valid_time_zone(const std::string &tz)
{
    return
        tz == "Europe/Amsterdam" ||
        tz == "Europe/Brussels" ||
        tz == "Europe/Berlin" ||
        tz == "Europe/Paris" ||
        tz == "Europe/London" ||
        tz == "America/New_York" ||
        tz == "America/Chicago" ||
        tz == "America/Denver" ||
        tz == "America/Los_Angeles" ||
        tz == "Australia/Sydney" ||
        tz == "Asia/Tokyo" ||
        tz == "Asia/Singapore" ||
        tz == "Etc/UTC";
}

struct RegionConfig {
    std::string time_zone = "Europe/Amsterdam";
    std::string date_format = "DD-MM-YYYY";
    bool use_24_hour_clock = true;
};

static void load_region_config(RegionConfig &cfg)
{
    nvs_handle_t nvs = 0;

    if (nvs_open("spa_region", NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }

    char value[64] = {};
    size_t len = sizeof(value);

    if (nvs_get_str(nvs, "tz", value, &len) == ESP_OK &&
        valid_time_zone(value)) {
        cfg.time_zone = value;
    }

    std::memset(value, 0, sizeof(value));
    len = sizeof(value);

    if (nvs_get_str(nvs, "datefmt", value, &len) == ESP_OK &&
        value[0] != '\0') {
        cfg.date_format = value;
    }

    uint8_t clock24 = 1;

    if (nvs_get_u8(nvs, "clock24", &clock24) == ESP_OK) {
        cfg.use_24_hour_clock = clock24 != 0;
    }

    nvs_close(nvs);
}

static esp_err_t save_region_config(const RegionConfig &cfg)
{
    nvs_handle_t nvs = 0;

    esp_err_t err =
        nvs_open("spa_region", NVS_READWRITE, &nvs);

    if (err != ESP_OK) return err;

    err = nvs_set_str(nvs, "tz", cfg.time_zone.c_str());

    if (err == ESP_OK) {
        err = nvs_set_str(
            nvs,
            "datefmt",
            cfg.date_format.c_str());
    }

    if (err == ESP_OK) {
        err = nvs_set_u8(
            nvs,
            "clock24",
            cfg.use_24_hour_clock ? 1 : 0);
    }

    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);
    return err;
}

static std::string region_config_json(const RegionConfig &cfg)
{
    std::string json = "{";

    json += "\"language\":\"Nederlands\",";
    json += "\"temperatureUnit\":\"";
    json += spa.fahrenheit ? "Fahrenheit" : "Celsius";
    json += "\",";

    json += "\"timeZone\":\"";
    json += json_escape(cfg.time_zone);
    json += "\",";

    json += "\"dateFormat\":\"";
    json += json_escape(cfg.date_format);
    json += "\",";

    json += "\"use24HourClock\":";
    json += cfg.use_24_hour_clock ? "true" : "false";

    json += "}";

    return json;
}

static const char *timezone_posix(const std::string &tz)
{
    if (tz == "Europe/Amsterdam" ||
        tz == "Europe/Brussels" ||
        tz == "Europe/Berlin" ||
        tz == "Europe/Paris") {
        return "CET-1CEST,M3.5.0/2,M10.5.0/3";
    }

    if (tz == "Europe/London") {
        return "GMT0BST,M3.5.0/1,M10.5.0/2";
    }

    if (tz == "America/New_York") {
        return "EST5EDT,M3.2.0/2,M11.1.0/2";
    }

    if (tz == "America/Chicago") {
        return "CST6CDT,M3.2.0/2,M11.1.0/2";
    }

    if (tz == "America/Denver") {
        return "MST7MDT,M3.2.0/2,M11.1.0/2";
    }

    if (tz == "America/Los_Angeles") {
        return "PST8PDT,M3.2.0/2,M11.1.0/2";
    }

    if (tz == "Australia/Sydney") {
        return "AEST-10AEDT,M10.1.0/2,M4.1.0/3";
    }

    if (tz == "Asia/Tokyo") {
        return "JST-9";
    }

    if (tz == "Asia/Singapore") {
        return "SGT-8";
    }

    return "UTC0";
}

static void apply_region_timezone()
{
    RegionConfig cfg;
    load_region_config(cfg);

    const char *posix = timezone_posix(cfg.time_zone);

    setenv("TZ", posix, 1);
    tzset();

    ESP_LOGI(TAG,
             "Tijdzone actief: %s (%s)",
             cfg.time_zone.c_str(),
             posix);
}

void web_time_sync_start()
{
    apply_region_timezone();

    if (esp_sntp_enabled()) {
        return;
    }

    ESP_LOGI(TAG, "SNTP tijdsynchronisatie starten");

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();
}

static bool system_time_synchronized()
{
    const time_t now = time(nullptr);

    /*
     * Alles duidelijk na 2020 beschouwen we als een geldige
     * gesynchroniseerde klok.
     */
    return now > 1577836800;
}

static std::string format_local_date(const RegionConfig &cfg,
                                     const struct tm &tm_now)
{
    char buf[32] = {};

    const char *format = "%d-%m-%Y";

    if (cfg.date_format == "MM/DD/YYYY") {
        format = "%m/%d/%Y";
    } else if (cfg.date_format == "YYYY-MM-DD") {
        format = "%Y-%m-%d";
    }

    strftime(buf, sizeof(buf), format, &tm_now);
    return buf;
}

static std::string format_local_time(const RegionConfig &cfg,
                                     const struct tm &tm_now)
{
    char buf[32] = {};

    strftime(buf,
             sizeof(buf),
             cfg.use_24_hour_clock ? "%H:%M" : "%I:%M %p",
             &tm_now);

    return buf;
}

static esp_err_t api_system(httpd_req_t *req)
{
    RegionConfig cfg;
    load_region_config(cfg);

    apply_region_timezone();

    const time_t now = time(nullptr);

    struct tm tm_now = {};
    localtime_r(&now, &tm_now);

    std::string json = "{";

    json += "\"time\":\"";
    json += json_escape(format_local_time(cfg, tm_now));
    json += "\",";

    json += "\"date\":\"";
    json += json_escape(format_local_date(cfg, tm_now));
    json += "\",";

    json += "\"timeSynchronized\":";
    json += system_time_synchronized() ? "true" : "false";
    json += ",";

    json += "\"timeZone\":\"";
    json += json_escape(cfg.time_zone);
    json += "\"";

    json += "}";

    send_json(req, json);
    return ESP_OK;
}


static esp_err_t api_region(httpd_req_t *req)
{
    RegionConfig cfg;
    load_region_config(cfg);

    send_json(req, region_config_json(cfg));
    return ESP_OK;
}

static esp_err_t api_region_post(httpd_req_t *req)
{
    std::string body;

    if (read_request_body(req, body, 512) != ESP_OK) {
        return ESP_FAIL;
    }

    RegionConfig cfg;
    load_region_config(cfg);

    const std::string time_zone =
        form_value(body, "timeZone");

    const std::string date_format =
        form_value(body, "dateFormat");

    const std::string clock24 =
        form_value(body, "use24HourClock");

    if (!valid_time_zone(time_zone)) {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Ongeldige tijdzone");
        return ESP_FAIL;
    }

    if (date_format != "DD-MM-YYYY" &&
        date_format != "MM/DD/YYYY" &&
        date_format != "YYYY-MM-DD") {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Ongeldig datumformaat");
        return ESP_FAIL;
    }

    cfg.time_zone = time_zone;
    cfg.date_format = date_format;
    cfg.use_24_hour_clock =
        clock24 == "true" || clock24 == "1";

    const esp_err_t err =
        save_region_config(cfg);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Regionale instellingen opslaan mislukt: %s",
            esp_err_to_name(err));

        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Opslaan mislukt");

        return ESP_FAIL;
    }

    ESP_LOGI(
        TAG,
        "Regio opgeslagen: tijdzone=%s datum=%s klok24=%s",
        cfg.time_zone.c_str(),
        cfg.date_format.c_str(),
        cfg.use_24_hour_clock ? "ja" : "nee");

    apply_region_timezone();

    send_json(req, region_config_json(cfg));
    return ESP_OK;
}


static std::string json_escape(const std::string &value)
{
    std::string out;
    out.reserve(value.size() + 8);

    for (unsigned char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;

            default:
                if (c >= 0x20) {
                    out += static_cast<char>(c);
                }
                break;
        }
    }

    return out;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static std::string url_decode(const std::string &value)
{
    std::string out;
    out.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') {
            out += ' ';
        } else if (value[i] == '%' && i + 2 < value.size()) {
            const int hi = hex_value(value[i + 1]);
            const int lo = hex_value(value[i + 2]);

            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
            } else {
                out += value[i];
            }
        } else {
            out += value[i];
        }
    }

    return out;
}

static std::string form_value(const std::string &body,
                              const std::string &key)
{
    size_t start = 0;

    while (start <= body.size()) {
        const size_t end = body.find('&', start);
        const std::string item =
            body.substr(start,
                        end == std::string::npos
                            ? std::string::npos
                            : end - start);

        const size_t eq = item.find('=');

        if (eq != std::string::npos) {
            const std::string item_key =
                url_decode(item.substr(0, eq));

            if (item_key == key) {
                return url_decode(item.substr(eq + 1));
            }
        }

        if (end == std::string::npos) break;
        start = end + 1;
    }

    return "";
}

static esp_err_t read_request_body(httpd_req_t *req,
                                   std::string &body,
                                   size_t max_len)
{
    if (req->content_len <= 0 ||
        static_cast<size_t>(req->content_len) > max_len) {

        httpd_resp_send_err(req,
                            HTTPD_400_BAD_REQUEST,
                            "Ongeldige aanvraag");

        return ESP_FAIL;
    }

    body.resize(req->content_len);

    int received = 0;

    while (received < req->content_len) {
        const int n =
            httpd_req_recv(req,
                           body.data() + received,
                           req->content_len - received);

        if (n <= 0) {
            httpd_resp_send_err(req,
                                HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Ontvangst mislukt");

            return ESP_FAIL;
        }

        received += n;
    }

    return ESP_OK;
}

static esp_err_t api_wifi_scan(httpd_req_t *req)
{
    wifi_scan_config_t scan_cfg = {};
    scan_cfg.show_hidden = false;

    ESP_LOGI(TAG, "WiFi scan gestart");

    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);

    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "WiFi scan mislukt: %s",
                 esp_err_to_name(err));

        httpd_resp_set_status(req, "500 Internal Server Error");
        send_json(req,
                  "{\"error\":\"WiFi scan kon niet worden uitgevoerd\"}");

        return ESP_OK;
    }

    uint16_t count = 0;
    err = esp_wifi_scan_get_ap_num(&count);

    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        send_json(req,
                  "{\"error\":\"Scanresultaten niet beschikbaar\"}");

        return ESP_OK;
    }

    if (count == 0) {
        send_json(req, "[]");
        return ESP_OK;
    }

    std::vector<wifi_ap_record_t> records(count);

    uint16_t actual = count;
    err = esp_wifi_scan_get_ap_records(&actual, records.data());

    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        send_json(req,
                  "{\"error\":\"Scanresultaten konden niet worden gelezen\"}");

        return ESP_OK;
    }

    std::sort(records.begin(),
              records.begin() + actual,
              [](const wifi_ap_record_t &a,
                 const wifi_ap_record_t &b) {
                    return a.rssi > b.rssi;
              });

    std::string json = "[";
    std::vector<std::string> seen;

    bool first = true;

    for (uint16_t i = 0; i < actual; ++i) {
        const std::string ssid(
            reinterpret_cast<const char *>(records[i].ssid));

        if (ssid.empty()) continue;

        if (std::find(seen.begin(),
                      seen.end(),
                      ssid) != seen.end()) {
            continue;
        }

        seen.push_back(ssid);

        if (!first) json += ",";
        first = false;

        json += "{\"ssid\":\"";
        json += json_escape(ssid);
        json += "\",\"rssi\":";
        json += std::to_string(records[i].rssi);
        json += ",\"encrypted\":";
        json += (records[i].authmode == WIFI_AUTH_OPEN)
                    ? "false"
                    : "true";
        json += "}";
    }

    json += "]";

    ESP_LOGI(TAG,
             "WiFi scan klaar: %u netwerk(en)",
             static_cast<unsigned>(seen.size()));

    send_json(req, json);
    return ESP_OK;
}


static bool parse_ipv4(const std::string &text, esp_ip4_addr_t *out)
{
    if (!out) return false;

    unsigned a, b, c, d;
    char extra;

    if (std::sscanf(text.c_str(),
                    "%u.%u.%u.%u%c",
                    &a, &b, &c, &d, &extra) != 4) {
        return false;
    }

    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return false;
    }

    out->addr =
        ESP_IP4TOADDR(a, b, c, d);

    return true;
}

static esp_err_t configure_sta_ip(const std::string &mode,
                                  const std::string &ip,
                                  const std::string &gateway,
                                  const std::string &subnet,
                                  const std::string &dns1,
                                  const std::string &dns2)
{
    esp_netif_t *sta =
        esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    if (!sta) {
        ESP_LOGE(TAG, "WIFI_STA_DEF niet gevonden");
        return ESP_ERR_NOT_FOUND;
    }

    if (mode.empty() || mode == "dhcp") {
        esp_err_t err = esp_netif_dhcpc_start(sta);

        if (err != ESP_OK &&
            err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {

            ESP_LOGE(TAG,
                     "DHCP client starten mislukt: %s",
                     esp_err_to_name(err));
            return err;
        }

        ESP_LOGI(TAG, "WiFi IP-modus: DHCP");
        return ESP_OK;
    }

    if (mode != "static") {
        return ESP_ERR_INVALID_ARG;
    }

    esp_netif_ip_info_t info = {};

    if (!parse_ipv4(ip, &info.ip) ||
        !parse_ipv4(gateway, &info.gw) ||
        !parse_ipv4(subnet, &info.netmask)) {

        ESP_LOGE(TAG, "Ongeldige vaste IPv4-configuratie");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = esp_netif_dhcpc_stop(sta);

    if (err != ESP_OK &&
        err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {

        ESP_LOGE(TAG,
                 "DHCP client stoppen mislukt: %s",
                 esp_err_to_name(err));
        return err;
    }

    err = esp_netif_set_ip_info(sta, &info);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Vast IP instellen mislukt: %s",
                 esp_err_to_name(err));
        return err;
    }

    if (!dns1.empty()) {
        esp_netif_dns_info_t dns = {};

        if (!parse_ipv4(dns1, &dns.ip.u_addr.ip4)) {
            return ESP_ERR_INVALID_ARG;
        }

        dns.ip.type = ESP_IPADDR_TYPE_V4;

        err = esp_netif_set_dns_info(
            sta,
            ESP_NETIF_DNS_MAIN,
            &dns);

        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                     "Primaire DNS instellen mislukt: %s",
                     esp_err_to_name(err));
            return err;
        }
    }

    if (!dns2.empty()) {
        esp_netif_dns_info_t dns = {};

        if (!parse_ipv4(dns2, &dns.ip.u_addr.ip4)) {
            return ESP_ERR_INVALID_ARG;
        }

        dns.ip.type = ESP_IPADDR_TYPE_V4;

        err = esp_netif_set_dns_info(
            sta,
            ESP_NETIF_DNS_BACKUP,
            &dns);

        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                     "Secundaire DNS instellen mislukt: %s",
                     esp_err_to_name(err));
            return err;
        }
    }

    ESP_LOGI(TAG,
             "WiFi IP-modus: vast IP %s, gateway %s",
             ip.c_str(),
             gateway.c_str());

    return ESP_OK;
}



static esp_err_t save_network_config_nvs(
    const std::string &ssid,
    const std::string &mode,
    const std::string &ip,
    const std::string &gateway,
    const std::string &subnet,
    const std::string &dns1,
    const std::string &dns2)
{
    nvs_handle_t handle = 0;

    esp_err_t err = nvs_open(
        "spa_net",
        NVS_READWRITE,
        &handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "NVS spa_net openen mislukt: %s",
                 esp_err_to_name(err));
        return err;
    }

    const uint8_t mode_value =
        (mode == "static") ? 1 : 0;

    err = nvs_set_str(handle, "ssid", ssid.c_str());

    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "mode", mode_value);
    }

    esp_ip4_addr_t value = {};

    if (err == ESP_OK && mode_value == 1) {
        if (!parse_ipv4(ip, &value)) {
            err = ESP_ERR_INVALID_ARG;
        } else {
            err = nvs_set_u32(handle, "ip", value.addr);
        }

        if (err == ESP_OK) {
            if (!parse_ipv4(gateway, &value)) {
                err = ESP_ERR_INVALID_ARG;
            } else {
                err = nvs_set_u32(handle, "gw", value.addr);
            }
        }

        if (err == ESP_OK) {
            if (!parse_ipv4(subnet, &value)) {
                err = ESP_ERR_INVALID_ARG;
            } else {
                err = nvs_set_u32(handle, "mask", value.addr);
            }
        }

        if (err == ESP_OK) {
            value.addr = 0;

            if (!dns1.empty() &&
                !parse_ipv4(dns1, &value)) {
                err = ESP_ERR_INVALID_ARG;
            } else {
                err = nvs_set_u32(handle, "dns1", value.addr);
            }
        }

        if (err == ESP_OK) {
            value.addr = 0;

            if (!dns2.empty() &&
                !parse_ipv4(dns2, &value)) {
                err = ESP_ERR_INVALID_ARG;
            } else {
                err = nvs_set_u32(handle, "dns2", value.addr);
            }
        }
    }

    if (err == ESP_OK && mode_value == 0) {
        nvs_erase_key(handle, "ip");
        nvs_erase_key(handle, "gw");
        nvs_erase_key(handle, "mask");
        nvs_erase_key(handle, "dns1");
        nvs_erase_key(handle, "dns2");
    }

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG,
                 "Netwerkconfig opgeslagen in NVS: %s voor '%s'",
                 mode_value ? "vast IP" : "DHCP",
                 ssid.c_str());
    } else {
        ESP_LOGE(TAG,
                 "Netwerkconfig opslaan in NVS mislukt: %s",
                 esp_err_to_name(err));
    }

    return err;
}

struct WifiIpReconfigureRequest {
    std::string mode;
    std::string ip;
    std::string gateway;
    std::string subnet;
    std::string dns1;
    std::string dns2;
};

static void wifi_ip_reconfigure_task(void *arg)
{
    auto *request =
        static_cast<WifiIpReconfigureRequest *>(arg);

    // Geef HTTP-server eerst tijd om het antwoord
    // volledig naar de browser te sturen.
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG,
             "WiFi IP-configuratie nu toepassen (%s)",
             request->mode.c_str());

    // Zelfde WiFi-netwerk behouden; alleen netwerkstack
    // kort opnieuw verbinden zodat DHCP/static schoon ingaat.
    esp_err_t err = esp_wifi_disconnect();

    if (err != ESP_OK &&
        err != ESP_ERR_WIFI_NOT_CONNECT) {

        ESP_LOGW(TAG,
                 "WiFi disconnect voor IP-wissel gaf: %s",
                 esp_err_to_name(err));
    }

    vTaskDelay(pdMS_TO_TICKS(150));

    err = configure_sta_ip(
        request->mode,
        request->ip,
        request->gateway,
        request->subnet,
        request->dns1,
        request->dns2);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Asynchrone IP-configuratie mislukt: %s",
                 esp_err_to_name(err));

        delete request;
        vTaskDelete(nullptr);
        return;
    }

    err = esp_wifi_connect();

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "WiFi reconnect na IP-wissel mislukt: %s",
                 esp_err_to_name(err));
    }

    delete request;
    vTaskDelete(nullptr);
}

static esp_err_t api_wifi_connect(httpd_req_t *req)
{
    std::string body;

    if (read_request_body(req, body) != ESP_OK) {
        return ESP_FAIL;
    }

    const std::string ssid =
        form_value(body, "ssid");

    const std::string password =
        form_value(body, "password");

    const std::string mode =
        form_value(body, "mode");

    const std::string ip =
        form_value(body, "ip");

    const std::string gateway =
        form_value(body, "gateway");

    const std::string subnet =
        form_value(body, "subnet");

    const std::string dns1 =
        form_value(body, "dns1");

    const std::string dns2 =
        form_value(body, "dns2");

    if (ssid.empty() || ssid.size() > 32) {
        httpd_resp_set_status(req, "400 Bad Request");
        send_json(req,
                  "{\"ok\":false,\"error\":\"Ongeldige WiFi-netwerknaam\"}");
        return ESP_OK;
    }

    if (password.size() > 64) {
        httpd_resp_set_status(req, "400 Bad Request");
        send_json(req,
                  "{\"ok\":false,\"error\":\"WiFi-wachtwoord is te lang\"}");
        return ESP_OK;
    }

    if (mode != "dhcp" && mode != "static") {
        httpd_resp_set_status(req, "400 Bad Request");
        send_json(req,
                  "{\"ok\":false,\"error\":\"Ongeldige IP-modus\"}");
        return ESP_OK;
    }

    if (mode == "static") {
        esp_ip4_addr_t test = {};

        if (!parse_ipv4(ip, &test) ||
            !parse_ipv4(gateway, &test) ||
            !parse_ipv4(subnet, &test) ||
            (!dns1.empty() && !parse_ipv4(dns1, &test)) ||
            (!dns2.empty() && !parse_ipv4(dns2, &test))) {

            httpd_resp_set_status(req, "400 Bad Request");
            send_json(req,
                      "{\"ok\":false,\"error\":\"Controleer de vaste IP-instellingen\"}");
            return ESP_OK;
        }
    }

    // Eerst bepalen of we al met exact dit SSID verbonden zijn.
    // In dat geval hoeven SSID en wachtwoord helemaal niet opnieuw
    // ingesteld te worden wanneer alleen DHCP/static verandert.
    wifi_ap_record_t current_ap = {};
    bool connected_same_ssid = false;

    if (esp_wifi_sta_get_ap_info(&current_ap) == ESP_OK) {
        const std::string connected_ssid(
            reinterpret_cast<const char *>(current_ap.ssid));

        connected_same_ssid =
            connected_ssid == ssid;
    }

    if (connected_same_ssid &&
        password.empty()) {

        esp_err_t save_err =
            save_network_config_nvs(
                ssid,
                mode,
                ip,
                gateway,
                subnet,
                dns1,
                dns2);

        if (save_err != ESP_OK) {
            httpd_resp_set_status(
                req,
                "500 Internal Server Error");

            send_json(
                req,
                "{\"ok\":false,\"error\":\"Netwerkconfig kon niet worden opgeslagen\"}");

            return ESP_OK;
        }

        auto *request =
            new WifiIpReconfigureRequest{
                mode,
                ip,
                gateway,
                subnet,
                dns1,
                dns2
            };

        BaseType_t task_ok = xTaskCreate(
            wifi_ip_reconfigure_task,
            "wifi_ip_change",
            4096,
            request,
            5,
            nullptr);

        if (task_ok != pdPASS) {
            delete request;

            httpd_resp_set_status(
                req,
                "500 Internal Server Error");

            send_json(
                req,
                "{\"ok\":false,\"error\":\"IP-wijziging kon niet worden gestart\"}");

            return ESP_OK;
        }

        ESP_LOGI(TAG,
                 "Zelfde WiFi-netwerk: wachtwoord behouden, IP-wijziging uitgesteld");

        send_json(
            req,
            "{\"ok\":true,\"state\":\"reconfiguring\"}");

        return ESP_OK;
    }

    wifi_config_t old_cfg = {};
    esp_wifi_get_config(WIFI_IF_STA, &old_cfg);

    const std::string old_ssid(
        reinterpret_cast<const char *>(old_cfg.sta.ssid));

    std::string effective_password = password;

    // Bij hetzelfde netwerk mag het wachtwoord leeg blijven.
    // Dan behouden we het reeds opgeslagen wachtwoord.
    if (effective_password.empty() &&
        ssid == old_ssid) {

        effective_password =
            reinterpret_cast<const char *>(
                old_cfg.sta.password);
    }

    wifi_config_t cfg = {};

    std::snprintf(
        reinterpret_cast<char *>(cfg.sta.ssid),
        sizeof(cfg.sta.ssid),
        "%s",
        ssid.c_str());

    std::snprintf(
        reinterpret_cast<char *>(cfg.sta.password),
        sizeof(cfg.sta.password),
        "%s",
        effective_password.c_str());

    cfg.sta.threshold.authmode =
        effective_password.empty()
            ? WIFI_AUTH_OPEN
            : WIFI_AUTH_WPA2_PSK;

    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.pmf_cfg.required = false;

    ESP_LOGI(TAG,
             "Nieuwe WiFi configuratie ontvangen voor SSID '%s'",
             ssid.c_str());

    // Nieuwe STA-config eerst opslaan.
    // Als we eerst disconnecten kan Matter direct een reconnect starten,
    // waardoor esp_wifi_set_config() ESP_ERR_WIFI_STATE teruggeeft.
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);

    if (err == ESP_OK &&
        g_matter_runtime_active_cb &&
        g_matter_runtime_active_cb() &&
        g_matter_wifi_sync_cb) {

        const esp_err_t matter_wifi_err =
            g_matter_wifi_sync_cb(
                ssid.c_str(),
                effective_password.c_str());

        if (matter_wifi_err != ESP_OK) {
            ESP_LOGE(TAG,
                     "WiFi-config naar Matter synchroniseren mislukt: %s",
                     esp_err_to_name(matter_wifi_err));
            err = matter_wifi_err;
        } else {
            ESP_LOGI(TAG,
                     "WiFi-config ook naar Matter gesynchroniseerd");
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Nieuwe WiFi configuratie opslaan mislukt: %s",
                 esp_err_to_name(err));

        httpd_resp_set_status(req,
                              "500 Internal Server Error");

        send_json(req,
                  "{\"ok\":false,\"error\":\"WiFi-instellingen konden niet worden opgeslagen\"}");

        return ESP_OK;
    }

    err = esp_wifi_disconnect();

    if (err != ESP_OK &&
        err != ESP_ERR_WIFI_NOT_CONNECT) {

        ESP_LOGW(TAG,
                 "WiFi disconnect vooraf gaf: %s",
                 esp_err_to_name(err));
    }

    err = configure_sta_ip(
        mode,
        ip,
        gateway,
        subnet,
        dns1,
        dns2);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "IP-configuratie toepassen mislukt: %s",
                 esp_err_to_name(err));

        httpd_resp_set_status(req,
                              "400 Bad Request");

        send_json(req,
                  "{\"ok\":false,\"error\":\"IP-instellingen konden niet worden toegepast\"}");

        return ESP_OK;
    }

    err = esp_wifi_connect();

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Verbinden starten mislukt: %s",
                 esp_err_to_name(err));

        httpd_resp_set_status(req,
                              "500 Internal Server Error");

        send_json(req,
                  "{\"ok\":false,\"error\":\"WiFi-verbinding kon niet worden gestart\"}");

        return ESP_OK;
    }

    send_json(req,
              "{\"ok\":true,\"state\":\"connecting\"}");

    return ESP_OK;
}

static esp_err_t api_wifi_forget(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Opgeslagen WiFi-instellingen wissen");

    wifi_config_t empty_cfg = {};

    esp_err_t err = esp_wifi_disconnect();

    if (err != ESP_OK &&
        err != ESP_ERR_WIFI_NOT_CONNECT) {

        ESP_LOGW(TAG,
                 "WiFi disconnect bij wissen: %s",
                 esp_err_to_name(err));
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &empty_cfg);

    if (err == ESP_OK &&
        g_matter_runtime_active_cb &&
        g_matter_runtime_active_cb() &&
        g_matter_wifi_sync_cb) {

        const esp_err_t matter_wifi_err =
            g_matter_wifi_sync_cb("", "");

        if (matter_wifi_err != ESP_OK) {
            ESP_LOGW(TAG,
                     "Matter WiFi-config wissen gaf: %s",
                     esp_err_to_name(matter_wifi_err));
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "WiFi configuratie wissen mislukt: %s",
                 esp_err_to_name(err));

        httpd_resp_set_status(req,
                              "500 Internal Server Error");

        send_json(req,
                  "{\"ok\":false,\"error\":\"WiFi-instellingen konden niet worden gewist\"}");

        return ESP_OK;
    }

    send_json(req,
              "{\"ok\":true}");

    return ESP_OK;
}

static const char *reset_reason_text()
{
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:
            return "Power-on";
        case ESP_RST_EXT:
            return "Externe reset";
        case ESP_RST_SW:
            return "Software reset";
        case ESP_RST_PANIC:
            return "Crash / panic";
        case ESP_RST_INT_WDT:
            return "Interrupt watchdog";
        case ESP_RST_TASK_WDT:
            return "Task watchdog";
        case ESP_RST_WDT:
            return "Watchdog";
        case ESP_RST_DEEPSLEEP:
            return "Deep sleep";
        case ESP_RST_BROWNOUT:
            return "Brownout";
        case ESP_RST_SDIO:
            return "SDIO reset";
        case ESP_RST_UNKNOWN:
        default:
            return "Onbekend";
    }
}


static esp_err_t api_wifi_status(httpd_req_t *req)
{
    esp_netif_t *sta =
        esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    esp_netif_t *ap =
        esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    wifi_config_t sta_cfg = {};
    wifi_config_t ap_cfg = {};

    esp_wifi_get_config(WIFI_IF_STA, &sta_cfg);

    char ssid[33] = {};
    std::snprintf(
        ssid,
        sizeof(ssid),
        "%s",
        reinterpret_cast<const char *>(sta_cfg.sta.ssid));

    wifi_ap_record_t ap_info = {};
    const bool connected =
        esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;

    int rssi = connected ? ap_info.rssi : 0;

    esp_netif_ip_info_t ip_info = {};
    if (sta) {
        esp_netif_get_ip_info(sta, &ip_info);
    }

    const char *hostname = "bestwayspa";

    if (sta) {
        const char *netif_hostname = nullptr;

        if (esp_netif_get_hostname(
                sta,
                &netif_hostname) == ESP_OK &&
            netif_hostname &&
            netif_hostname[0]) {

            hostname = netif_hostname;
        }
    }

    uint8_t mac[6] = {};
    esp_wifi_get_mac(WIFI_IF_STA, mac);

    wifi_mode_t wifi_mode = WIFI_MODE_NULL;
    esp_wifi_get_mode(&wifi_mode);

    const bool ap_active =
        wifi_mode == WIFI_MODE_AP ||
        wifi_mode == WIFI_MODE_APSTA;

    char ap_ssid[33] = {};
    std::snprintf(
        ap_ssid,
        sizeof(ap_ssid),
        "%s",
        "LazySpaSetup");

    if (ap_active &&
        esp_wifi_get_config(WIFI_IF_AP, &ap_cfg) == ESP_OK &&
        ap_cfg.ap.ssid[0] != 0) {

        std::snprintf(
            ap_ssid,
            sizeof(ap_ssid),
            "%s",
            reinterpret_cast<const char *>(ap_cfg.ap.ssid));
    }

    esp_netif_ip_info_t ap_ip = {};
    ap_ip.ip.addr = ESP_IP4TOADDR(192, 168, 4, 1);

    if (ap_active && ap) {
        esp_netif_get_ip_info(ap, &ap_ip);
    }

    esp_netif_dhcp_status_t dhcp_status =
        ESP_NETIF_DHCP_INIT;

    if (sta) {
        esp_netif_dhcpc_get_status(
            sta,
            &dhcp_status);
    }

    const char *ip_mode =
        dhcp_status == ESP_NETIF_DHCP_STARTED
            ? "dhcp"
            : "static";

    esp_netif_dns_info_t dns1 = {};
    esp_netif_dns_info_t dns2 = {};

    if (sta) {
        esp_netif_get_dns_info(
            sta,
            ESP_NETIF_DNS_MAIN,
            &dns1);

        esp_netif_get_dns_info(
            sta,
            ESP_NETIF_DNS_BACKUP,
            &dns2);
    }

    const std::string ssid_json =
        json_escape(ssid);

    const std::string hostname_json =
        json_escape(hostname);

    const std::string ap_ssid_json =
        json_escape(ap_ssid);

    const esp_app_desc_t *app_desc =
        esp_app_get_description();

    const char *firmware_version =
        (app_desc && app_desc->version[0])
            ? app_desc->version
            : "unknown";

    const esp_partition_t *running_partition =
        esp_ota_get_running_partition();

    const char *ota_partition =
        running_partition
            ? running_partition->label
            : "unknown";

    const char *reset_reason =
        reset_reason_text();

    // Grote JSON-buffer niet op de kleine HTTPD-task stack zetten.
    // Heap voorkomt stack overflow in snprintf.
    constexpr size_t json_buf_size = 1600;
    char *buf = static_cast<char *>(malloc(json_buf_size));

    if (!buf) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        send_json(req, "{\"ok\":false,\"error\":\"Onvoldoende geheugen\"}");
        return ESP_OK;
    }

    std::snprintf(
        buf,
        json_buf_size,

        "{"
        "\"connected\":%s,"
        "\"wifi\":%s,"
        "\"ssid\":\"%s\","
        "\"ip\":\"" IPSTR "\","
        "\"rssi\":%d,"
        "\"hostname\":\"%s\","
        "\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
        "\"ipMode\":\"%s\","
        "\"gateway\":\"" IPSTR "\","
        "\"subnet\":\"" IPSTR "\","
        "\"dns1\":\"" IPSTR "\","
        "\"dns2\":\"" IPSTR "\","
        "\"apMode\":%s,"
        "\"apActive\":%s,"
        "\"apSsid\":\"%s\","
        "\"apIp\":\"" IPSTR "\","
        "\"mode\":\"Matter/STA\","
        "\"firmware\":\"%s\","
        "\"chip\":\"ESP32-C6\","
        "\"otaPartition\":\"%s\","
        "\"webInterface\":\"embedded\","
        "\"resetReason\":\"%s\""
        "}",

        connected ? "true" : "false",
        connected ? "true" : "false",

        ssid_json.c_str(),

        IP2STR(&ip_info.ip),

        rssi,

        hostname_json.c_str(),

        mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5],

        ip_mode,

        IP2STR(&ip_info.gw),
        IP2STR(&ip_info.netmask),

        IP2STR(&dns1.ip.u_addr.ip4),
        IP2STR(&dns2.ip.u_addr.ip4),

        ap_active ? "true" : "false",
        ap_active ? "true" : "false",

        ap_ssid_json.c_str(),

        IP2STR(&ap_ip.ip),

        firmware_version,
        ota_partition,
        reset_reason);

    send_json(req, buf);
    free(buf);

    return ESP_OK;
}

static esp_err_t api_matter_status(httpd_req_t *req)
{
    char ip[24];
    int rssi = 0;
    bool wifi = false;
    get_network_info(ip, sizeof(ip), &rssi, &wifi);

    const bool enabled = web_matter_enabled();
    const bool runtime_active =
        g_matter_runtime_active_cb ? g_matter_runtime_active_cb() : false;
    const bool commissioned =
        (runtime_active && g_matter_commissioned_cb) ? g_matter_commissioned_cb() : false;

    char flags[180];
    std::snprintf(flags, sizeof(flags),
        "{\"enabled\":%s,\"runtimeActive\":%s,\"connected\":%s,\"commissioned\":%s",
        enabled ? "true" : "false", runtime_active ? "true" : "false",
        (runtime_active && wifi && commissioned) ? "true" : "false",
        commissioned ? "true" : "false");
    std::string details = g_matter_status_cb ? g_matter_status_cb(runtime_active) : "{}";
    std::string json = flags;
    if (details.size() > 2 && details.front() == '{' && details.back() == '}') {
        json += ',';
        json += details.substr(1);
    } else {
        json += '}';
    }
    send_json(req, json);
    return ESP_OK;
}

static void matter_setting_restart_task(void *)
{
    vTaskDelay(pdMS_TO_TICKS(750));
    ESP_LOGW(TAG, "Herstart voor gewijzigde Matter-instelling");
    esp_restart();
}

static esp_err_t api_matter_settings_post(httpd_req_t *req)
{
    std::string body;
    if (read_request_body(req, body, 128) != ESP_OK) {
        return ESP_FAIL;
    }

    // Reuse the existing route: no additional HTTP handler or polling.
    if (form_value(body, "action") == "openWindow") {
        if (!g_matter_runtime_active_cb || !g_matter_runtime_active_cb() || !g_matter_open_window_cb) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Matter is niet actief");
            return ESP_FAIL;
        }
        const esp_err_t err = g_matter_open_window_cb();
        if (err != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                err == ESP_ERR_INVALID_STATE ? "Er is al een koppelvenster actief" : "Koppelvenster openen mislukt");
            return ESP_FAIL;
        }
        send_json(req, "{\"ok\":true,\"windowOpening\":true}");
        return ESP_OK;
    }

    const std::string enabled_value = form_value(body, "enabled");
    if (enabled_value != "true" && enabled_value != "false" &&
        enabled_value != "1" && enabled_value != "0") {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "Ongeldige Matter-instelling");
        return ESP_FAIL;
    }

    const bool enabled = enabled_value == "true" || enabled_value == "1";
    const esp_err_t err = save_matter_enabled(enabled);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Matter instelling opslaan mislukt: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Matter-instelling opslaan mislukt");
        return err;
    }

    const bool runtime_active =
        g_matter_runtime_active_cb ? g_matter_runtime_active_cb() : false;
    const bool restart_needed = (enabled != runtime_active);

    ESP_LOGI(TAG, "Matter instelling opgeslagen: %s (runtime=%s, restart=%s)",
             enabled ? "AAN" : "UIT",
             runtime_active ? "ACTIEF" : "INACTIEF",
             restart_needed ? "JA" : "NEE");

    send_json(req, enabled
        ? (restart_needed
            ? "{\"ok\":true,\"enabled\":true,\"restartScheduled\":true}"
            : "{\"ok\":true,\"enabled\":true,\"restartScheduled\":false}")
        : (restart_needed
            ? "{\"ok\":true,\"enabled\":false,\"restartScheduled\":true}"
            : "{\"ok\":true,\"enabled\":false,\"restartScheduled\":false}"));

    if (restart_needed) {
        if (xTaskCreate(matter_setting_restart_task, "matter_cfg_restart", 4096, nullptr, 3, nullptr) != pdPASS) {
            ESP_LOGE(TAG, "Matter herstart-task kon niet worden gestart");
        }
    }
    return ESP_OK;
}

static void matter_reset_task(void *)
{
    vTaskDelay(pdMS_TO_TICKS(500));

    if (!g_matter_reset_cb) {
        ESP_LOGE(TAG, "Matter reset callback ontbreekt");
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGW(TAG, "Matter-koppeling wissen gestart");
    const esp_err_t err = g_matter_reset_cb();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Matter reset mislukt: %s", esp_err_to_name(err));
    }

    vTaskDelete(nullptr);
}

static esp_err_t api_matter_reset_post(httpd_req_t *req)
{
    if (!g_matter_reset_cb) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Matter reset niet beschikbaar");
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "Matter-koppeling wissen aangevraagd via webinterface");
    send_json(req, "{\"ok\":true,\"restarting\":true}");

    const BaseType_t ok = xTaskCreate(matter_reset_task, "matter_reset",
                                      4096, nullptr, 5, nullptr);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Matter reset task starten mislukt");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

struct LiveCommandResult {
    bool ok = false;
    httpd_err_code_t error_code = HTTPD_400_BAD_REQUEST;
    std::string error;
};

static LiveCommandResult execute_live_command(const std::string &cmd)
{
    LiveCommandResult result;

    ESP_LOGI(TAG, "Web -> %s", cmd.c_str());

    const auto family =
        bestway_bus.model_info().family;

    const bool hardware = bestway_bus.mode() == BestwayBus::Mode::Hardware;

    const bool target_command =
        cmd == "target:+" ||
        cmd == "target:-";

    /*
     * 4-wire volgens Spa Control v3.2.6:
     * - Heater en target zijn bestuurbaar wanneer Godmode actief is.
     * - Een zelfstandige 4-wire Power-actie is niet bewezen.
     */
    if (hardware && family == BestwayProtocolFamily::Uart4Wire &&
        cmd == "toggle:power") {

        ESP_LOGW(TAG, "Web Power geweigerd voor 4-wire");
        result.error_code = HTTPD_501_METHOD_NOT_IMPLEMENTED;
        result.error =
            "Power heeft voor 4-wire geen zelfstandige bewezen bediening";
        return result;
    }

    if (hardware && family == BestwayProtocolFamily::Uart4Wire &&
        !bestway_bus.godmode() &&
        (cmd == "toggle:heater" || target_command)) {

        ESP_LOGW(TAG,
                 "4-wire command vereist Godmode: %s",
                 cmd.c_str());
        result.error =
            "Schakel eerst Godmode in voor deze 4-wire bediening";
        return result;
    }

    if (cmd == "toggle:godmode") {
        if (family != BestwayProtocolFamily::Uart4Wire) {
            result.error =
                "Godmode is alleen beschikbaar voor 4-wire";
            return result;
        }

        bestway_bus.set_godmode(!bestway_bus.godmode());
        ESP_LOGI(TAG,
                 "Web -> 4-wire Godmode %s",
                 bestway_bus.godmode() ? "AAN" : "UIT");
        result.ok = true;
        return result;
    }

    if (cmd == "toggle:fullpower") {
        if (family != BestwayProtocolFamily::Uart4Wire) {
            result.error =
                "Full Power is alleen beschikbaar voor 4-wire";
            return result;
        }

        if (!bestway_bus.godmode()) {
            result.error =
                "Schakel eerst Godmode in voor Full Power";
            return result;
        }

        bestway_bus.set_godmode_full_power(
            !bestway_bus.godmode_full_power());

        ESP_LOGI(TAG,
                 "Web -> 4-wire Full Power %s",
                 bestway_bus.godmode_full_power() ? "AAN" : "UIT");
        result.ok = true;
        return result;
    }

    if (hardware && family == BestwayProtocolFamily::SixWireType2 &&
        cmd == "toggle:power") {

        ESP_LOGW(TAG, "Web Power geweigerd voor Type2");
        result.error_code = HTTPD_501_METHOD_NOT_IMPLEMENTED;
        result.error =
            "Power is voor Type2 niet betrouwbaar bestuurbaar";
        return result;
    }

    if (cmd == "toggle:power") bestway_bus.set_power(!spa.power);
    else if (cmd == "toggle:filter") bestway_bus.set_filter(!spa.filter);
    else if (cmd == "toggle:bubbles") bestway_bus.set_bubbles(!spa.bubbles);
    else if (cmd == "toggle:jets") bestway_bus.set_jets(!spa.jets);
    else if (cmd == "toggle:heater") bestway_bus.set_heater(!spa.heater);
    else if (cmd == "target:+") bestway_bus.step_target(true);
    else if (cmd == "target:-") bestway_bus.step_target(false);
    else if (cmd == "toggle:unit" || cmd == "press:unit") bestway_bus.set_unit(!spa.fahrenheit);
    else if (cmd == "press:lock") bestway_bus.set_lock(!spa.locked);
    else if (cmd == "press:timer") bestway_bus.press_timer();
    else {
        result.error = "Onbekende opdracht";
        return result;
    }

    result.ok = true;
    return result;
}

static esp_err_t api_command(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 96) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Ongeldige opdracht");
        return ESP_FAIL;
    }

    char body[97] = {};
    int received = 0;
    while (received < req->content_len) {
        const int n = httpd_req_recv(
            req,
            body + received,
            req->content_len - received);

        if (n <= 0) {
            httpd_resp_send_err(
                req,
                HTTPD_500_INTERNAL_SERVER_ERROR,
                "Ontvangst mislukt");
            return ESP_FAIL;
        }
        received += n;
    }
    body[received] = '\0';

    const LiveCommandResult result =
        execute_live_command(std::string(body));

    if (!result.ok) {
        httpd_resp_send_err(
            req,
            result.error_code,
            result.error.c_str());
        return ESP_FAIL;
    }

    // REST blijft als fallback beschikbaar; normale bediening gebruikt WS.
    send_json(req, status_json());
    return ESP_OK;
}

static esp_err_t ws_send_text(httpd_req_t *req, const std::string &text)
{
    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = reinterpret_cast<uint8_t *>(
        const_cast<char *>(text.data()));
    frame.len = text.size();
    return httpd_ws_send_frame(req, &frame);
}

static esp_err_t websocket_handler(httpd_req_t *req)
{
    // Eerste GET is alleen de WebSocket-upgrade/handshake.
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG,
                 "WebSocket client verbonden fd=%d",
                 httpd_req_to_sockfd(req));
        return ESP_OK;
    }

    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);
    if (err != ESP_OK) {
        return err;
    }

    if (frame.len == 0 || frame.len > 96) {
        return ws_send_text(
            req,
            "{\"wsError\":\"Ongeldige WebSocket-opdracht\"}");
    }

    uint8_t payload[97] = {};
    frame.payload = payload;

    err = httpd_ws_recv_frame(req, &frame, frame.len);
    if (err != ESP_OK) {
        return err;
    }

    if (frame.type != HTTPD_WS_TYPE_TEXT) {
        return ESP_OK;
    }

    payload[frame.len] = 0;
    const std::string cmd(
        reinterpret_cast<const char *>(payload),
        frame.len);

    if (cmd == "state:get") {
        return ws_send_text(req, status_json());
    }

    const LiveCommandResult result =
        execute_live_command(cmd);

    if (!result.ok) {
        std::string json = "{\"wsError\":\"";
        json += json_escape(result.error);
        json += "\"}";
        return ws_send_text(req, json);
    }

    // Direct antwoord zoals de ESP8266. De pompbevestigde state volgt later
    // automatisch via web_server_push_live_state().
    return ws_send_text(req, status_json());
}

void web_server_push_live_state()
{
    if (!g_server) {
        return;
    }

    int client_fds[8] = {};
    size_t client_count = sizeof(client_fds) / sizeof(client_fds[0]);

    if (httpd_get_client_list(
            g_server,
            &client_count,
            client_fds) != ESP_OK) {
        return;
    }

    bool have_websocket = false;
    for (size_t i = 0; i < client_count; ++i) {
        if (httpd_ws_get_fd_info(
                g_server,
                client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
            have_websocket = true;
            break;
        }
    }

    if (!have_websocket) {
        return;
    }

    const std::string json = status_json();
    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = reinterpret_cast<uint8_t *>(
        const_cast<char *>(json.data()));
    frame.len = json.size();

    for (size_t i = 0; i < client_count; ++i) {
        const int fd = client_fds[i];
        if (httpd_ws_get_fd_info(
                g_server,
                fd) != HTTPD_WS_CLIENT_WEBSOCKET) {
            continue;
        }

        const esp_err_t err =
            httpd_ws_send_frame_async(
                g_server,
                fd,
                &frame);

        if (err != ESP_OK) {
            ESP_LOGD(TAG,
                     "WebSocket push fd=%d mislukt: %s",
                     fd,
                     esp_err_to_name(err));
        }
    }
}

static void append_mqtt_publication_json(
    std::string &json,
    const char *name,
    const MqttPublicationConfig &item)
{
    json += ",\"publish";
    json += name;
    json += "\":";
    json += item.enabled ? "true" : "false";

    json += ",\"topic";
    json += name;
    json += "\":\"";
    json += json_escape(item.topic);
    json += "\"";
}

static std::string mqtt_config_json()
{
    const MqttConfig &cfg = mqtt_manager.config();

    std::string json = "{";

    json += "\"enabled\":";
    json += cfg.enabled ? "true" : "false";

    json += ",\"host\":\"";
    json += json_escape(cfg.host);
    json += "\"";

    json += ",\"port\":";
    json += std::to_string(cfg.port);

    json += ",\"username\":\"";
    json += json_escape(cfg.username);
    json += "\"";

    json += ",\"clientId\":\"";
    json += json_escape(cfg.client_id);
    json += "\"";

    json += ",\"baseTopic\":\"";
    json += json_escape(cfg.base_topic);
    json += "\"";

    json += ",\"homeAssistantDiscovery\":";
    json += cfg.home_assistant_discovery ? "true" : "false";

    append_mqtt_publication_json(json, "Temperature",   cfg.temperature);
    append_mqtt_publication_json(json, "Target",        cfg.target);
    append_mqtt_publication_json(json, "Power",         cfg.power);
    append_mqtt_publication_json(json, "Heater",        cfg.heater);
    append_mqtt_publication_json(json, "HeatingActive", cfg.heating_active);
    append_mqtt_publication_json(json, "Godmode",       cfg.godmode);
    append_mqtt_publication_json(json, "FullPower",     cfg.full_power);
    append_mqtt_publication_json(json, "Filter",        cfg.filter);
    append_mqtt_publication_json(json, "Bubbles",       cfg.bubbles);
    append_mqtt_publication_json(json, "Jets",          cfg.jets);
    append_mqtt_publication_json(json, "Locked",        cfg.locked);
    append_mqtt_publication_json(json, "Connected",     cfg.connected);
    append_mqtt_publication_json(json, "Ready",         cfg.ready);
    append_mqtt_publication_json(json, "Rssi",          cfg.rssi);
    append_mqtt_publication_json(json, "Heap",          cfg.heap);
    append_mqtt_publication_json(json, "Uptime",        cfg.uptime);
    append_mqtt_publication_json(json, "Firmware",      cfg.firmware);
    append_mqtt_publication_json(json, "Ip",            cfg.ip);
    append_mqtt_publication_json(json, "Json",          cfg.json);
    append_mqtt_publication_json(json, "Maintenance",   cfg.maintenance);
    append_mqtt_publication_json(json, "MaintenanceFilterReplace", cfg.maintenance_filter_replace);
    append_mqtt_publication_json(json, "MaintenanceFilterClean",   cfg.maintenance_filter_clean);
    append_mqtt_publication_json(json, "MaintenanceChlorine",      cfg.maintenance_chlorine);

    json += "}";

    return json;
}

static bool form_bool(
    const std::string &body,
    const char *key,
    bool default_value)
{
    const std::string value = form_value(body, key);

    if (value.empty()) {
        return default_value;
    }

    return value == "true" ||
           value == "1" ||
           value == "on";
}

static void load_mqtt_publication_form(
    const std::string &body,
    const char *name,
    MqttPublicationConfig &item)
{
    const std::string publish_key =
        std::string("publish") + name;

    const std::string topic_key =
        std::string("topic") + name;

    item.enabled =
        form_bool(
            body,
            publish_key.c_str(),
            item.enabled);

    const std::string topic =
        form_value(
            body,
            topic_key.c_str());

    if (!topic.empty()) {
        item.topic = topic;
    }
}

static esp_err_t api_mqtt_get(httpd_req_t *req)
{
    send_json(req, mqtt_config_json());
    return ESP_OK;
}

static esp_err_t api_mqtt_post(httpd_req_t *req)
{
    std::string body;

    if (read_request_body(req, body, 4096) != ESP_OK) {
        return ESP_FAIL;
    }

    MqttConfig &cfg = mqtt_manager.config();

    cfg.enabled =
        form_bool(
            body,
            "enabled",
            cfg.enabled);

    const std::string host =
        form_value(body, "host");

    const std::string port_text =
        form_value(body, "port");

    const std::string username =
        form_value(body, "username");

    const std::string password =
        form_value(body, "password");

    const std::string client_id =
        form_value(body, "clientId");

    const std::string base_topic =
        form_value(body, "baseTopic");

    cfg.host = host;
    cfg.username = username;

    // Leeg wachtwoord betekent:
    // bestaand opgeslagen wachtwoord behouden.
    if (!password.empty()) {
        cfg.password = password;
    }

    if (!client_id.empty()) {
        cfg.client_id = client_id;
    }

    if (!base_topic.empty()) {
        cfg.base_topic = base_topic;
    }

    cfg.home_assistant_discovery =
        form_bool(
            body,
            "homeAssistantDiscovery",
            cfg.home_assistant_discovery);

    if (!port_text.empty()) {
        char *end = nullptr;

        const long port =
            std::strtol(
                port_text.c_str(),
                &end,
                10);

        if (!end ||
            *end != '\0' ||
            port < 1 ||
            port > 65535) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Ongeldige MQTT-poort");

            return ESP_FAIL;
        }

        cfg.port =
            static_cast<uint16_t>(port);
    }

    load_mqtt_publication_form(body, "Temperature",   cfg.temperature);
    load_mqtt_publication_form(body, "Target",        cfg.target);
    load_mqtt_publication_form(body, "Power",         cfg.power);
    load_mqtt_publication_form(body, "Heater",        cfg.heater);
    load_mqtt_publication_form(body, "HeatingActive", cfg.heating_active);
    load_mqtt_publication_form(body, "Godmode",       cfg.godmode);
    load_mqtt_publication_form(body, "FullPower",     cfg.full_power);
    load_mqtt_publication_form(body, "Filter",        cfg.filter);
    load_mqtt_publication_form(body, "Bubbles",       cfg.bubbles);
    load_mqtt_publication_form(body, "Jets",          cfg.jets);
    load_mqtt_publication_form(body, "Locked",        cfg.locked);
    load_mqtt_publication_form(body, "Connected",     cfg.connected);
    load_mqtt_publication_form(body, "Ready",         cfg.ready);
    load_mqtt_publication_form(body, "Rssi",          cfg.rssi);
    load_mqtt_publication_form(body, "Heap",          cfg.heap);
    load_mqtt_publication_form(body, "Uptime",        cfg.uptime);
    load_mqtt_publication_form(body, "Firmware",      cfg.firmware);
    load_mqtt_publication_form(body, "Ip",            cfg.ip);
    load_mqtt_publication_form(body, "Json",          cfg.json);
    load_mqtt_publication_form(body, "Maintenance",   cfg.maintenance);
    load_mqtt_publication_form(body, "MaintenanceFilterReplace", cfg.maintenance_filter_replace);
    load_mqtt_publication_form(body, "MaintenanceFilterClean",   cfg.maintenance_filter_clean);
    load_mqtt_publication_form(body, "MaintenanceChlorine",      cfg.maintenance_chlorine);

    if (!mqtt_manager.save()) {
        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "MQTT-instellingen opslaan mislukt");

        return ESP_FAIL;
    }

    if (!mqtt_manager.start()) {
        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "MQTT-client opnieuw starten mislukt");

        return ESP_FAIL;
    }

    ESP_LOGI(
        TAG,
        "MQTT instellingen opgeslagen en direct toegepast: enabled=%s host=%s port=%u",
        cfg.enabled ? "true" : "false",
        cfg.host.c_str(),
        static_cast<unsigned>(cfg.port));

    send_json(req, mqtt_config_json());

    return ESP_OK;
}



/*
 * ============================================================
 * Energie
 * ============================================================
 *
 * Gebaseerd op de werkende Spa Control-logica:
 *
 * - Heater telt alleen wanneer werkelijk verwarmen actief is.
 * - Filter, bubbles en jets tellen hun actieve tijd.
 * - Vermogen per functie is configureerbaar.
 * - Kosten = totaal kWh * prijs per kWh.
 * - Tellers en instellingen blijven via NVS bewaard.
 * - NVS-save maximaal iedere 15 minuten tijdens normaal bedrijf.
 */

struct EnergyState {
    uint64_t heater_ms = 0;
    uint64_t filter_ms = 0;
    uint64_t bubbles_ms = 0;
    uint64_t jets_ms = 0;

    uint32_t heater_watts = 2200;
    uint32_t filter_watts = 60;
    uint32_t bubbles_watts = 800;
    uint32_t jets_watts = 800;

    double price_per_kwh = 0.30;
    std::string currency = "€";
};

static EnergyState g_energy;
static SemaphoreHandle_t g_energy_mutex = nullptr;
static TaskHandle_t g_energy_task = nullptr;

static constexpr uint64_t ENERGY_SAVE_INTERVAL_MS =
    15ULL * 60ULL * 1000ULL;

static void energy_lock()
{
    if (g_energy_mutex) {
        xSemaphoreTake(g_energy_mutex, portMAX_DELAY);
    }
}

static void energy_unlock()
{
    if (g_energy_mutex) {
        xSemaphoreGive(g_energy_mutex);
    }
}

static void energy_save()
{
    EnergyState snapshot;

    energy_lock();
    snapshot = g_energy;
    energy_unlock();

    nvs_handle_t nvs = 0;

    if (nvs_open("spa_energy", NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGW(TAG, "Energie NVS openen mislukt");
        return;
    }

    esp_err_t err = ESP_OK;

    if (err == ESP_OK) err = nvs_set_u64(nvs, "heater_ms", snapshot.heater_ms);
    if (err == ESP_OK) err = nvs_set_u64(nvs, "filter_ms", snapshot.filter_ms);
    if (err == ESP_OK) err = nvs_set_u64(nvs, "bubble_ms", snapshot.bubbles_ms);
    if (err == ESP_OK) err = nvs_set_u64(nvs, "jets_ms", snapshot.jets_ms);

    if (err == ESP_OK) err = nvs_set_u32(nvs, "heater_w", snapshot.heater_watts);
    if (err == ESP_OK) err = nvs_set_u32(nvs, "filter_w", snapshot.filter_watts);
    if (err == ESP_OK) err = nvs_set_u32(nvs, "bubble_w", snapshot.bubbles_watts);
    if (err == ESP_OK) err = nvs_set_u32(nvs, "jets_w", snapshot.jets_watts);

    /*
     * Double en valuta als blobs bewaren, zodat we geen afronding
     * aan de instellingen toevoegen.
     */
    if (err == ESP_OK) {
        err = nvs_set_blob(
            nvs,
            "price",
            &snapshot.price_per_kwh,
            sizeof(snapshot.price_per_kwh));
    }

    if (err == ESP_OK) {
        err = nvs_set_str(
            nvs,
            "currency",
            snapshot.currency.c_str());
    }

    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Energie opslaan mislukt: %s",
            esp_err_to_name(err));
    }
}

static void energy_load()
{
    EnergyState loaded;

    nvs_handle_t nvs = 0;

    if (nvs_open("spa_energy", NVS_READONLY, &nvs) != ESP_OK) {
        energy_lock();
        g_energy = loaded;
        energy_unlock();
        return;
    }

    nvs_get_u64(nvs, "heater_ms", &loaded.heater_ms);
    nvs_get_u64(nvs, "filter_ms", &loaded.filter_ms);
    nvs_get_u64(nvs, "bubble_ms", &loaded.bubbles_ms);
    nvs_get_u64(nvs, "jets_ms", &loaded.jets_ms);

    nvs_get_u32(nvs, "heater_w", &loaded.heater_watts);
    nvs_get_u32(nvs, "filter_w", &loaded.filter_watts);
    nvs_get_u32(nvs, "bubble_w", &loaded.bubbles_watts);
    nvs_get_u32(nvs, "jets_w", &loaded.jets_watts);

    size_t price_size = sizeof(loaded.price_per_kwh);

    if (nvs_get_blob(
            nvs,
            "price",
            &loaded.price_per_kwh,
            &price_size) != ESP_OK ||
        price_size != sizeof(loaded.price_per_kwh)) {

        loaded.price_per_kwh = 0.30;
    }

    size_t currency_size = 0;

    if (nvs_get_str(
            nvs,
            "currency",
            nullptr,
            &currency_size) == ESP_OK &&
        currency_size > 0 &&
        currency_size <= 16) {

        std::string currency(currency_size, '\0');

        if (nvs_get_str(
                nvs,
                "currency",
                currency.data(),
                &currency_size) == ESP_OK) {

            if (!currency.empty() &&
                currency.back() == '\0') {
                currency.pop_back();
            }

            if (!currency.empty()) {
                loaded.currency = currency;
            }
        }
    }

    nvs_close(nvs);

    energy_lock();
    g_energy = loaded;
    energy_unlock();

    ESP_LOGI(
        TAG,
        "Energie geladen: heater=%.2f h filter=%.2f h bubbles=%.2f h jets=%.2f h",
        loaded.heater_ms / 3600000.0,
        loaded.filter_ms / 3600000.0,
        loaded.bubbles_ms / 3600000.0,
        loaded.jets_ms / 3600000.0);
}

static void energy_task_fn(void *)
{
    uint64_t previous_ms =
        static_cast<uint64_t>(
            esp_timer_get_time() / 1000ULL);

    uint64_t last_save_ms = previous_ms;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        const uint64_t now_ms =
            static_cast<uint64_t>(
                esp_timer_get_time() / 1000ULL);

        uint64_t elapsed_ms =
            now_ms >= previous_ms
                ? now_ms - previous_ms
                : 0;

        previous_ms = now_ms;

        /*
         * Een onverwachte task-pauze mag nooit ineens minuten/uren
         * energie bijboeken.
         */
        if (elapsed_ms > 5000) {
            elapsed_ms = 5000;
        }

        if (spa.connected &&
            spa.data_valid &&
            elapsed_ms > 0) {

            energy_lock();

            if (spa.heater_active) {
                g_energy.heater_ms += elapsed_ms;
            }

            if (spa.filter) {
                g_energy.filter_ms += elapsed_ms;
            }

            if (spa.bubbles) {
                g_energy.bubbles_ms += elapsed_ms;
            }

            if (spa.jets) {
                g_energy.jets_ms += elapsed_ms;
            }

            energy_unlock();
        }

        if ((now_ms - last_save_ms) >=
            ENERGY_SAVE_INTERVAL_MS) {

            energy_save();
            last_save_ms = now_ms;
        }
    }
}

static std::string energy_json()
{
    EnergyState e;

    energy_lock();
    e = g_energy;
    energy_unlock();

    const double heater_hours =
        e.heater_ms / 3600000.0;

    const double filter_hours =
        e.filter_ms / 3600000.0;

    const double bubbles_hours =
        e.bubbles_ms / 3600000.0;

    const double jets_hours =
        e.jets_ms / 3600000.0;

    const double heater_kwh =
        heater_hours *
        static_cast<double>(e.heater_watts) / 1000.0;

    const double filter_kwh =
        filter_hours *
        static_cast<double>(e.filter_watts) / 1000.0;

    const double bubbles_kwh =
        bubbles_hours *
        static_cast<double>(e.bubbles_watts) / 1000.0;

    const double jets_kwh =
        jets_hours *
        static_cast<double>(e.jets_watts) / 1000.0;

    const double total_kwh =
        heater_kwh +
        filter_kwh +
        bubbles_kwh +
        jets_kwh;

    const double estimated_cost =
        total_kwh * e.price_per_kwh;

    char numbers[1024];

    std::snprintf(
        numbers,
        sizeof(numbers),
        "{"
        "\"heaterHours\":%.6f,"
        "\"filterHours\":%.6f,"
        "\"bubblesHours\":%.6f,"
        "\"jetsHours\":%.6f,"
        "\"heaterKwh\":%.6f,"
        "\"filterKwh\":%.6f,"
        "\"bubblesKwh\":%.6f,"
        "\"jetsKwh\":%.6f,"
        "\"totalKwh\":%.6f,"
        "\"estimatedCost\":%.6f,"
        "\"heaterWatts\":%u,"
        "\"filterWatts\":%u,"
        "\"bubblesWatts\":%u,"
        "\"jetsWatts\":%u,"
        "\"pricePerKwh\":%.6f,"
        "\"currency\":\"",
        heater_hours,
        filter_hours,
        bubbles_hours,
        jets_hours,
        heater_kwh,
        filter_kwh,
        bubbles_kwh,
        jets_kwh,
        total_kwh,
        estimated_cost,
        static_cast<unsigned>(e.heater_watts),
        static_cast<unsigned>(e.filter_watts),
        static_cast<unsigned>(e.bubbles_watts),
        static_cast<unsigned>(e.jets_watts),
        e.price_per_kwh);

    std::string json = numbers;
    json += json_escape(e.currency);
    json += "\"}";

    return json;
}

static bool parse_positive_u32(
    const std::string &text,
    uint32_t &value)
{
    if (text.empty()) {
        return false;
    }

    char *end = nullptr;

    const unsigned long parsed =
        std::strtoul(
            text.c_str(),
            &end,
            10);

    if (!end ||
        *end != '\0' ||
        parsed > 100000UL) {
        return false;
    }

    value = static_cast<uint32_t>(parsed);
    return true;
}

static bool parse_nonnegative_double(
    const std::string &text,
    double &value)
{
    if (text.empty()) {
        return false;
    }

    char *end = nullptr;

    const double parsed =
        std::strtod(
            text.c_str(),
            &end);

    if (!end ||
        *end != '\0' ||
        parsed < 0.0 ||
        parsed > 1000000.0) {
        return false;
    }

    value = parsed;
    return true;
}

static esp_err_t api_energy_get(httpd_req_t *req)
{
    send_json(req, energy_json());
    return ESP_OK;
}

static esp_err_t api_energy_post(httpd_req_t *req)
{
    std::string body;

    if (read_request_body(req, body, 4096) != ESP_OK) {
        return ESP_FAIL;
    }

    EnergyState updated;

    energy_lock();
    updated = g_energy;
    energy_unlock();

    uint32_t value_u32 = 0;
    double value_double = 0.0;

    const std::string heater_watts =
        form_value(body, "heaterWatts");

    const std::string filter_watts =
        form_value(body, "filterWatts");

    const std::string bubbles_watts =
        form_value(body, "bubblesWatts");

    const std::string jets_watts =
        form_value(body, "jetsWatts");

    const std::string price =
        form_value(body, "pricePerKwh");

    const std::string currency =
        form_value(body, "currency");

    if (!heater_watts.empty()) {
        if (!parse_positive_u32(
                heater_watts,
                value_u32)) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Ongeldig heatervermogen");
            return ESP_FAIL;
        }

        updated.heater_watts = value_u32;
    }

    if (!filter_watts.empty()) {
        if (!parse_positive_u32(
                filter_watts,
                value_u32)) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Ongeldig filtervermogen");
            return ESP_FAIL;
        }

        updated.filter_watts = value_u32;
    }

    if (!bubbles_watts.empty()) {
        if (!parse_positive_u32(
                bubbles_watts,
                value_u32)) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Ongeldig bubblesvermogen");
            return ESP_FAIL;
        }

        updated.bubbles_watts = value_u32;
    }

    if (!jets_watts.empty()) {
        if (!parse_positive_u32(
                jets_watts,
                value_u32)) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Ongeldig jetsvermogen");
            return ESP_FAIL;
        }

        updated.jets_watts = value_u32;
    }

    if (!price.empty()) {
        if (!parse_nonnegative_double(
                price,
                value_double)) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Ongeldige energieprijs");
            return ESP_FAIL;
        }

        updated.price_per_kwh = value_double;
    }

    if (!currency.empty()) {
        if (currency.size() > 8) {
            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Valuta is te lang");
            return ESP_FAIL;
        }

        updated.currency = currency;
    }

    /*
     * Backup restore:
     * wanneer draaiuren aanwezig zijn, herstellen we ze ook.
     * Bij normaal opslaan vanuit de Energie-pagina ontbreken deze
     * velden en blijven de bestaande tellers onaangeroerd.
     */
    const struct {
        const char *key;
        uint64_t *destination;
    } counters[] = {
        {"heaterHours",  &updated.heater_ms},
        {"filterHours",  &updated.filter_ms},
        {"bubblesHours", &updated.bubbles_ms},
        {"jetsHours",    &updated.jets_ms},
    };

    for (const auto &counter : counters) {
        const std::string text =
            form_value(body, counter.key);

        if (text.empty()) {
            continue;
        }

        double hours = 0.0;

        if (!parse_nonnegative_double(
                text,
                hours)) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Ongeldige energieteller");
            return ESP_FAIL;
        }

        *counter.destination =
            static_cast<uint64_t>(
                hours * 3600000.0 + 0.5);
    }

    energy_lock();
    g_energy = updated;
    energy_unlock();

    energy_save();

    ESP_LOGI(
        TAG,
        "Energie-instellingen/tellers opgeslagen");

    send_json(req, energy_json());
    return ESP_OK;
}

static esp_err_t api_energy_delete(httpd_req_t *req)
{
    energy_lock();

    g_energy.heater_ms = 0;
    g_energy.filter_ms = 0;
    g_energy.bubbles_ms = 0;
    g_energy.jets_ms = 0;

    energy_unlock();

    energy_save();

    ESP_LOGI(TAG, "Energietellers gereset");

    send_json(
        req,
        "{\"ok\":true,\"cleared\":\"energy\"}");

    return ESP_OK;
}



/*
 * ============================================================
 * Onderhoud
 * ============================================================
 *
 * Houdt bij:
 * - filter vervangen
 * - filter reinigen
 * - chloor toevoegen
 *
 * Instellingen en laatste uitvoerdatum worden direct in NVS
 * bewaard. De webinterface berekent zelf niets; alle status,
 * datums en resterende dagen komen uit deze backend.
 */

struct MaintenanceItem {
    bool enabled = true;
    uint16_t interval_days = 1;
    int64_t last_done = 0;
};

struct MaintenanceState {
    MaintenanceItem filter_replace {true, 30, 0};
    MaintenanceItem filter_clean   {true, 7,  0};
    MaintenanceItem chlorine       {true, 3,  0};
};

static MaintenanceState g_maintenance;
static SemaphoreHandle_t g_maintenance_mutex = nullptr;

static void maintenance_lock()
{
    if (g_maintenance_mutex) {
        xSemaphoreTake(g_maintenance_mutex, portMAX_DELAY);
    }
}

static void maintenance_unlock()
{
    if (g_maintenance_mutex) {
        xSemaphoreGive(g_maintenance_mutex);
    }
}

static void maintenance_save()
{
    MaintenanceState state;

    maintenance_lock();
    state = g_maintenance;
    maintenance_unlock();

    nvs_handle_t nvs = 0;

    if (nvs_open("spa_maint", NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGW(TAG, "Onderhoud NVS openen mislukt");
        return;
    }

    esp_err_t err = ESP_OK;

    if (err == ESP_OK)
        err = nvs_set_u8(nvs, "fr_en",
                         state.filter_replace.enabled ? 1 : 0);

    if (err == ESP_OK)
        err = nvs_set_u16(nvs, "fr_days",
                          state.filter_replace.interval_days);

    if (err == ESP_OK)
        err = nvs_set_i64(nvs, "fr_last",
                          state.filter_replace.last_done);

    if (err == ESP_OK)
        err = nvs_set_u8(nvs, "fc_en",
                         state.filter_clean.enabled ? 1 : 0);

    if (err == ESP_OK)
        err = nvs_set_u16(nvs, "fc_days",
                          state.filter_clean.interval_days);

    if (err == ESP_OK)
        err = nvs_set_i64(nvs, "fc_last",
                          state.filter_clean.last_done);

    if (err == ESP_OK)
        err = nvs_set_u8(nvs, "cl_en",
                         state.chlorine.enabled ? 1 : 0);

    if (err == ESP_OK)
        err = nvs_set_u16(nvs, "cl_days",
                          state.chlorine.interval_days);

    if (err == ESP_OK)
        err = nvs_set_i64(nvs, "cl_last",
                          state.chlorine.last_done);

    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Onderhoud opslaan mislukt: %s",
            esp_err_to_name(err));
    }
}

static void maintenance_load()
{
    MaintenanceState loaded;

    nvs_handle_t nvs = 0;

    if (nvs_open("spa_maint", NVS_READONLY, &nvs) != ESP_OK) {
        maintenance_lock();
        g_maintenance = loaded;
        maintenance_unlock();
        return;
    }

    uint8_t enabled = 0;
    uint16_t days = 0;
    int64_t last = 0;

    if (nvs_get_u8(nvs, "fr_en", &enabled) == ESP_OK)
        loaded.filter_replace.enabled = enabled != 0;

    if (nvs_get_u16(nvs, "fr_days", &days) == ESP_OK &&
        days >= 1 && days <= 3650)
        loaded.filter_replace.interval_days = days;

    if (nvs_get_i64(nvs, "fr_last", &last) == ESP_OK)
        loaded.filter_replace.last_done = last;

    if (nvs_get_u8(nvs, "fc_en", &enabled) == ESP_OK)
        loaded.filter_clean.enabled = enabled != 0;

    if (nvs_get_u16(nvs, "fc_days", &days) == ESP_OK &&
        days >= 1 && days <= 3650)
        loaded.filter_clean.interval_days = days;

    if (nvs_get_i64(nvs, "fc_last", &last) == ESP_OK)
        loaded.filter_clean.last_done = last;

    if (nvs_get_u8(nvs, "cl_en", &enabled) == ESP_OK)
        loaded.chlorine.enabled = enabled != 0;

    if (nvs_get_u16(nvs, "cl_days", &days) == ESP_OK &&
        days >= 1 && days <= 3650)
        loaded.chlorine.interval_days = days;

    if (nvs_get_i64(nvs, "cl_last", &last) == ESP_OK)
        loaded.chlorine.last_done = last;

    nvs_close(nvs);

    maintenance_lock();
    g_maintenance = loaded;
    maintenance_unlock();

    ESP_LOGI(TAG, "Onderhoudsgegevens geladen");
}

static std::string maintenance_format_date(
    time_t timestamp,
    const RegionConfig &region)
{
    if (timestamp < 1700000000) {
        return "";
    }

    struct tm local_tm = {};
    localtime_r(&timestamp, &local_tm);

    char buffer[24] = {};

    if (region.date_format == "YYYY-MM-DD") {
        std::strftime(
            buffer,
            sizeof(buffer),
            "%Y-%m-%d",
            &local_tm);
    } else if (region.date_format == "MM-DD-YYYY") {
        std::strftime(
            buffer,
            sizeof(buffer),
            "%m-%d-%Y",
            &local_tm);
    } else {
        std::strftime(
            buffer,
            sizeof(buffer),
            "%d-%m-%Y",
            &local_tm);
    }

    return buffer;
}

static int maintenance_days_remaining(
    const MaintenanceItem &item,
    time_t now)
{
    if (item.last_done < 1700000000) {
        return static_cast<int>(item.interval_days);
    }

    struct tm last_tm = {};
    localtime_r(
        reinterpret_cast<const time_t *>(&item.last_done),
        &last_tm);

    /*
     * Middag gebruiken voorkomt problemen rond DST-overgangen.
     */
    last_tm.tm_hour = 12;
    last_tm.tm_min = 0;
    last_tm.tm_sec = 0;

    const time_t last_day =
        mktime(&last_tm);

    struct tm now_tm = {};
    localtime_r(&now, &now_tm);

    now_tm.tm_hour = 12;
    now_tm.tm_min = 0;
    now_tm.tm_sec = 0;

    const time_t today =
        mktime(&now_tm);

    const long elapsed_days =
        static_cast<long>(
            std::llround(
                difftime(today, last_day) /
                86400.0));

    return
        static_cast<int>(item.interval_days) -
        static_cast<int>(elapsed_days);
}

static time_t maintenance_next_date(
    const MaintenanceItem &item)
{
    if (item.last_done < 1700000000) {
        return 0;
    }

    time_t last =
        static_cast<time_t>(item.last_done);

    struct tm tm_value = {};
    localtime_r(&last, &tm_value);

    tm_value.tm_hour = 12;
    tm_value.tm_min = 0;
    tm_value.tm_sec = 0;
    tm_value.tm_mday += item.interval_days;

    return mktime(&tm_value);
}

static void maintenance_append_item_json(
    std::string &json,
    const char *name,
    const MaintenanceItem &item,
    time_t now,
    const RegionConfig &region)
{
    const int remaining =
        maintenance_days_remaining(item, now);

    const bool due =
        item.enabled &&
        item.last_done >= 1700000000 &&
        remaining <= 0;

    const std::string last_date =
        maintenance_format_date(
            static_cast<time_t>(item.last_done),
            region);

    const std::string next_date =
        maintenance_format_date(
            maintenance_next_date(item),
            region);

    json += "\"";
    json += name;
    json += "\":{";

    json += "\"enabled\":";
    json += item.enabled ? "true" : "false";

    json += ",\"intervalDays\":";
    json += std::to_string(item.interval_days);

    json += ",\"lastDate\":\"";
    json += json_escape(last_date);
    json += "\"";

    json += ",\"nextDate\":\"";
    json += json_escape(next_date);
    json += "\"";

    json += ",\"daysRemaining\":";
    json += std::to_string(remaining);

    json += ",\"due\":";
    json += due ? "true" : "false";

    json += "}";
}

static std::string maintenance_json()
{
    MaintenanceState state;

    maintenance_lock();
    state = g_maintenance;
    maintenance_unlock();

    RegionConfig region;
    load_region_config(region);

    const time_t now = time(nullptr);

    bool alarm = false;
    bool warning = false;

    const MaintenanceItem *items[] = {
        &state.filter_replace,
        &state.filter_clean,
        &state.chlorine
    };

    for (const MaintenanceItem *item : items) {
        if (!item->enabled ||
            item->last_done < 1700000000) {
            continue;
        }

        const int remaining =
            maintenance_days_remaining(
                *item,
                now);

        if (remaining <= 0) {
            alarm = true;
        } else if (remaining <= 2) {
            warning = true;
        }
    }

    const char *status =
        alarm
            ? "ALARM"
            : warning
                ? "WARNING"
                : "OK";

    std::string json = "{";

    json += "\"status\":\"";
    json += status;
    json += "\",";

    maintenance_append_item_json(
        json,
        "filterReplace",
        state.filter_replace,
        now,
        region);

    json += ",";

    maintenance_append_item_json(
        json,
        "filterClean",
        state.filter_clean,
        now,
        region);

    json += ",";

    maintenance_append_item_json(
        json,
        "chlorine",
        state.chlorine,
        now,
        region);

    json += "}";

    return json;
}

static bool maintenance_parse_days(
    const std::string &text,
    uint16_t &days)
{
    if (text.empty()) {
        return false;
    }

    char *end = nullptr;

    const long value =
        std::strtol(
            text.c_str(),
            &end,
            10);

    if (!end ||
        *end != '\0' ||
        value < 1 ||
        value > 3650) {
        return false;
    }

    days = static_cast<uint16_t>(value);
    return true;
}

static esp_err_t api_maintenance_get(
    httpd_req_t *req)
{
    send_json(req, maintenance_json());
    return ESP_OK;
}

static esp_err_t api_maintenance_post(
    httpd_req_t *req)
{
    std::string body;

    if (read_request_body(
            req,
            body,
            2048) != ESP_OK) {
        return ESP_FAIL;
    }

    MaintenanceState updated;

    maintenance_lock();
    updated = g_maintenance;
    maintenance_unlock();

    uint16_t days = 0;

    if (!maintenance_parse_days(
            form_value(body, "filterReplaceDays"),
            days)) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Ongeldig interval filter vervangen");

        return ESP_FAIL;
    }

    updated.filter_replace.interval_days = days;
    updated.filter_replace.enabled =
        form_bool(
            body,
            "filterReplaceEnabled",
            updated.filter_replace.enabled);

    if (!maintenance_parse_days(
            form_value(body, "filterCleanDays"),
            days)) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Ongeldig interval filter reinigen");

        return ESP_FAIL;
    }

    updated.filter_clean.interval_days = days;
    updated.filter_clean.enabled =
        form_bool(
            body,
            "filterCleanEnabled",
            updated.filter_clean.enabled);

    if (!maintenance_parse_days(
            form_value(body, "chlorineDays"),
            days)) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Ongeldig interval chloor");

        return ESP_FAIL;
    }

    updated.chlorine.interval_days = days;
    updated.chlorine.enabled =
        form_bool(
            body,
            "chlorineEnabled",
            updated.chlorine.enabled);

    maintenance_lock();
    g_maintenance = updated;
    maintenance_unlock();

    maintenance_save();

    ESP_LOGI(
        TAG,
        "Onderhoudsinstellingen opgeslagen");

    send_json(req, maintenance_json());
    return ESP_OK;
}

static esp_err_t api_maintenance_done(
    httpd_req_t *req)
{
    std::string body;

    if (read_request_body(
            req,
            body,
            512) != ESP_OK) {
        return ESP_FAIL;
    }

    const std::string item =
        form_value(body, "item");

    const time_t now = time(nullptr);

    if (now < 1700000000) {
        httpd_resp_set_status(
            req,
            "503 Service Unavailable");

        send_json(
            req,
            "{\"error\":\"Tijd is nog niet gesynchroniseerd\"}");

        return ESP_OK;
    }

    bool valid = true;

    maintenance_lock();

    if (item == "filterReplace") {
        g_maintenance.filter_replace.last_done =
            static_cast<int64_t>(now);

    } else if (item == "filterClean") {
        g_maintenance.filter_clean.last_done =
            static_cast<int64_t>(now);

    } else if (item == "chlorine") {
        g_maintenance.chlorine.last_done =
            static_cast<int64_t>(now);

    } else {
        valid = false;
    }

    maintenance_unlock();

    if (!valid) {
        httpd_resp_set_status(
            req,
            "400 Bad Request");

        send_json(
            req,
            "{\"error\":\"Onbekend onderhoudsitem\"}");

        return ESP_OK;
    }

    maintenance_save();

    ESP_LOGI(
        TAG,
        "Onderhoud geregistreerd: %s",
        item.c_str());

    send_json(req, maintenance_json());
    return ESP_OK;
}


static bool parse_schedule_form(const std::string &body,
                                ScheduleItem &item,
                                bool require_id,
                                uint16_t *id_out)
{
    const std::string id_text =
        form_value(body, "id");

    const std::string enabled_text =
        form_value(body, "enabled");

    const std::string days_text =
        form_value(body, "daysMask");

    const std::string hour_text =
        form_value(body, "hour");

    const std::string minute_text =
        form_value(body, "minute");

    const std::string action_text =
        form_value(body, "action");

    const std::string value_text =
        form_value(body, "value");

    if (require_id && id_text.empty()) {
        return false;
    }

    if (days_text.empty() ||
        hour_text.empty() ||
        minute_text.empty() ||
        action_text.empty()) {
        return false;
    }

    char *end = nullptr;

    if (require_id) {
        const long id =
            std::strtol(id_text.c_str(), &end, 10);

        if (!end || *end != '\0' ||
            id <= 0 || id > 65535) {
            return false;
        }

        if (id_out) {
            *id_out = static_cast<uint16_t>(id);
        }
    }

    end = nullptr;
    const long days =
        std::strtol(days_text.c_str(), &end, 10);

    if (!end || *end != '\0' ||
        days <= 0 || days > 127) {
        return false;
    }

    end = nullptr;
    const long hour =
        std::strtol(hour_text.c_str(), &end, 10);

    if (!end || *end != '\0' ||
        hour < 0 || hour > 23) {
        return false;
    }

    end = nullptr;
    const long minute =
        std::strtol(minute_text.c_str(), &end, 10);

    if (!end || *end != '\0' ||
        minute < 0 || minute > 59) {
        return false;
    }

    end = nullptr;
    const long action =
        std::strtol(action_text.c_str(), &end, 10);

    if (!end || *end != '\0' ||
        action < 0 || action > 8) {
        return false;
    }

    long value = 0;

    if (!value_text.empty()) {
        end = nullptr;
        value =
            std::strtol(value_text.c_str(), &end, 10);

        if (!end || *end != '\0') {
            return false;
        }
    }

    if (action == 8 &&
        (value < 20 || value > 40)) {
        return false;
    }

    item.enabled =
        enabled_text != "false" &&
        enabled_text != "0";

    item.days_mask =
        static_cast<uint8_t>(days);

    item.hour =
        static_cast<uint8_t>(hour);

    item.minute =
        static_cast<uint8_t>(minute);

    item.action =
        static_cast<ScheduleAction>(action);

    item.value =
        static_cast<int32_t>(value);

    return true;
}

static std::string schedules_json()
{
    std::string json = "[";

    for (uint8_t i = 0;
         i < scheduler_manager.count();
         ++i) {

        const ScheduleItem *item =
            scheduler_manager.get(i);

        if (!item) continue;

        if (json.size() > 1) {
            json += ",";
        }

        json += "{";

        json += "\"id\":";
        json += std::to_string(item->id);

        json += ",\"enabled\":";
        json += item->enabled ? "true" : "false";

        json += ",\"daysMask\":";
        json += std::to_string(item->days_mask);

        json += ",\"hour\":";
        json += std::to_string(item->hour);

        json += ",\"minute\":";
        json += std::to_string(item->minute);

        json += ",\"action\":";
        json += std::to_string(
            static_cast<unsigned>(item->action));

        json += ",\"value\":";
        json += std::to_string(item->value);

        json += "}";
    }

    json += "]";
    return json;
}

static esp_err_t api_schedules_get(httpd_req_t *req)
{
    send_json(req, schedules_json());
    return ESP_OK;
}

static esp_err_t api_schedules_post(httpd_req_t *req)
{
    std::string body;

    if (read_request_body(req, body, 512) != ESP_OK) {
        return ESP_FAIL;
    }

    ScheduleItem item;

    if (!parse_schedule_form(
            body,
            item,
            false,
            nullptr)) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Ongeldig plannerschema");

        return ESP_FAIL;
    }

    if (scheduler_manager.count() >=
        SchedulerManager::MAX_SCHEDULES) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Maximum aantal schema's bereikt");

        return ESP_FAIL;
    }

    if (!scheduler_manager.add(item)) {
        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Schema opslaan mislukt");

        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Planner schema toegevoegd");

    send_json(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t api_schedules_put(httpd_req_t *req)
{
    std::string body;

    if (read_request_body(req, body, 512) != ESP_OK) {
        return ESP_FAIL;
    }

    // Day switches use the live server record, never stale fields from a browser.
    const std::string day_text = form_value(body, "dayMask");
    if (!day_text.empty()) {
        const std::string id_text = form_value(body, "id");
        const std::string enabled_text = form_value(body, "enabled");
        char *id_end = nullptr;
        char *day_end = nullptr;
        const long id_value = std::strtol(id_text.c_str(), &id_end, 10);
        const long day_value = std::strtol(day_text.c_str(), &day_end, 10);
        if (id_text.empty() || !id_end || *id_end != '\0' ||
            id_value <= 0 || id_value > 65535 || !day_end || *day_end != '\0' ||
            day_value <= 0 || day_value > 64 ||
            (day_value & (day_value - 1)) != 0 ||
            (enabled_text != "true" && enabled_text != "false")) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Ongeldige planner dag");
            return ESP_FAIL;
        }
        const ScheduleDayResult result = scheduler_manager.set_day_enabled(
            static_cast<uint16_t>(id_value), static_cast<uint8_t>(day_value),
            enabled_text == "true");
        if (result != ScheduleDayResult::Ok) {
            httpd_resp_set_status(req,
                result == ScheduleDayResult::Full ? "409 Conflict" :
                result == ScheduleDayResult::NotFound ? "404 Not Found" :
                result == ScheduleDayResult::InvalidDay ? "400 Bad Request" :
                "500 Internal Server Error");
            send_json(req, result == ScheduleDayResult::Full
                ? "{\"error\":\"Planner vol (24 regels); maak ruimte om deze dag apart te schakelen\"}"
                : "{\"error\":\"Planner dag niet bijgewerkt\"}");
            return ESP_OK;
        }
        send_json(req, "{\"ok\":true}");
        return ESP_OK;
    }

    ScheduleItem item;
    uint16_t id = 0;

    if (!parse_schedule_form(
            body,
            item,
            true,
            &id)) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Ongeldig plannerschema");

        return ESP_FAIL;
    }

    if (!scheduler_manager.update(id, item)) {
        httpd_resp_send_err(
            req,
            HTTPD_404_NOT_FOUND,
            "Schema niet gevonden");

        return ESP_FAIL;
    }

    ESP_LOGI(
        TAG,
        "Planner schema %u bijgewerkt",
        static_cast<unsigned>(id));

    send_json(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t api_schedules_delete(httpd_req_t *req)
{
    char query[128] = {};

    const esp_err_t query_err =
        httpd_req_get_url_query_str(
            req,
            query,
            sizeof(query));

    if (query_err != ESP_OK) {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Schema-id ontbreekt");

        return ESP_FAIL;
    }

    char id_text[16] = {};

    if (httpd_query_key_value(
            query,
            "id",
            id_text,
            sizeof(id_text)) != ESP_OK) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Schema-id ontbreekt");

        return ESP_FAIL;
    }

    char *end = nullptr;

    const long id =
        std::strtol(id_text, &end, 10);

    if (!end ||
        *end != '\0' ||
        id <= 0 ||
        id > 65535) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Ongeldig schema-id");

        return ESP_FAIL;
    }

    /*
     * Als dayMask is meegegeven, verwijder alleen die specifieke
     * weekdag uit het schema. Alleen wanneer daarna geen dagen
     * meer overblijven wordt het volledige schema verwijderd.
     */
    char day_mask_text[16] = {};
    const bool has_day_mask =
        httpd_query_key_value(
            query,
            "dayMask",
            day_mask_text,
            sizeof(day_mask_text)) == ESP_OK;

    if (has_day_mask) {
        char *day_end = nullptr;
        const long day_mask =
            std::strtol(day_mask_text, &day_end, 10);

        if (!day_end ||
            *day_end != '\0' ||
            day_mask <= 0 ||
            day_mask > 127) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Ongeldig dagmasker");

            return ESP_FAIL;
        }

        const ScheduleItem *existing = nullptr;

        for (uint8_t i = 0;
             i < scheduler_manager.count();
             ++i) {

            const ScheduleItem *candidate =
                scheduler_manager.get(i);

            if (candidate &&
                candidate->id == static_cast<uint16_t>(id)) {
                existing = candidate;
                break;
            }
        }

        if (!existing) {
            httpd_resp_send_err(
                req,
                HTTPD_404_NOT_FOUND,
                "Schema niet gevonden");

            return ESP_FAIL;
        }

        ScheduleItem updated = *existing;

        updated.days_mask =
            static_cast<uint8_t>(
                updated.days_mask &
                ~static_cast<uint8_t>(day_mask));

        if (updated.days_mask == 0) {
            if (!scheduler_manager.remove(
                    static_cast<uint16_t>(id))) {

                httpd_resp_send_err(
                    req,
                    HTTPD_500_INTERNAL_SERVER_ERROR,
                    "Schema verwijderen mislukt");

                return ESP_FAIL;
            }

            ESP_LOGI(
                TAG,
                "Planner schema %ld volledig verwijderd",
                id);
        } else {
            if (!scheduler_manager.update(
                    static_cast<uint16_t>(id),
                    updated)) {

                httpd_resp_send_err(
                    req,
                    HTTPD_500_INTERNAL_SERVER_ERROR,
                    "Planner dag verwijderen mislukt");

                return ESP_FAIL;
            }

            ESP_LOGI(
                TAG,
                "Planner schema %ld dagmasker bijgewerkt: 0x%02X",
                id,
                updated.days_mask);
        }

        send_json(req, "{\"ok\":true}");
        return ESP_OK;
    }

    /*
     * Geen dayMask: bestaande functie behouden.
     * Dit wordt gebruikt voor 'Aangevinkte verwijderen'.
     */
    if (!scheduler_manager.remove(
            static_cast<uint16_t>(id))) {

        httpd_resp_send_err(
            req,
            HTTPD_404_NOT_FOUND,
            "Schema niet gevonden");

        return ESP_FAIL;
    }

    ESP_LOGI(
        TAG,
        "Planner schema %ld verwijderd",
        id);

    send_json(req, "{\"ok\":true}");
    return ESP_OK;
}


static void restart_task(void *)
{
    // Geef de HTTP-response eerst tijd om naar de browser te gaan.
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}


static esp_err_t api_restart(httpd_req_t *req)
{
    send_json(req, "{\"ok\":true,\"restarting\":true}");

    const BaseType_t task_ok = xTaskCreate(
        restart_task,
        "web_restart",
        4096,
        nullptr,
        5,
        nullptr);

    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "Restart-task kon niet worden gestart");
        return ESP_FAIL;
    }

    return ESP_OK;
}



static std::string maintenance_backup_json()
{
    MaintenanceState state;

    maintenance_lock();
    state = g_maintenance;
    maintenance_unlock();

    auto append_item =
        [](std::string &json,
           const char *name,
           const MaintenanceItem &item)
        {
            json += "\"";
            json += name;
            json += "\":{";

            json += "\"enabled\":";
            json += item.enabled ? "true" : "false";

            json += ",\"intervalDays\":";
            json += std::to_string(
                item.interval_days);

            /*
             * Als string bewaren:
             * - geen 32-bit JSON-parserbeperking
             * - exacte epochwaarde blijft behouden
             */
            json += ",\"lastDone\":\"";
            json += std::to_string(
                item.last_done);
            json += "\"";

            json += "}";
        };

    std::string json = "{";

    append_item(
        json,
        "filterReplace",
        state.filter_replace);

    json += ",";

    append_item(
        json,
        "filterClean",
        state.filter_clean);

    json += ",";

    append_item(
        json,
        "chlorine",
        state.chlorine);

    json += "}";

    return json;
}


static esp_err_t api_backup_file_get(httpd_req_t *req)
{
    char query[128] = {};
    char name[32] = {};

    if (httpd_req_get_url_query_str(
            req,
            query,
            sizeof(query)) != ESP_OK ||
        httpd_query_key_value(
            query,
            "name",
            name,
            sizeof(name)) != ESP_OK) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Back-up naam ontbreekt");

        return ESP_FAIL;
    }

    const std::string backup_name = name;

    if (backup_name == "hardware") {
        HardwareConfig cfg;
        load_hardware_config(cfg);

        send_json(
            req,
            hardware_config_json(cfg));

        return ESP_OK;
    }

    if (backup_name == "maintenance") {
        send_json(
            req,
            maintenance_backup_json());

        return ESP_OK;
    }

    if (backup_name == "settings") {
        RegionConfig region;
        load_region_config(region);

        std::string json = "{";

        json += "\"region\":";
        json += region_config_json(region);

        json += ",\"mqtt\":";
        json += mqtt_config_json();

        json += "}";

        send_json(req, json);
        return ESP_OK;
    }

    httpd_resp_send_err(
        req,
        HTTPD_404_NOT_FOUND,
        "Onbekend back-up onderdeel");

    return ESP_FAIL;
}


static esp_err_t api_backup_file_post(httpd_req_t *req)
{
    char query[128] = {};
    char name[32] = {};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "name", name, sizeof(name)) != ESP_OK) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Back-up naam ontbreekt");
        return ESP_FAIL;
    }

    std::string body;

    if (read_request_body(req, body, 8192) != ESP_OK) {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Back-up data ongeldig");
        return ESP_FAIL;
    }

    const std::string backup_name = name;

    if (backup_name == "hardware") {
        HardwareConfig cfg;

        int cio = -1;
        int dsp = -1;
        int temp = 0;
        std::string pcb;

        if (!json_get_int(body, "cio", cio) ||
            !json_get_int(body, "dsp", dsp) ||
            cio < 0 || cio > 8 ||
            dsp < 0 || dsp > 8 ||
            !json_get_string(body, "pcb", pcb) ||
            !json_get_pin_array(body, cfg.pins)) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Ongeldige hardware back-up");
            return ESP_FAIL;
        }

        if (bestway_model_info(static_cast<BestwayModel>(cio)).family !=
            bestway_model_info(static_cast<BestwayModel>(dsp)).family) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "CIO en DSP protocol komen niet overeen");
            return ESP_FAIL;
        }

        std::string temp_text;

        if (json_get_string(body, "hasTempSensor", temp_text)) {
            temp = (temp_text == "1" || temp_text == "true") ? 1 : 0;
        } else {
            int temp_number = 0;

            if (json_get_int(body, "hasTempSensor", temp_number)) {
                temp = temp_number != 0 ? 1 : 0;
            }
        }

        cfg.cio = static_cast<uint8_t>(cio);
        cfg.dsp = static_cast<uint8_t>(dsp);
        cfg.pcb = "esp32c6";
        cfg.has_temp_sensor = temp != 0;

        if (save_hardware_config(cfg) != ESP_OK) {
            httpd_resp_send_err(
                req,
                HTTPD_500_INTERNAL_SERVER_ERROR,
                "Hardware herstellen mislukt");
            return ESP_FAIL;
        }

        send_json(req, "{\"ok\":true,\"restored\":\"hardware\"}");
        return ESP_OK;
    }

    if (backup_name == "maintenance") {
        MaintenanceState restored;

        maintenance_lock();
        restored = g_maintenance;
        maintenance_unlock();

        struct RestoreItem {
            const char *name;
            MaintenanceItem *item;
        };

        RestoreItem items[] = {
            {"filterReplace", &restored.filter_replace},
            {"filterClean",   &restored.filter_clean},
            {"chlorine",      &restored.chlorine},
        };

        for (auto &entry : items) {
            std::string item_json;

            if (!json_get_object(
                    body,
                    entry.name,
                    item_json)) {

                httpd_resp_send_err(
                    req,
                    HTTPD_400_BAD_REQUEST,
                    "Ongeldige onderhoud back-up");

                return ESP_FAIL;
            }

            bool enabled =
                entry.item->enabled;

            int interval_days =
                entry.item->interval_days;

            std::string last_done_text;

            if (!json_get_bool(
                    item_json,
                    "enabled",
                    enabled) ||
                !json_get_int(
                    item_json,
                    "intervalDays",
                    interval_days) ||
                !json_get_string(
                    item_json,
                    "lastDone",
                    last_done_text)) {

                httpd_resp_send_err(
                    req,
                    HTTPD_400_BAD_REQUEST,
                    "Ongeldige onderhoud back-up");

                return ESP_FAIL;
            }

            if (interval_days < 1 ||
                interval_days > 3650) {

                httpd_resp_send_err(
                    req,
                    HTTPD_400_BAD_REQUEST,
                    "Ongeldig onderhoudsinterval");

                return ESP_FAIL;
            }

            char *end = nullptr;

            const long long last_done =
                std::strtoll(
                    last_done_text.c_str(),
                    &end,
                    10);

            if (!end ||
                *end != '\0' ||
                last_done < 0) {

                httpd_resp_send_err(
                    req,
                    HTTPD_400_BAD_REQUEST,
                    "Ongeldige onderhoudsdatum");

                return ESP_FAIL;
            }

            entry.item->enabled =
                enabled;

            entry.item->interval_days =
                static_cast<uint16_t>(
                    interval_days);

            entry.item->last_done =
                static_cast<int64_t>(
                    last_done);
        }

        maintenance_lock();
        g_maintenance = restored;
        maintenance_unlock();

        maintenance_save();

        ESP_LOGI(
            TAG,
            "Onderhoud hersteld uit back-up");

        send_json(
            req,
            "{\"ok\":true,\"restored\":\"maintenance\"}");

        return ESP_OK;
    }

    if (backup_name == "settings") {
        std::string region_json;
        std::string mqtt_json;

        if (!json_get_object(body, "region", region_json) ||
            !json_get_object(body, "mqtt", mqtt_json)) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Ongeldige settings back-up");

            return ESP_FAIL;
        }

        /*
         * ====================================================
         * Regio / tijd
         * ====================================================
         */
        RegionConfig region;
        load_region_config(region);

        std::string time_zone;
        std::string date_format;
        bool clock24 = region.use_24_hour_clock;

        if (!json_get_string(
                region_json,
                "timeZone",
                time_zone) ||
            !json_get_string(
                region_json,
                "dateFormat",
                date_format) ||
            !json_get_bool(
                region_json,
                "use24HourClock",
                clock24)) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Ongeldige regio-instellingen in back-up");

            return ESP_FAIL;
        }

        if (!valid_time_zone(time_zone) ||
            (date_format != "DD-MM-YYYY" &&
             date_format != "MM/DD/YYYY" &&
             date_format != "YYYY-MM-DD")) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Ongeldige regio-instellingen in back-up");

            return ESP_FAIL;
        }

        region.time_zone = time_zone;
        region.date_format = date_format;
        region.use_24_hour_clock = clock24;

        /*
         * ====================================================
         * MQTT
         * ====================================================
         */
        MqttConfig restored =
            mqtt_manager.config();

        bool bool_value = false;
        int int_value = 0;
        std::string string_value;

        if (json_get_bool(
                mqtt_json,
                "enabled",
                bool_value)) {
            restored.enabled = bool_value;
        }

        if (json_get_string(
                mqtt_json,
                "host",
                string_value)) {
            restored.host = string_value;
        }

        if (json_get_int(
                mqtt_json,
                "port",
                int_value) &&
            int_value >= 1 &&
            int_value <= 65535) {

            restored.port =
                static_cast<uint16_t>(int_value);
        }

        if (json_get_string(
                mqtt_json,
                "username",
                string_value)) {
            restored.username = string_value;
        }

        /*
         * Wachtwoord zit bewust niet in de back-up.
         * Bestaand opgeslagen wachtwoord blijft behouden.
         */

        if (json_get_string(
                mqtt_json,
                "clientId",
                string_value) &&
            !string_value.empty()) {

            restored.client_id =
                string_value;
        }

        if (json_get_string(
                mqtt_json,
                "baseTopic",
                string_value) &&
            !string_value.empty()) {

            restored.base_topic =
                string_value;
        }

        if (json_get_bool(
                mqtt_json,
                "homeAssistantDiscovery",
                bool_value)) {

            restored.home_assistant_discovery =
                bool_value;
        }

        struct PubRestore {
            const char *name;
            MqttPublicationConfig *publication;
        };

        PubRestore publications[] = {
            {"Temperature",   &restored.temperature},
            {"Target",        &restored.target},
            {"Power",         &restored.power},
            {"Heater",        &restored.heater},
            {"HeatingActive", &restored.heating_active},
            {"Godmode",       &restored.godmode},
            {"FullPower",     &restored.full_power},
            {"Filter",        &restored.filter},
            {"Bubbles",       &restored.bubbles},
            {"Jets",          &restored.jets},
            {"Locked",        &restored.locked},
            {"Connected",     &restored.connected},
            {"Ready",         &restored.ready},
            {"Rssi",          &restored.rssi},
            {"Heap",          &restored.heap},
            {"Uptime",        &restored.uptime},
            {"Firmware",      &restored.firmware},
            {"Ip",            &restored.ip},
            {"Json",          &restored.json},
            {"Maintenance",   &restored.maintenance},
            {"MaintenanceFilterReplace", &restored.maintenance_filter_replace},
            {"MaintenanceFilterClean",   &restored.maintenance_filter_clean},
            {"MaintenanceChlorine",      &restored.maintenance_chlorine},
        };

        for (auto &entry : publications) {
            const std::string publish_key =
                std::string("publish") + entry.name;

            const std::string topic_key =
                std::string("topic") + entry.name;

            if (json_get_bool(
                    mqtt_json,
                    publish_key.c_str(),
                    bool_value)) {

                entry.publication->enabled =
                    bool_value;
            }

            if (json_get_string(
                    mqtt_json,
                    topic_key.c_str(),
                    string_value) &&
                !string_value.empty()) {

                entry.publication->topic =
                    string_value;
            }
        }

        /*
         * Eerst opslaan, daarna toepassen.
         */
        if (save_region_config(region) != ESP_OK) {
            httpd_resp_send_err(
                req,
                HTTPD_500_INTERNAL_SERVER_ERROR,
                "Regio herstellen mislukt");

            return ESP_FAIL;
        }

        mqtt_manager.config() = restored;

        if (!mqtt_manager.save()) {
            httpd_resp_send_err(
                req,
                HTTPD_500_INTERNAL_SERVER_ERROR,
                "MQTT herstellen mislukt");

            return ESP_FAIL;
        }

        apply_region_timezone();

        if (!mqtt_manager.start()) {
            httpd_resp_send_err(
                req,
                HTTPD_500_INTERNAL_SERVER_ERROR,
                "MQTT opnieuw starten mislukt");

            return ESP_FAIL;
        }

        ESP_LOGI(
            TAG,
            "Settings hersteld uit back-up");

        send_json(
            req,
            "{\"ok\":true,\"restored\":\"settings\"}");

        return ESP_OK;
    }

    httpd_resp_send_err(
        req,
        HTTPD_404_NOT_FOUND,
        "Onbekend back-up onderdeel");
    return ESP_FAIL;
}



/* ============================================================
 * Diagnostiek en live logboek
 * ============================================================ */

struct WebLogEntry {
    int64_t timestamp = 0;
    char level[6] = "INFO";
    char message[256] = {};
};

static constexpr size_t WEB_LOG_CAPACITY = 100;
// Logging staat standaard uit. Reserveer daarom in productie geen tientallen
// kilobytes RAM voor een logbuffer die normaal niet gebruikt wordt.
// De buffer wordt alleen tijdens een bewuste debug-sessie dynamisch aangemaakt.
static WebLogEntry *g_web_logs = nullptr;

static size_t g_web_log_start = 0;
static size_t g_web_log_count = 0;

static portMUX_TYPE g_web_log_mux =
    portMUX_INITIALIZER_UNLOCKED;

static vprintf_like_t g_previous_log_vprintf = nullptr;
static bool g_web_log_hook_installed = false;
// Bewust NIET persistent: na iedere reboot staat debug logging uit.
static bool g_web_log_enabled = false;


static const char *web_log_level_from_line(const char *line)
{
    if (!line) return "INFO";

    for (const char *p = line; *p; ++p) {
        if (p[1] == ' ' && p[2] == '(') {
            if (*p == 'E') return "ERROR";
            if (*p == 'W') return "WARN";
            if (*p == 'I' || *p == 'D' || *p == 'V') return "INFO";
        }
    }

    return "INFO";
}


static void web_log_store(
    const char *level,
    const char *message)
{
    if (!g_web_log_enabled || !g_web_logs) return;
    if (!message || !*message) return;

    WebLogEntry entry = {};
    entry.timestamp = static_cast<int64_t>(time(nullptr));

    std::snprintf(
        entry.level,
        sizeof(entry.level),
        "%s",
        level ? level : "INFO");

    size_t len = std::strlen(message);

    while (len &&
           (message[len - 1] == '\r' ||
            message[len - 1] == '\n')) {
        --len;
    }

    if (len >= sizeof(entry.message)) {
        len = sizeof(entry.message) - 1;
    }

    std::memcpy(entry.message, message, len);
    entry.message[len] = '\0';

    if (!entry.message[0]) return;

    portENTER_CRITICAL(&g_web_log_mux);

    size_t index;

    if (g_web_log_count < WEB_LOG_CAPACITY) {
        index =
            (g_web_log_start + g_web_log_count) %
            WEB_LOG_CAPACITY;
        ++g_web_log_count;
    } else {
        index = g_web_log_start;
        g_web_log_start =
            (g_web_log_start + 1) %
            WEB_LOG_CAPACITY;
    }

    g_web_logs[index] = entry;

    portEXIT_CRITICAL(&g_web_log_mux);
}


static int web_log_vprintf(
    const char *format,
    va_list args)
{
    va_list normal_args;
    va_copy(normal_args, args);

    int result = 0;

    if (g_previous_log_vprintf) {
        result =
            g_previous_log_vprintf(
                format,
                normal_args);
    } else {
        result =
            std::vprintf(
                format,
                normal_args);
    }

    va_end(normal_args);

    char line[320] = {};

    va_list copy_args;
    va_copy(copy_args, args);

    std::vsnprintf(
        line,
        sizeof(line),
        format,
        copy_args);

    va_end(copy_args);

    web_log_store(
        web_log_level_from_line(line),
        line);

    return result;
}


static void web_log_install_hook()
{
    if (g_web_log_hook_installed) return;

    g_previous_log_vprintf =
        esp_log_set_vprintf(web_log_vprintf);

    g_web_log_hook_installed = true;
}


static void web_log_uninstall_hook()
{
    if (!g_web_log_hook_installed) return;

    if (g_previous_log_vprintf) {
        esp_log_set_vprintf(g_previous_log_vprintf);
    }

    g_web_log_hook_installed = false;
    g_previous_log_vprintf = nullptr;
}


static void web_log_clear_buffer()
{
    portENTER_CRITICAL(&g_web_log_mux);
    g_web_log_start = 0;
    g_web_log_count = 0;
    portEXIT_CRITICAL(&g_web_log_mux);
}


static esp_err_t web_log_set_enabled(bool enabled)
{
    if (enabled == g_web_log_enabled) return ESP_OK;

    if (enabled) {
        // Pas RAM reserveren wanneer de gebruiker debug logging bewust aanzet.
        // 100 entries is circa 27 kB; de vroegere vaste ring + snapshot kostten
        // samen ruim 54 kB RAM, ook wanneer logging uit stond.
        if (!g_web_logs) {
            g_web_logs = static_cast<WebLogEntry *>(
                heap_caps_calloc(
                    WEB_LOG_CAPACITY,
                    sizeof(WebLogEntry),
                    MALLOC_CAP_8BIT));

            if (!g_web_logs) {
                return ESP_ERR_NO_MEM;
            }
        }

        web_log_clear_buffer();
        g_web_log_enabled = true;
        web_log_install_hook();

        // Alleen tijdens een bewuste debug-sessie runtime logging aan.
        esp_log_level_set("*", ESP_LOG_INFO);
        return ESP_OK;
    }

    // Eerst de hook stoppen zodat niemand meer in de buffer schrijft.
    web_log_uninstall_hook();
    g_web_log_enabled = false;
    esp_log_level_set("*", ESP_LOG_NONE);

    portENTER_CRITICAL(&g_web_log_mux);
    WebLogEntry *buffer = g_web_logs;
    g_web_logs = nullptr;
    g_web_log_start = 0;
    g_web_log_count = 0;
    portEXIT_CRITICAL(&g_web_log_mux);

    if (buffer) {
        heap_caps_free(buffer);
    }

    return ESP_OK;
}


static std::string web_log_json_escape(const char *text)
{
    std::string out;

    if (!text) return out;

    for (unsigned char c : std::string(text)) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c >= 0x20 && c != 0x7f) {
                    out += static_cast<char>(c);
                }
                break;
        }
    }

    return out;
}


static esp_err_t api_logs_get(httpd_req_t *req)
{
    // Logging staat normaal uit: geen buffer, geen snapshot en geen heapwerk.
    if (!g_web_log_enabled || !g_web_logs) {
        send_json(req, "[]");
        return ESP_OK;
    }

    size_t limit = 50;
    const size_t query_len = httpd_req_get_url_query_len(req);

    if (query_len > 0 && query_len < 128) {
        char query[128] = {};
        char value[16] = {};
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
            httpd_query_key_value(query, "limit", value, sizeof(value)) == ESP_OK) {
            const long parsed = std::strtol(value, nullptr, 10);
            if (parsed > 0) limit = static_cast<size_t>(parsed);
        }
    }

    if (limit > WEB_LOG_CAPACITY) limit = WEB_LOG_CAPACITY;

    size_t start = 0;
    size_t count = 0;
    portENTER_CRITICAL(&g_web_log_mux);
    count = g_web_log_count;
    if (count > limit) count = limit;
    start = (g_web_log_start + (g_web_log_count - count)) % WEB_LOG_CAPACITY;
    portEXIT_CRITICAL(&g_web_log_mux);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    esp_err_t err = httpd_resp_send_chunk(req, "[", 1);
    if (err != ESP_OK) return err;

    for (size_t i = 0; i < count; ++i) {
        WebLogEntry entry = {};
        const size_t index = (start + i) % WEB_LOG_CAPACITY;

        // Slechts één entry (~272 bytes) per keer onder de lock kopiëren.
        portENTER_CRITICAL(&g_web_log_mux);
        if (g_web_logs) {
            entry = g_web_logs[index];
        }
        portEXIT_CRITICAL(&g_web_log_mux);

        std::string chunk;
        chunk.reserve(384);
        if (i) chunk += ",";
        chunk += "{\"timestamp\":";
        chunk += std::to_string(entry.timestamp);
        chunk += ",\"level\":\"";
        chunk += entry.level;
        chunk += "\",\"message\":\"";
        chunk += web_log_json_escape(entry.message);
        chunk += "\"}";

        err = httpd_resp_send_chunk(req, chunk.c_str(), chunk.size());
        if (err != ESP_OK) return err;
    }

    err = httpd_resp_send_chunk(req, "]", 1);
    if (err != ESP_OK) return err;
    return httpd_resp_send_chunk(req, nullptr, 0);
}


static esp_err_t api_logs_delete(httpd_req_t *req)
{
    web_log_clear_buffer();
    send_json(req, "{\"ok\":true}");
    return ESP_OK;
}


static esp_err_t api_logging_get(httpd_req_t *req)
{
    char json[180];
    std::snprintf(json, sizeof(json),
        "{\"enabled\":%s,\"uptimeMs\":%lld,\"resetReason\":\"%s\"}",
        g_web_log_enabled ? "true" : "false",
        static_cast<long long>(esp_timer_get_time() / 1000), reset_reason_text());
    send_json(req, json);
    return ESP_OK;
}


static esp_err_t api_logging_post(httpd_req_t *req)
{
    std::string body;
    if (read_request_body(req, body, 128) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Ongeldige logging-instelling");
        return ESP_FAIL;
    }

    const std::string value = form_value(body, "enabled");
    if (value != "1" && value != "true" && value != "on" && value != "yes" &&
        value != "0" && value != "false" && value != "off" && value != "no") {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Ongeldige logging-instelling");
        return ESP_FAIL; // An absent/malformed parameter must not disable logging.
    }
    const bool enabled =
        value == "1" || value == "true" || value == "on" || value == "yes";

    const esp_err_t log_err = web_log_set_enabled(enabled);
    if (log_err != ESP_OK) {
        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Onvoldoende geheugen voor debug logging");
        return log_err;
    }

    send_json(req, enabled
        ? "{\"ok\":true,\"enabled\":true}"
        : "{\"ok\":true,\"enabled\":false}");
    return ESP_OK;
}


static bool diagnostics_wifi_connected()
{
    wifi_ap_record_t ap = {};
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
}


static int diagnostics_wifi_rssi()
{
    wifi_ap_record_t ap = {};

    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK
        ? ap.rssi
        : 0;
}


static bool diagnostics_time_synchronized()
{
    return time(nullptr) >= 1700000000;
}


static uint32_t diagnostics_heap_fragmentation()
{
    const size_t free_heap =
        heap_caps_get_free_size(MALLOC_CAP_8BIT);

    const size_t largest =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    if (!free_heap || largest >= free_heap) {
        return 0;
    }

    return static_cast<uint32_t>(
        100 - ((largest * 100) / free_heap));
}


static esp_err_t api_diagnostics_get(httpd_req_t *req)
{
    const uint32_t free_heap =
        static_cast<uint32_t>(
            heap_caps_get_free_size(MALLOC_CAP_8BIT));

    std::string json = "{";

    json += "\"wifiConnected\":";
    json += diagnostics_wifi_connected() ? "true" : "false";

    json += ",\"rssi\":";
    json += std::to_string(diagnostics_wifi_rssi());

    json += ",\"mqttConnected\":";
    json += mqtt_manager.connected() ? "true" : "false";

    json += ",\"timeSynchronized\":";
    json += diagnostics_time_synchronized() ? "true" : "false";

    json += ",\"spaConnected\":";
    json += spa.connected ? "true" : "false";

    json += ",\"spaDataValid\":";
    json += spa.data_valid ? "true" : "false";

    json += ",\"type1Power\":";
    json += spa.power ? "true" : "false";

    json += ",\"type1Payload7\":";
    json += std::to_string(bestway_bus.type1_debug_payload7());

    json += ",\"type1Payload9\":";
    json += std::to_string(bestway_bus.type1_debug_payload9());

    json += ",\"type1Brightness\":";
    json += std::to_string(bestway_bus.type1_debug_brightness());

    json += ",\"type1DspUploadCount\":";
    json += std::to_string(bestway_bus.type1_debug_dsp_upload_count());

    json += ",\"type1DspPayload\":[";
    for (size_t i = 0; i < 11; ++i) {
        if (i > 0) {
            json += ",";
        }
        json += std::to_string(
            bestway_bus.type1_debug_dsp_payload(i));
    }
    json += "]";

    json += ",\"spaErrorCode\":";
    json += std::to_string(spa.error_code);

    json += ",\"freeHeap\":";
    json += std::to_string(free_heap);

    json += ",\"heapFragmentation\":";
    json += std::to_string(
        diagnostics_heap_fragmentation());

    json += ",\"otaLastError\":\"\"";

    json += ",\"mqttReconnectCount\":";
    json += std::to_string(
        mqtt_manager.reconnect_count());

    json += ",\"mqttLastError\":";
    json += std::to_string(
        mqtt_manager.last_error());

    json += ",\"matterEnabled\":";
    json += web_matter_enabled() ? "true" : "false";

    json += ",\"matterRuntimeActive\":";
    json += (g_matter_runtime_active_cb && g_matter_runtime_active_cb()) ? "true" : "false";

    json += ",\"statusLedAvailable\":";
    json += status_led_available() ? "true" : "false";

    json += ",\"statusLedGpio\":";
    json += std::to_string(status_led_gpio());

    json += ",\"statusLedState\":\"";
    json += status_led_state_name();
    json += "\"";

    json += ",\"statusLedColor\":\"";
    json += status_led_color_name();
    json += "\"";

    json += ",\"statusLedBlinking\":";
    json += status_led_is_blinking() ? "true" : "false";

    json += "}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    return httpd_resp_send(
        req,
        json.c_str(),
        json.size());
}


static esp_err_t api_selftest_post(httpd_req_t *req)
{
    const bool memory_ok =
        heap_caps_get_free_size(MALLOC_CAP_8BIT) >
        20480;

    const bool wifi_ok =
        diagnostics_wifi_connected();

    const bool time_ok =
        diagnostics_time_synchronized();

    const MqttConfig &mqtt_cfg =
        mqtt_manager.config();

    const bool mqtt_ok =
        !mqtt_cfg.enabled ||
        mqtt_manager.connected();

    const bool spa_ok =
        spa.connected &&
        spa.data_valid;

    const bool ok =
        memory_ok &&
        wifi_ok &&
        time_ok &&
        mqtt_ok &&
        spa_ok;

    std::string json = "{";

    json += "\"ok\":";
    json += ok ? "true" : "false";

    json += ",\"memory\":";
    json += memory_ok ? "true" : "false";

    json += ",\"wifi\":";
    json += wifi_ok ? "true" : "false";

    json += ",\"time\":";
    json += time_ok ? "true" : "false";

    json += ",\"mqtt\":";
    json += mqtt_ok ? "true" : "false";

    json += ",\"spa\":";
    json += spa_ok ? "true" : "false";

    json += "}";

    web_log_store(
        ok ? "INFO" : "WARN",
        ok
            ? "Diagnostische zelftest geslaagd"
            : "Diagnostische zelftest: aandacht vereist");

    httpd_resp_set_type(req, "application/json");

    return httpd_resp_send(
        req,
        json.c_str(),
        json.size());
}



/*
 * ============================================================
 * Firmware OTA
 * ============================================================
 *
 * Alleen de applicatie-firmware wordt bijgewerkt.
 * Webinterface is embedded in de firmware; LittleFS wordt niet
 * gebruikt.
 *
 * Werkwijze:
 * - upload gaat naar de niet-actieve OTA-partitie
 * - huidige firmware blijft onaangeroerd tijdens upload
 * - esp_ota_end() valideert het nieuwe app-image
 * - bootpartitie wordt pas na succesvolle validatie gewijzigd
 * - daarna gecontroleerde herstart
 */

static uint8_t g_ota_buffer[4096];


static void ota_restart_task(void *)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}


static esp_err_t api_ota_post(
    httpd_req_t *req)
{
    if (!req) {
        return ESP_FAIL;
    }

    const int content_length =
        req->content_len;

    if (content_length <= 0) {
        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Firmwarebestand ontbreekt");

        return ESP_FAIL;
    }

    const esp_partition_t *running =
        esp_ota_get_running_partition();

    const esp_partition_t *update =
        esp_ota_get_next_update_partition(nullptr);

    if (!update) {
        ESP_LOGE(
            TAG,
            "OTA: geen vrije OTA-partitie gevonden");

        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Geen OTA-partitie beschikbaar");

        return ESP_FAIL;
    }

    if (static_cast<size_t>(content_length) >
        update->size) {

        ESP_LOGE(
            TAG,
            "OTA: firmware te groot: %d bytes, partitie %u bytes",
            content_length,
            static_cast<unsigned>(update->size));

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Firmwarebestand is te groot");

        return ESP_FAIL;
    }

    status_led_set_ota_active(true);

    ESP_LOGI(
        TAG,
        "OTA gestart: running=%s update=%s size=%d/%u",
        running ? running->label : "?",
        update->label,
        content_length,
        static_cast<unsigned>(update->size));

    esp_ota_handle_t ota_handle = 0;

    esp_err_t err =
        esp_ota_begin(
            update,
            static_cast<size_t>(content_length),
            &ota_handle);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "esp_ota_begin mislukt: %s",
            esp_err_to_name(err));

        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "OTA kon niet worden gestart");

        status_led_set_ota_active(false);
        return ESP_FAIL;
    }

    int remaining = content_length;
    size_t written = 0;

    while (remaining > 0) {
        const int wanted =
            std::min(
                remaining,
                static_cast<int>(sizeof(g_ota_buffer)));

        const int received =
            httpd_req_recv(
                req,
                reinterpret_cast<char *>(g_ota_buffer),
                wanted);

        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            ESP_LOGE(
                TAG,
                "OTA upload timeout");

            esp_ota_abort(ota_handle);

            httpd_resp_send_err(
                req,
                HTTPD_408_REQ_TIMEOUT,
                "OTA upload timeout");

            status_led_set_ota_active(false);
            return ESP_FAIL;
        }

        if (received <= 0) {
            ESP_LOGE(
                TAG,
                "OTA upload afgebroken na %u bytes",
                static_cast<unsigned>(written));

            esp_ota_abort(ota_handle);

            status_led_set_ota_active(false);
            return ESP_FAIL;
        }

        err =
            esp_ota_write(
                ota_handle,
                g_ota_buffer,
                static_cast<size_t>(received));

        if (err != ESP_OK) {
            ESP_LOGE(
                TAG,
                "esp_ota_write mislukt na %u bytes: %s",
                static_cast<unsigned>(written),
                esp_err_to_name(err));

            esp_ota_abort(ota_handle);

            httpd_resp_send_err(
                req,
                HTTPD_500_INTERNAL_SERVER_ERROR,
                "Firmware schrijven mislukt");

            status_led_set_ota_active(false);
            return ESP_FAIL;
        }

        written +=
            static_cast<size_t>(received);

        remaining -= received;
    }

    err =
        esp_ota_end(ota_handle);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "OTA image validatie mislukt: %s",
            esp_err_to_name(err));

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Ongeldig firmwarebestand");

        status_led_set_ota_active(false);
        return ESP_FAIL;
    }

    err =
        esp_ota_set_boot_partition(update);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "OTA bootpartitie instellen mislukt: %s",
            esp_err_to_name(err));

        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Bootpartitie instellen mislukt");

        status_led_set_ota_active(false);
        return ESP_FAIL;
    }

    ESP_LOGI(
        TAG,
        "OTA succesvol: %u bytes naar %s",
        static_cast<unsigned>(written),
        update->label);

    send_json(
        req,
        "{\"ok\":true,\"restarting\":true}");

    const BaseType_t task_ok =
        xTaskCreate(
            ota_restart_task,
            "ota_restart",
            4096,
            nullptr,
            5,
            nullptr);

    if (task_ok != pdPASS) {
        ESP_LOGE(
            TAG,
            "OTA restart-task kon niet worden gestart");

        /*
         * Firmware en bootpartitie zijn al succesvol ingesteld.
         * Een handmatige restart activeert de nieuwe firmware.
         */
    }

    return ESP_OK;
}


static esp_err_t api_not_implemented(httpd_req_t *req)
{
    httpd_resp_set_status(req, "501 Not Implemented");
    send_json(req, "{\"ok\":false,\"phase\":3,\"error\":\"Deze functie volgt in een volgende migratiefase\"}");
    return ESP_OK;
}

static esp_err_t static_handler(httpd_req_t *req)
{
    std::string uri(req->uri);
    const auto q = uri.find('?');
    if (q != std::string::npos) uri.resize(q);

    for (const auto &asset : ASSETS) {
        if (uri == asset.uri) {
            httpd_resp_set_type(req, asset.mime);
            if (uri == "/sw.js" || uri.find("/js/") == 0 || uri.find("/css/") == 0) {
                httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
            }
            const size_t len = static_cast<size_t>(asset.end - asset.start);
            return httpd_resp_send(req, reinterpret_cast<const char *>(asset.start), len);
        }
    }
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Niet gevonden");
    return ESP_FAIL;
}

static void register_uri(const char *uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t h = {};
    h.uri = uri;
    h.method = method;
    h.handler = handler;
    h.user_ctx = nullptr;
    ESP_ERROR_CHECK(httpd_register_uri_handler(g_server, &h));
}

esp_err_t web_server_start()
{
    if (g_server) return ESP_OK;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    /*
     * Extra ruimte voor Historie, Energie, Onderhoud en verdere
     * gemigreerde v3.2.6 API's.
     */
    cfg.max_uri_handlers = 48;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.lru_purge_enable = true;

    if (!g_history_mutex) {
        g_history_mutex = xSemaphoreCreateMutex();
    }

    if (!g_history_mutex) {
        ESP_LOGE(TAG, "Historie mutex aanmaken mislukt");
        return ESP_ERR_NO_MEM;
    }

    history_load();

    if (!g_maintenance_mutex) {
        g_maintenance_mutex = xSemaphoreCreateMutex();
    }

    if (!g_maintenance_mutex) {
        ESP_LOGE(TAG, "Onderhoud mutex aanmaken mislukt");
        return ESP_ERR_NO_MEM;
    }

    maintenance_load();

    if (!g_energy_mutex) {
        g_energy_mutex = xSemaphoreCreateMutex();
    }

    if (!g_energy_mutex) {
        ESP_LOGE(TAG, "Energie mutex aanmaken mislukt");
        return ESP_ERR_NO_MEM;
    }

    energy_load();

    if (!g_energy_task) {
        const BaseType_t energy_task_result =
            xTaskCreate(
                energy_task_fn,
                "spa_energy",
                4096,
                nullptr,
                4,
                &g_energy_task);

        if (energy_task_result != pdPASS) {
            g_energy_task = nullptr;
            ESP_LOGE(TAG, "Energie task starten mislukt");
            return ESP_ERR_NO_MEM;
        }
    }

    if (!g_history_task) {
        const BaseType_t task_result =
            xTaskCreate(
                history_task_fn,
                "spa_history",
                4096,
                nullptr,
                4,
                &g_history_task);

        if (task_result != pdPASS) {
            g_history_task = nullptr;
            ESP_LOGE(TAG, "Historie task starten mislukt");
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = httpd_start(&g_server, &cfg);
    if (err != ESP_OK) return err;

    // Logging is in normale productie bewust volledig UIT.
    // /api/logging zet tijdens een handmatige debug-sessie zowel
    // de ESP runtime logs als de live web-logcapture tijdelijk aan.
    g_web_log_enabled = false;
    esp_log_level_set("*", ESP_LOG_NONE);

    register_uri("/api/status", HTTP_GET, api_status);
    register_uri("/api/command", HTTP_POST, api_command);

    httpd_uri_t ws_uri = {};
    ws_uri.uri = "/ws";
    ws_uri.method = HTTP_GET;
    ws_uri.handler = websocket_handler;
    ws_uri.user_ctx = nullptr;
    ws_uri.is_websocket = true;
    ESP_ERROR_CHECK(httpd_register_uri_handler(g_server, &ws_uri));
    register_uri("/api/settings/region", HTTP_GET, api_region);
    register_uri("/api/settings/region", HTTP_POST, api_region_post);
    register_uri("/api/settings/system", HTTP_GET, api_system);

    register_uri("/api/settings/mqtt", HTTP_GET, api_mqtt_get);
    register_uri("/api/settings/mqtt", HTTP_POST, api_mqtt_post);
    register_uri("/api/wifi/status", HTTP_GET, api_wifi_status);
    register_uri("/api/wifi/scan", HTTP_GET, api_wifi_scan);
    register_uri("/api/wifi/connect", HTTP_POST, api_wifi_connect);
    register_uri("/api/wifi/forget", HTTP_POST, api_wifi_forget);
    register_uri("/api/settings/matter", HTTP_GET, api_matter_status);
    register_uri("/api/settings/matter", HTTP_POST, api_matter_settings_post);
    register_uri("/api/matter/reset", HTTP_POST, api_matter_reset_post);

    register_uri("/api/settings/demo", HTTP_GET, api_demo_get);
    register_uri("/api/settings/demo", HTTP_POST, api_demo_post);

    register_uri("/api/hardware", HTTP_GET, api_hardware_get);
    register_uri("/api/hardware", HTTP_POST, api_hardware_post);
    register_uri("/api/hardware/test", HTTP_POST, api_hardware_test);

    register_uri("/api/schedules", HTTP_GET, api_schedules_get);
    register_uri("/api/schedules", HTTP_POST, api_schedules_post);
    register_uri("/api/schedules", HTTP_PUT, api_schedules_put);
    register_uri("/api/schedules", HTTP_DELETE, api_schedules_delete);

    register_uri("/api/restart", HTTP_POST, api_restart);

    register_uri("/api/maintenance", HTTP_GET, api_maintenance_get);
    register_uri("/api/maintenance", HTTP_POST, api_maintenance_post);
    register_uri("/api/maintenance/done", HTTP_POST, api_maintenance_done);

    register_uri("/api/history", HTTP_GET, api_history_get);
    register_uri("/api/history", HTTP_DELETE, api_history_delete);

    register_uri("/api/energy", HTTP_GET, api_energy_get);
    register_uri("/api/energy", HTTP_POST, api_energy_post);
    register_uri("/api/energy", HTTP_DELETE, api_energy_delete);

    register_uri("/api/backup/file", HTTP_GET, api_backup_file_get);
    register_uri("/api/backup/file", HTTP_POST, api_backup_file_post);

    register_uri("/api/ota", HTTP_POST, api_ota_post);

    // Niet-geimplementeerde API-calls worden onderaan door /api/* opgevangen.
    // De twee extra /api/settings/* en /api/wifi/* POST-wildcards waren redundant
    // en vulden samen met WebSocket alle HTTPD URI-slots, waardoor de laatste
    // static-handler niet meer geregistreerd kon worden (ESP_ERR_HTTPD_HANDLERS_FULL).
    register_uri("/api/diagnostics", HTTP_GET, api_diagnostics_get);
    register_uri("/api/selftest", HTTP_POST, api_selftest_post);

    register_uri("/api/logging", HTTP_GET, api_logging_get);
    register_uri("/api/logging", HTTP_POST, api_logging_post);
    register_uri("/api/logs", HTTP_GET, api_logs_get);
    register_uri("/api/logs", HTTP_DELETE, api_logs_delete);

    register_uri("/api/*", HTTP_GET, api_not_implemented);
    register_uri("/api/*", HTTP_POST, api_not_implemented);
    register_uri("/api/*", HTTP_DELETE, api_not_implemented);
    register_uri("/api/*", HTTP_PUT, api_not_implemented);

    register_uri("/*", HTTP_GET, static_handler);

    ESP_LOGI(TAG, "Webinterface gestart op poort 80");
    ESP_LOGI(TAG, "Live WebSocket actief op /ws (zelfde HTTP-server)");
    ESP_LOGI(TAG, "Dashboard: push-state + directe knoppen, REST alleen fallback");
    return ESP_OK;
}

