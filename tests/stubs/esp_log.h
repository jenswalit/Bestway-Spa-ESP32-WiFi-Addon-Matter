#pragma once
#include <cstdint>
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_FAIL=-1;
inline const char* esp_err_to_name(int){return "test";}
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
