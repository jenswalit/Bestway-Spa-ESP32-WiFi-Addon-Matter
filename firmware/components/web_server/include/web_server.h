#pragma once
#include <esp_err.h>
#include <string>

using web_matter_commissioned_cb_t = bool (*)();
using web_matter_status_cb_t = std::string (*)(bool runtime_active);
using web_matter_open_window_cb_t = esp_err_t (*)();
using web_matter_reset_cb_t = esp_err_t (*)();
using web_matter_runtime_active_cb_t = bool (*)();
using web_matter_wifi_sync_cb_t = esp_err_t (*)(const char *ssid,
                                                const char *password);

esp_err_t web_server_start();

// Push de actuele spa-state naar verbonden WebSocket-clients.
// Wordt buiten interrupts aangeroepen wanneer BestwayBus state_version wijzigt.
void web_server_push_live_state();

void web_matter_register_callbacks(web_matter_commissioned_cb_t commissioned_cb,
                                   web_matter_reset_cb_t reset_cb,
                                   web_matter_runtime_active_cb_t runtime_active_cb,
                                   web_matter_wifi_sync_cb_t wifi_sync_cb,
                                   web_matter_status_cb_t status_cb,
                                   web_matter_open_window_cb_t open_window_cb);
bool web_matter_enabled();

void web_time_sync_start();
