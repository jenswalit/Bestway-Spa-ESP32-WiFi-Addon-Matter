#pragma once
#include <cstdint>
#include <string>

struct SpaState {
    bool mqtt_online = false;
    bool heater = false;
    bool heater_active = false;
    bool filter = false;
    bool bubbles = false;
    bool jets = false;
    bool power = false;
    bool locked = false;
    bool fahrenheit = false;
    bool timer_active = false;
    bool connected = false;
    bool data_valid = false;

    int temperature_c = 37;

    // Startwaarde is alleen een interne placeholder.
    // target_temperature_valid bepaalt of deze waarde werkelijk
    // door de pomp bevestigd is.
    int target_temperature_c = 20;
    bool target_temperature_valid = false;

    // 0 = geen fout, anders bijvoorbeeld E01 -> 1, E02 -> 2
    int error_code = 0;

    uint64_t boot_ms = 0;
    uint64_t last_packet_ms = 0;

    void begin();
    void set_connection_state(bool online);
    void mark_packet_received();
    void set_target_temperature(int target_c);
    std::string uptime() const;
};

extern SpaState spa;
