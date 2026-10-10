#include "spa_state.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>

#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
using httpd_err_code_t = int;
constexpr int HTTPD_400_BAD_REQUEST = 400;
constexpr int HTTPD_501_METHOD_NOT_IMPLEMENTED = 501;
enum class BestwayProtocolFamily { Uart4Wire, SixWireType1, SixWireType2 };

// Hardware calls are recorded; simulation methods below come from the bus source.
class BestwayBus {
public:
    enum class Mode { Simulation, Hardware };
    struct ModelInfo { BestwayProtocolFamily family; };
    Mode mode_ = Mode::Simulation;
    ModelInfo info{BestwayProtocolFamily::Uart4Wire};
    bool god = false, full = false;
    int version = 0, hardware_steps = 0;
    Mode mode() const { return mode_; }
    const char *mode_name() const { return mode_ == Mode::Simulation ? "SIMULATION" : "HARDWARE"; }
    const ModelInfo &model_info() const { return info; }
    bool godmode() const { return god; }
    bool godmode_full_power() const { return full; }
    void set_godmode(bool value) { god = value; }
    void set_godmode_full_power(bool value) { full = value; }
    void bump_version() { ++version; }
    void set_target_temperature(int target_c);
    void step_target(bool up);
    void set_power(bool value) { spa.power = value; }
    void set_heater(bool value) { spa.heater = value; }
    void set_filter(bool value) { spa.filter = value; }
    void set_bubbles(bool value) { spa.bubbles = value; }
    void set_jets(bool value) { spa.jets = value; }
    void set_unit(bool value) { spa.fahrenheit = value; }
    void set_lock(bool value) { spa.locked = value; }
    void press_timer() { spa.timer_active = !spa.timer_active; }
};
BestwayBus bestway_bus;
void get_network_info(char *ip, size_t length, int *rssi, bool *wifi)
{
    std::snprintf(ip, length, "192.0.2.1");
    *rssi = -50;
    *wifi = true;
}
unsigned esp_get_free_heap_size() { return 100000; }

#include "demo_bus.inc"
#include "demo_status.inc"
#include "demo_command.inc"

int main()
{
    // Same setter used at demo startup; target must appear in the real status JSON.
    bestway_bus.set_target_temperature(39);
    assert(spa.target_temperature_valid);
    assert(status_json().find("\"target\":39") != std::string::npos);
    for (auto family : {BestwayProtocolFamily::Uart4Wire,
                        BestwayProtocolFamily::SixWireType1,
                        BestwayProtocolFamily::SixWireType2}) {
        bestway_bus.info.family = family;
        bestway_bus.mode_ = BestwayBus::Mode::Simulation;
        bestway_bus.god = false;
        bestway_bus.set_target_temperature(39);
        assert(execute_live_command("target:+").ok);
        assert(spa.target_temperature_c == 40);
        assert(execute_live_command("target:+").ok);
        assert(spa.target_temperature_c == 40);
        bestway_bus.set_target_temperature(20);
        assert(execute_live_command("target:-").ok);
        assert(spa.target_temperature_c == 20);
        assert(execute_live_command("target:+").ok);
        assert(spa.target_temperature_c == 21);
        assert(execute_live_command("toggle:heater").ok);
        assert(execute_live_command("toggle:power").ok);
    }
    bestway_bus.mode_ = BestwayBus::Mode::Hardware;
    bestway_bus.info.family = BestwayProtocolFamily::Uart4Wire;
    bestway_bus.god = false;
    assert(!execute_live_command("target:+").ok);
    assert(!execute_live_command("target:-").ok);
    assert(!execute_live_command("toggle:heater").ok);
    assert(!execute_live_command("toggle:power").ok);
    bestway_bus.god = true;
    assert(execute_live_command("target:+").ok);
    assert(execute_live_command("toggle:heater").ok);
    assert(!execute_live_command("toggle:power").ok);
    bestway_bus.info.family = BestwayProtocolFamily::SixWireType1;
    assert(execute_live_command("target:-").ok);
    assert(execute_live_command("toggle:power").ok);
    bestway_bus.info.family = BestwayProtocolFamily::SixWireType2;
    assert(execute_live_command("target:+").ok);
    assert(!execute_live_command("toggle:power").ok);
    assert(!execute_live_command("unknown").ok);
    spa.target_temperature_valid = false;
    assert(status_json().find("\"target\":null") != std::string::npos);
    std::cout << "PASS: demo target visible, +/- bounded at 20-40 C, hardware command restrictions preserved\n";
}
