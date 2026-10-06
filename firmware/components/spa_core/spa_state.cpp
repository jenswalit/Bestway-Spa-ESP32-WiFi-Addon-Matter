#include "spa_state.h"
#include <esp_timer.h>
#include <cstdio>

SpaState spa;

static uint64_t now_ms()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
}

void SpaState::begin()
{
    boot_ms = now_ms();
}

void SpaState::set_connection_state(bool online)
{
    connected = online;

    if (!online) {
        data_valid = false;
    }
}

void SpaState::mark_packet_received()
{
    last_packet_ms = now_ms();
    data_valid = true;
    connected = true;
}

void SpaState::set_target_temperature(int target_c)
{
    if (target_c < 20) target_c = 20;
    if (target_c > 40) target_c = 40;
    target_temperature_c = target_c;
}

std::string SpaState::uptime() const
{
    uint64_t total = (now_ms() - boot_ms) / 1000ULL;
    const uint64_t h = total / 3600ULL;
    total %= 3600ULL;
    const uint64_t m = total / 60ULL;
    const uint64_t s = total % 60ULL;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%lluu %llum %llus",
                  static_cast<unsigned long long>(h),
                  static_cast<unsigned long long>(m),
                  static_cast<unsigned long long>(s));
    return std::string(buf);
}
