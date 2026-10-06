#pragma once
#include <esp_err.h>
#include <string>

// Must run before Wi-Fi/BLE/ADC and before the spa bus starts.
esp_err_t bestway_matter_credentials_init();
void bestway_matter_register_provider();
// Only the explicit Matter-reset action calls this. No general NVS erase.
esp_err_t bestway_matter_rotate_credentials();
std::string bestway_matter_status_json(bool runtime_active);
// Queues work on the existing Matter task; ESP_OK means accepted, not completed.
esp_err_t bestway_matter_open_window();
