#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "bestway_bus.h"
#include "spa_state.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <cstring>
#include <nvs.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "esp_rom_sys.h"

#ifndef CONFIG_BESTWAY_SIMULATION
#define CONFIG_BESTWAY_SIMULATION 1
#endif

static const char *TAG = "bestway_bus";
static constexpr uint64_t SIM_ACK_DELAY_MS = 250;
static constexpr uint64_t SIM_PACKET_INTERVAL_MS = 1000;
static constexpr uint64_t SIM_HEAT_STEP_MS = 5000;
static constexpr uint64_t SIM_COOL_STEP_MS = 30000;



static void load_saved_pins(uint8_t pins[8])
{
    // Nieuwe ESP32-C6 printmapping (2026-09):
    // CIO1,CIO2,CIO3,DSP1,DSP2,DSP3,Audio,Temp
    static constexpr uint8_t NEW_6WIRE_PINS[8] = {23, 22, 21, 20, 19, 18, 11, 0};
    static constexpr uint8_t OLD_6WIRE_PINS[8] = {4, 5, 6, 7, 10, 11, 18, 19};
    static constexpr uint8_t NEW_4WIRE_PINS[8] = {23, 22, 0, 20, 19, 0, 0, 0};
    static constexpr uint8_t OLD_4WIRE_PINS[8] = {4, 5, 0, 7, 10, 0, 0, 19};

    nvs_handle_t nvs = 0;

    if (nvs_open("spa_hw", NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }

    size_t size = 8;
    if (nvs_get_blob(nvs, "pins", pins, &size) != ESP_OK || size != 8) {
        nvs_close(nvs);
        return;
    }

    // Deze firmware ondersteunt nog maar één print: onze ESP32-C6 print.
    // Daarom worden de twee exact bekende oude presets altijd gemigreerd.
    // Andere/custom GPIO-arrays blijven onaangeroerd.
    const bool old_6wire = std::memcmp(pins, OLD_6WIRE_PINS, sizeof(OLD_6WIRE_PINS)) == 0;
    const bool old_4wire = std::memcmp(pins, OLD_4WIRE_PINS, sizeof(OLD_4WIRE_PINS)) == 0;

    if (old_6wire || old_4wire) {
        const uint8_t *replacement = old_4wire ? NEW_4WIRE_PINS : NEW_6WIRE_PINS;
        std::memcpy(pins, replacement, 8);

        esp_err_t save_err = nvs_set_blob(nvs, "pins", pins, 8);
        if (save_err == ESP_OK) {
            save_err = nvs_set_str(nvs, "pcb", "esp32c6");
        }
        if (save_err == ESP_OK) {
            save_err = nvs_commit(nvs);
        }

        if (save_err == ESP_OK) {
            ESP_LOGW(TAG,
                     "Oude GPIO-preset automatisch gemigreerd naar nieuwe ESP32-C6 print: %u,%u,%u,%u,%u,%u,%u,%u",
                     pins[0], pins[1], pins[2], pins[3],
                     pins[4], pins[5], pins[6], pins[7]);
        } else {
            ESP_LOGE(TAG, "Migratie van oude GPIO-preset kon niet in NVS worden opgeslagen: %s",
                     esp_err_to_name(save_err));
        }
    }

    nvs_close(nvs);
}

static BestwayModel load_saved_model()
{
    nvs_handle_t nvs = 0;

    if (nvs_open("spa_hw", NVS_READONLY, &nvs) != ESP_OK) {
        return BestwayModel::PRE2021;
    }

    uint8_t model = 0;

    if (nvs_get_u8(nvs, "cio", &model) != ESP_OK || model > 8) {
        nvs_close(nvs);
        return BestwayModel::PRE2021;
    }

    nvs_close(nvs);
    return static_cast<BestwayModel>(model);
}

BestwayBus bestway_bus;

static uint64_t now_ms()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
}


static bool load_saved_demo_mode()
{
    /*
     * Runtime demo-instelling.
     *
     * Wanneer nog niets in NVS staat, gebruiken we de bestaande
     * CONFIG_BESTWAY_SIMULATION waarde als fabrieksdefault.
     */
    bool enabled = CONFIG_BESTWAY_SIMULATION != 0;

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


void BestwayBus::begin()
{
    model_ = load_saved_model();
    load_saved_pins(pins_);

    ESP_LOGI(TAG, "Model uit NVS: %s", model_name());
    ESP_LOGI(TAG, "GPIO uit NVS: %u,%u,%u,%u,%u,%u,%u,%u",
             pins_[0], pins_[1], pins_[2], pins_[3],
             pins_[4], pins_[5], pins_[6], pins_[7]);
    ESP_LOGI(TAG, "Mogelijkheden: AirJet=%s HydroJet=%s",
             has_air() ? "ja" : "nee",
             has_jets() ? "ja" : "nee");

    const bool demo_mode = load_saved_demo_mode();

    if (demo_mode) {
        mode_ = Mode::Simulation;
        connected_ = true;
        spa.set_connection_state(true);
        spa.data_valid = true;
        spa.temperature_c = 37;
        set_target_temperature(39);

        ESP_LOGW(TAG, "DEMO-MODUS actief - geen pomp nodig");
        ESP_LOGI(TAG,
                 "Bestway model: %s | protocol: %s | air=%s | jets=%s",
                 model_name(),
                 protocol_name(),
                 has_air() ? "ja" : "nee",
                 has_jets() ? "ja" : "nee");

        ESP_LOGI(TAG,
                 "Commando's krijgen na %llu ms een gesimuleerde pompbevestiging",
                 static_cast<unsigned long long>(SIM_ACK_DELAY_MS));
    } else {
        mode_ = Mode::Hardware;
        connected_ = false;
        spa.set_connection_state(false);
        spa.data_valid = false;

        if (model_info().family == BestwayProtocolFamily::Uart4Wire) {
            if (init_uart4wire()) {
                ESP_LOGI(TAG, "HARDWARE mode: 4-wire UART gestart");
            } else {
                ESP_LOGE(TAG,
                         "HARDWARE mode: 4-wire UART initialisatie mislukt");
            }
        } else if (model_info().family ==
                   BestwayProtocolFamily::SixWireType1) {

            const bool cio_ok = init_type1_cio();
            const bool dsp_ok = init_type1_dsp();

            if (cio_ok && dsp_ok) {
                ESP_LOGI(TAG,
                         "HARDWARE mode: 6-wire Type1 CIO + DSP gestart");
            } else {
                ESP_LOGE(
                    TAG,
                    "HARDWARE mode: 6-wire Type1 initialisatie mislukt CIO=%s DSP=%s",
                    cio_ok ? "ok" : "fout",
                    dsp_ok ? "ok" : "fout");
            }

        } else if (model_info().family ==
                   BestwayProtocolFamily::SixWireType2) {

            const bool cio_ok = init_type2_cio();
            const bool dsp_ok = init_type2_dsp();

            if (cio_ok && dsp_ok) {
                ESP_LOGI(TAG,
                         "HARDWARE mode: 6-wire Type2 CIO + DSP gestart");
            } else {
                ESP_LOGE(
                    TAG,
                    "HARDWARE mode: 6-wire Type2 initialisatie mislukt CIO=%s DSP=%s",
                    cio_ok ? "ok" : "fout",
                    dsp_ok ? "ok" : "fout");
            }

        } else {
            ESP_LOGW(TAG,
                     "HARDWARE mode: protocol %s nog niet geactiveerd",
                     protocol_name());
        }
    }
}

void BestwayBus::loop()
{
    /*
     * Tijdens de bench-connectortest zijn de zes buslijnen tijdelijk
     * eigendom van de test. De normale CIO/DSP code mag ze dan niet
     * aanraken.
     */
    if (connector_test_active_) {
        return;
    }

    const uint64_t now = now_ms();
    if (mode_ == Mode::Simulation) {
        simulation_loop(now);
    } else {
        hardware_loop(now);
    }
}

bool BestwayBus::is_connected() const { return connected_; }
BestwayBus::Mode BestwayBus::mode() const { return mode_; }
const char *BestwayBus::mode_name() const { return mode_ == Mode::Simulation ? "SIMULATION" : "HARDWARE"; }
uint32_t BestwayBus::state_version() const { return state_version_; }

BestwayModel BestwayBus::model() const
{
    return model_;
}

const BestwayModelInfo &BestwayBus::model_info() const
{
    return bestway_model_info(model_);
}

const char *BestwayBus::model_name() const
{
    return model_info().name;
}

const char *BestwayBus::protocol_name() const
{
    return bestway_protocol_name(model_info().family);
}

bool BestwayBus::has_air() const
{
    return model_info().has_air;
}

bool BestwayBus::has_jets() const
{
    return model_info().has_jets;
}

bool BestwayBus::godmode() const
{
    return model_info().family ==
               BestwayProtocolFamily::Uart4Wire &&
           godmode_;
}

void BestwayBus::set_godmode(bool enabled)
{
    if (model_info().family !=
        BestwayProtocolFamily::Uart4Wire) {

        if (enabled) {
            ESP_LOGW(
                TAG,
                "Godmode genegeerd: alleen beschikbaar voor 4-wire");
        }

        godmode_ = false;
        return;
    }

    if (godmode_ == enabled) {
        ESP_LOGI(
            TAG,
            "4-wire Godmode al %s",
            enabled ? "AAN" : "UIT");
        return;
    }

    godmode_ = enabled;

    /*
     * Bij inschakelen synchroniseren we eerst onze interne 4-wire
     * state met de laatst bekende fysieke DSP-status.
     *
     * Daarmee start Godmode niet vanuit een willekeurige toestand.
     * De daadwerkelijke Godmode output-logica wordt in de volgende
     * stap aangesloten op uart4wire_loop().
     */
    if (godmode_ && have_dsp_state_) {
        sync_4wire_state_from_dsp(
            dsp_bubbles_,
            dsp_jets_,
            dsp_pump_,
            spa.heater);
    }

    /*
     * Oude remote-doelen niet over de moduswissel heen meenemen.
     * Nieuwe opdrachten worden na inschakelen opnieuw aangevraagd.
     */
    pending_pump_ = false;
    pending_bubbles_ = false;
    pending_jets_ = false;

    godmode_heater_stage_ = 0;
    godmode_heater2_due_ms_ = 0;
    godmode_pump_off_pending_ = false;
    godmode_pump_off_due_ms_ = 0;
    godmode_hysteresis_ = false;

    /*
     * Bij verlaten van Godmode is de originele spa weer volledig
     * verantwoordelijk voor de heater-output.
     */
    if (!godmode_) {
        spa.heater_active = false;
    }

    bump_version();

    ESP_LOGI(
        TAG,
        "4-wire Godmode %s",
        godmode_ ? "AAN" : "UIT");
}

void BestwayBus::bump_version()
{
    ++state_version_;
}

void BestwayBus::queue(PendingBool &pending, bool value, const char *name)
{
    pending.active = true;
    pending.value = value;
    pending.due_ms = now_ms() + SIM_ACK_DELAY_MS;
    ESP_LOGI(TAG, "command %s=%s -> wacht op pompbevestiging", name, value ? "on" : "off");
}

bool BestwayBus::apply_pending(PendingBool &pending, bool &target, const char *name, uint64_t now)
{
    if (!pending.active || now < pending.due_ms) return false;
    pending.active = false;
    if (target == pending.value) return false;
    target = pending.value;
    ESP_LOGI(TAG, "SIM pomp -> %s bevestigd: %s", name, target ? "on" : "off");
    return true;
}

bool BestwayBus::godmode_full_power() const
{
    return
        model_info().family ==
            BestwayProtocolFamily::Uart4Wire &&
        godmode_full_power_;
}


void BestwayBus::set_godmode_full_power(bool enabled)
{
    if (model_info().family !=
        BestwayProtocolFamily::Uart4Wire) {

        ESP_LOGW(
            TAG,
            "Full Power genegeerd: alleen beschikbaar voor 4-wire");

        return;
    }

    if (godmode_full_power_ == enabled) {
        return;
    }

    godmode_full_power_ = enabled;

    /*
     * Als Full Power tijdens actief verwarmen UIT wordt gezet,
     * direct terugvallen naar alleen trap 1.
     *
     * De heater zelf blijft aan; alleen heat2 verdwijnt.
     */
    if (!godmode_full_power_ &&
        godmode_heater_stage_ > 1) {

        godmode_heater_stage_ = 1;
        godmode_heater2_due_ms_ = 0;
    }

    /*
     * Als Full Power tijdens trap 1 AAN wordt gezet,
     * opnieuw dezelfde 10 seconden vertraging gebruiken.
     */
    if (godmode_full_power_ &&
        godmode_ &&
        spa.heater &&
        godmode_heater_stage_ == 1) {

        godmode_heater2_due_ms_ =
            esp_timer_get_time() / 1000ULL + 10000;
    }

    bump_version();

    ESP_LOGI(
        TAG,
        "4-wire Godmode Full Power=%s",
        godmode_full_power_ ? "ON" : "OFF");
}



void BestwayBus::set_power(bool on)
{
    if (mode_ == Mode::Simulation) {
        queue(power_, on, "power");
        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType1) {

        type1_remote_press_state(
            BestwayButton::Power,
            on,
            5000);

        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType2) {

        /*
         * Type2 / M54149E:
         *
         * Net als Spa Control v3.2.6 is de Power-status niet
         * betrouwbaar uitleesbaar en wordt die als altijd AAN
         * beschouwd. Daarom Power hier niet state-aware behandelen,
         * maar als één echte fysieke Power-knopdruk.
         *
         * 'on' wordt bewust niet gebruikt voor de knoplogica:
         * iedere oproep betekent één Power-toggle.
         */
        ESP_LOGI(
            TAG,
            "Type2 Power momentary toggle aangevraagd (%s)",
            on ? "ON request" : "OFF request");

        type2_start_momentary_action(
            BestwayButton::Power,
            100);

        return;
    }

    ESP_LOGW(TAG,
             "hardware command power=%s nog niet aangesloten voor %s",
             on ? "on" : "off",
             protocol_name());
}
void BestwayBus::set_filter(bool on)
{
    if (mode_ == Mode::Simulation) {
        queue(filter_, on, "filter");
        return;
    }

    if (model_info().family == BestwayProtocolFamily::Uart4Wire) {
        if (godmode_) {
            Bestway4WireState state =
                bestway_4wire_state(model_, hw_state_index_);

            /*
             * Spa Control v3.2.6:
             * pomp uit tijdens heaterbedrijf -> eerst heater uit,
             * daarna nog 5 seconden pomp laten draaien.
             */
            if (!on && state.pump && spa.heater) {
                spa.heater = false;
                godmode_heater_stage_ = 0;
                godmode_heater2_due_ms_ = 0;

                if (state.heat) {
                    apply_4wire_toggle(Bestway4WireToggle::Heat);
                }

                godmode_pump_off_pending_ = true;
                godmode_pump_off_due_ms_ = now_ms() + 5000;

                ESP_LOGI(
                    TAG,
                    "4-wire Godmode Pump OFF: heater uit, 5 s cooling delay");

                bump_version();
                return;
            }

            godmode_pump_off_pending_ = false;
            godmode_pump_off_due_ms_ = 0;

            if (state.pump != on) {
                apply_4wire_toggle(Bestway4WireToggle::Pump);
            }

            pending_pump_ = false;
            target_pump_ = on;

            ESP_LOGI(TAG,
                     "4-wire Godmode Pump=%s",
                     on ? "on" : "off");
            return;
        }

        target_pump_ = on;

        if (have_dsp_state_ && dsp_pump_ == on) {
            pending_pump_ = false;

            ESP_LOGI(TAG,
                     "4-wire remote Pump al %s",
                     on ? "on" : "off");
        } else {
            pending_pump_ = true;

            ESP_LOGI(TAG,
                     "4-wire remote Pump doel=%s",
                     on ? "on" : "off");
        }

        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType1) {

        type1_start_state_action(
            BestwayButton::Pump,
            on);

        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType2) {

        type2_start_state_action(
            BestwayButton::Pump,
            on);

        return;
    }

    ESP_LOGW(TAG,
             "hardware command filter=%s nog niet aangesloten voor %s",
             on ? "on" : "off",
             protocol_name());
}

void BestwayBus::set_bubbles(bool on)
{
    if (!has_air()) {
        ESP_LOGW(TAG, "bubbles geweigerd: model %s heeft geen AirJet", model_name());
        return;
    }

    if (mode_ == Mode::Simulation) {
        queue(bubbles_, on, "bubbles");
        return;
    }

    if (model_info().family == BestwayProtocolFamily::Uart4Wire) {
        if (godmode_) {
            const Bestway4WireState state =
                bestway_4wire_state(model_, hw_state_index_);

            if (state.bubbles != on) {
                apply_4wire_toggle(Bestway4WireToggle::Bubbles);
            }

            pending_bubbles_ = false;
            target_bubbles_ = on;

            ESP_LOGI(TAG,
                     "4-wire Godmode Bubbles=%s",
                     on ? "on" : "off");
            return;
        }

        target_bubbles_ = on;

        if (have_dsp_state_ && dsp_bubbles_ == on) {
            pending_bubbles_ = false;

            ESP_LOGI(TAG,
                     "4-wire remote Bubbles al %s",
                     on ? "on" : "off");
        } else {
            pending_bubbles_ = true;

            ESP_LOGI(TAG,
                     "4-wire remote Bubbles doel=%s",
                     on ? "on" : "off");
        }

        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType1) {

        type1_start_state_action(
            BestwayButton::Bubbles,
            on);

        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType2) {

        type2_start_state_action(
            BestwayButton::Bubbles,
            on);

        return;
    }

    ESP_LOGW(TAG,
             "hardware command bubbles=%s nog niet aangesloten voor %s",
             on ? "on" : "off",
             protocol_name());
}

void BestwayBus::set_jets(bool on)
{
    if (!has_jets()) {
        ESP_LOGW(TAG, "jets geweigerd: model %s heeft geen HydroJet", model_name());
        return;
    }

    if (mode_ == Mode::Simulation) {
        queue(jets_, on, "jets");
        return;
    }

    if (model_info().family == BestwayProtocolFamily::Uart4Wire) {
        if (godmode_) {
            const Bestway4WireState state =
                bestway_4wire_state(model_, hw_state_index_);

            if (state.jets != on) {
                apply_4wire_toggle(Bestway4WireToggle::Jets);
            }

            pending_jets_ = false;
            target_jets_ = on;

            ESP_LOGI(TAG,
                     "4-wire Godmode Jets=%s",
                     on ? "on" : "off");
            return;
        }

        target_jets_ = on;

        if (have_dsp_state_ && dsp_jets_ == on) {
            pending_jets_ = false;

            ESP_LOGI(TAG,
                     "4-wire remote Jets al %s",
                     on ? "on" : "off");
        } else {
            pending_jets_ = true;

            ESP_LOGI(TAG,
                     "4-wire remote Jets doel=%s",
                     on ? "on" : "off");
        }

        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType1) {

        type1_start_state_action(
            BestwayButton::HydroJets,
            on);

        return;
    }

    ESP_LOGW(TAG,
             "hardware command jets=%s nog niet aangesloten voor %s",
             on ? "on" : "off",
             protocol_name());
}

void BestwayBus::set_heater(bool on)
{
    if (mode_ == Mode::Simulation) {
        queue(heater_, on, "heater");
        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::Uart4Wire) {

        if (!godmode_) {
            ESP_LOGW(TAG,
                     "4-wire Heater vereist Godmode");
            return;
        }

        Bestway4WireState state =
            bestway_4wire_state(model_, hw_state_index_);

        /*
         * spa.heater is de verwarmingsvraag ("groene heater").
         * spa.heater_active wordt door de thermostaat bepaald.
         */
        if (spa.heater != on) {
            spa.heater = on;
            bump_version();
        }

        if (state.heat != on) {
            apply_4wire_toggle(Bestway4WireToggle::Heat);
            state = bestway_4wire_state(
                model_,
                hw_state_index_);
        }

        if (on) {
            /*
             * Verwarmen vereist circulatie.
             * Dit volgt dezelfde logica als v3.2.6.
             */
            if (!state.pump) {
                apply_4wire_toggle(
                    Bestway4WireToggle::Pump);
            }

            godmode_pump_off_pending_ = false;
            godmode_pump_off_due_ms_ = 0;

        } else {
            godmode_heater_stage_ = 0;
            godmode_heater2_due_ms_ = 0;
            godmode_hysteresis_ = false;

            if (spa.heater_active) {
                spa.heater_active = false;
                bump_version();
            }
        }

        ESP_LOGI(TAG,
                 "4-wire Godmode Heater request=%s",
                 on ? "on" : "off");
        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType1) {

        if (spa.heater == on) {
            ESP_LOGI(TAG,
                     "Type1 Heat al %s",
                     on ? "on" : "off");
            return;
        }

        type1_remote_sequence_ =
            on
                ? Type1RemoteSequence::HeatEnable
                : Type1RemoteSequence::HeatDisable;

        type1_remote_sequence_step_ = 0;

        ESP_LOGI(TAG,
                 "Type1 Heat sequence gestart -> %s",
                 on ? "on" : "off");

        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType2) {

        type2_start_state_action(
            BestwayButton::Heat,
            on);

        return;
    }

    ESP_LOGW(TAG,
             "hardware command heater=%s nog niet aangesloten voor %s",
             on ? "on" : "off",
             protocol_name());
}
void BestwayBus::set_unit(bool fahrenheit)
{
    if (mode_ == Mode::Simulation) {
        if (spa.fahrenheit != fahrenheit) {
            spa.fahrenheit = fahrenheit;
            type1_unit_fahrenheit_ = fahrenheit;
            bump_version();
        }
        return;
    }

    /*
     * Spa Control v3.2.6:
     * SETUNIT op een 4-wire model is alleen toegestaan wanneer
     * Godmode actief is.
     *
     * v3.2.6 rekende daarbij het target C<->F om omdat het target
     * intern in de gekozen display-eenheid werd opgeslagen.
     *
     * In deze ESP32-C6 versie blijft de interne targettemperatuur
     * altijd Celsius. Daardoor hoeven en mogen we het target hier
     * niet omrekenen.
     */
    if (model_info().family ==
        BestwayProtocolFamily::Uart4Wire) {

        if (!godmode_) {
            ESP_LOGW(
                TAG,
                "4-wire Unit wijziging geweigerd: Godmode vereist");
            return;
        }

        if (spa.fahrenheit != fahrenheit) {
            spa.fahrenheit = fahrenheit;
            bump_version();

            ESP_LOGI(
                TAG,
                "4-wire Godmode Unit=%s",
                fahrenheit ? "F" : "C");
        }

        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType1) {

        type1_start_state_action(
            BestwayButton::Unit,
            fahrenheit);

        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType2) {

        type2_start_state_action(
            BestwayButton::Unit,
            fahrenheit);

        return;
    }

    ESP_LOGW(TAG,
             "hardware command unit=%s nog niet aangesloten voor %s",
             fahrenheit ? "F" : "C",
             protocol_name());
}

void BestwayBus::set_lock(bool locked)
{
    if (mode_ == Mode::Simulation) {
        if (spa.locked != locked) {
            spa.locked = locked;
            bump_version();
        }
        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType1) {

        type1_remote_press_state(
            BestwayButton::Lock,
            locked,
            5000);

        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType2) {

        type2_start_state_action(
            BestwayButton::Lock,
            locked);

        return;
    }

    ESP_LOGW(TAG,
             "hardware command lock=%s nog niet aangesloten voor %s",
             locked ? "on" : "off",
             protocol_name());
}

void BestwayBus::press_timer()
{
    if (mode_ == Mode::Simulation) {
        spa.timer_active = !spa.timer_active;
        bump_version();
        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType1) {

        /*
         * Timer is een momentane fysieke knop.
         * Niet state-aware maken: de drie timer-leds hebben in v4
         * afzonderlijke betekenissen en vormen niet één bewezen
         * aan/uit-status.
         */
        type1_start_momentary_action(
            BestwayButton::Timer,
            800);

        ESP_LOGI(TAG, "Type1 remote Timer knop");

        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType2) {

        /*
         * Oude v4 Type2:
         * unlock() en daarna TIMER ongeveer 100 ms.
         */
        type2_start_momentary_action(
            BestwayButton::Timer,
            100);

        return;
    }

    ESP_LOGW(TAG,
             "hardware Timer knop nog niet aangesloten voor %s",
             protocol_name());
}

void BestwayBus::step_target(bool up)
{
    if (mode_ == Mode::Simulation) {
        /*
         * Simulatie blijft intern in hele graden Celsius werken.
         * Hardware Type1 gebruikt hieronder de echte pompknop.
         */
        const int target =
            spa.target_temperature_c + (up ? 1 : -1);

        set_target_temperature(target);
        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::Uart4Wire) {

        if (!godmode_) {
            ESP_LOGW(
                TAG,
                "4-wire target stap geweigerd: Godmode vereist");
            return;
        }

        const int target =
            spa.target_temperature_c + (up ? 1 : -1);

        ESP_LOGI(
            TAG,
            "4-wire Godmode target stap: %s -> %d C",
            up ? "UP" : "DOWN",
            target);

        set_target_temperature(target);
        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType1) {

        /*
         * Na een ESP-reboot is de echte pomp-target nog onbekend.
         * Stuur dan precies één echte UP/DOWN-knopdruk. De parser leert
         * daarna de werkelijke target weer van het pompdisplay.
         */
        if (!spa.target_temperature_valid) {
            type1_target_sequence_active_ = false;
            type1_target_release_active_ = false;

            type1_start_momentary_action(
                up ? BestwayButton::Up : BestwayButton::Down,
                800);

            ESP_LOGI(
                TAG,
                "Type1 target onbekend na reboot: directe %s knop",
                up ? "UP" : "DOWN");
            return;
        }

        /*
         * Zodra de echte target bekend is gebruikt Web +/- weer
         * dezelfde absolute target-sequence als MQTT en Matter.
         */
        const int target =
            spa.target_temperature_c + (up ? 1 : -1);

        ESP_LOGI(
            TAG,
            "Type1 web target stap: %s -> %d C",
            up ? "UP" : "DOWN",
            target);

        set_target_temperature(target);
        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType2) {

        // Een handmatige +/- krijgt voorrang op een lopende
        // absolute target-sequence.
        type2_target_sequence_active_ = false;
        type2_target_step_in_progress_ = false;
        type2_target_release_active_ = false;

        /*
         * Oude v4 Type2 gebruikt voor directe UP/DOWN-knoppen
         * een momentane druk van 100 ms na unlock().
         */
        type2_start_momentary_action(
            up ? BestwayButton::Up : BestwayButton::Down,
            100);

        ESP_LOGI(TAG,
                 "Type2 remote target stap: %s",
                 up ? "UP" : "DOWN");

        return;
    }

    ESP_LOGW(TAG,
             "hardware target stap %s nog niet aangesloten voor %s",
             up ? "UP" : "DOWN",
             protocol_name());
}

void BestwayBus::set_target_temperature(int target_c)
{
    if (target_c < 20) target_c = 20;
    if (target_c > 40) target_c = 40;

    if (mode_ == Mode::Simulation) {
        spa.set_target_temperature(target_c);
        // De simulator bevestigt het doel direct; hardware wacht op pompdata.
        spa.target_temperature_valid = true;
        bump_version();

        ESP_LOGI(TAG,
                 "Simulation target=%d C",
                 spa.target_temperature_c);
        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::Uart4Wire) {

        if (!godmode_) {
            ESP_LOGW(TAG,
                     "4-wire target vereist Godmode");
            return;
        }

        if (spa.target_temperature_c != target_c) {
            spa.set_target_temperature(target_c);
            bump_version();
        }

        ESP_LOGI(TAG,
                 "4-wire Godmode target=%d C",
                 target_c);
        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType1) {

        /*
         * In Fahrenheit-modus kan twee verschillende fysieke F-stappen
         * naar dezelfde afgeronde Celsiuswaarde mappen.
         *
         * Als we de echte displaytarget kennen, vergelijk daarom met
         * de ruwe pompwaarde. Anders gebruiken we Celsius als fallback.
         */
        const int requested_display_target =
            type1_unit_fahrenheit_
                ? ((target_c * 9 + 2) / 5) + 32
                : target_c;

        const bool target_already_reached =
            (type1_unit_fahrenheit_ &&
             type1_target_display_value_ >= 0)
                ? (type1_target_display_value_ ==
                   requested_display_target)
                : (spa.target_temperature_c == target_c);

        if (target_already_reached) {
            ESP_LOGI(TAG,
                     "Type1 target al bereikt: %d C / display=%d",
                     target_c,
                     requested_display_target);
            return;
        }

        type1_target_sequence_target_c_ = target_c;
        type1_target_sequence_active_ = true;
        type1_target_release_active_ = false;
        type1_target_attempts_ = 0;

        ESP_LOGI(TAG,
                 "Type1 target sequence gestart: %d -> %d C",
                 spa.target_temperature_c,
                 target_c);

        return;
    }

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType2) {

        const int requested_display_target =
            type2_unit_fahrenheit_
                ? ((target_c * 9 + 2) / 5) + 32
                : target_c;

        const bool target_already_reached =
            (type2_unit_fahrenheit_ &&
             type2_target_display_value_ >= 0)
                ? (type2_target_display_value_ ==
                   requested_display_target)
                : (spa.target_temperature_c == target_c);

        if (target_already_reached) {
            ESP_LOGI(TAG,
                     "Type2 target al bereikt: %d C / display=%d",
                     target_c,
                     requested_display_target);
            return;
        }

        type2_target_sequence_target_c_ = target_c;
        type2_target_sequence_active_ = true;
        type2_target_step_in_progress_ = false;
        type2_target_release_active_ = false;

        ESP_LOGI(TAG,
                 "Type2 target sequence gestart: %d -> %d C",
                 spa.target_temperature_c,
                 target_c);

        return;
    }

    ESP_LOGW(TAG,
             "hardware target=%d C nog niet aangesloten voor %s",
             target_c,
             protocol_name());
}


BestwayBus::ConnectorTestResult BestwayBus::run_connector_test()
{
    ConnectorTestResult result{};

    result.pairs[0].name = "data";
    result.pairs[1].name = "clock";
    result.pairs[2].name = "select";

    for (int pair = 0; pair < 3; ++pair) {
        result.pairs[pair].cio_pin = pins_[pair];
        result.pairs[pair].dsp_pin = pins_[pair + 3];
    }

    /*
     * Deze loopbacktest hoort bij de 6-wire print:
     *
     * pins[0] <-> pins[3]  Data / TD
     * pins[1] <-> pins[4]  Clock
     * pins[2] <-> pins[5]  Select / LD
     *
     * 4-wire gebruikt UART en vraagt een andere testmethode.
     */
    if (model_info().family != BestwayProtocolFamily::SixWireType1 &&
        model_info().family != BestwayProtocolFamily::SixWireType2) {

        ESP_LOGW(TAG,
                 "Connectortest niet beschikbaar voor protocol %s",
                 protocol_name());

        return result;
    }

    result.supported = true;

    ESP_LOGW(TAG,
             "Connectortest gestart: CIO %u,%u,%u <-> DSP %u,%u,%u",
             pins_[0], pins_[1], pins_[2],
             pins_[3], pins_[4], pins_[5]);

    /*
     * Eerst de gewone protocol-loop blokkeren.
     */
    connector_test_active_ = true;

    /*
     * Daarna de CIO interrupts verwijderen.
     * De globale ISR service blijft bestaan; alleen onze handlers
     * worden tijdelijk losgekoppeld.
     */
    if (model_info().family == BestwayProtocolFamily::SixWireType1) {
        const gpio_num_t clk =
            static_cast<gpio_num_t>(pins_[1]);

        const gpio_num_t cs =
            static_cast<gpio_num_t>(pins_[2]);

        gpio_set_intr_type(clk, GPIO_INTR_DISABLE);
        gpio_set_intr_type(cs, GPIO_INTR_DISABLE);

        gpio_isr_handler_remove(clk);
        gpio_isr_handler_remove(cs);

    } else {
        const gpio_num_t clk =
            static_cast<gpio_num_t>(pins_[1]);

        const gpio_num_t ld =
            static_cast<gpio_num_t>(pins_[2]);

        gpio_set_intr_type(clk, GPIO_INTR_DISABLE);
        gpio_set_intr_type(ld, GPIO_INTR_DISABLE);

        gpio_isr_handler_remove(clk);
        gpio_isr_handler_remove(ld);
    }

    /*
     * Alle zes testlijnen eerst hoogohmig maken.
     */
    for (int i = 0; i < 6; ++i) {
        const gpio_num_t pin =
            static_cast<gpio_num_t>(pins_[i]);

        gpio_set_direction(pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin, GPIO_FLOATING);
    }

    esp_rom_delay_us(2000);

    bool state = false;
    bool overall = true;

    for (int pair = 0; pair < 3; ++pair) {

        ConnectorTestPair &p = result.pairs[pair];

        const gpio_num_t cio_pin =
            static_cast<gpio_num_t>(p.cio_pin);

        const gpio_num_t dsp_pin =
            static_cast<gpio_num_t>(p.dsp_pin);

        /*
         * Zelfde pin aan beide kanten kan geen loopbacktest vormen.
         */
        if (p.cio_pin == p.dsp_pin) {
            p.applicable = false;
            p.pass = false;
            continue;
        }

        p.applicable = true;

        /*
         * Richting 1:
         * CIO -> DSP
         */
        gpio_set_direction(dsp_pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(dsp_pin, GPIO_FLOATING);

        gpio_set_level(cio_pin, 0);
        gpio_set_direction(cio_pin, GPIO_MODE_OUTPUT);

        for (int t = 0; t < 100; ++t) {
            state = !state;

            gpio_set_level(
                cio_pin,
                state ? 1 : 0);

            esp_rom_delay_us(100);

            const bool received =
                gpio_get_level(dsp_pin) != 0;

            if (received != state) {
                ++p.forward_errors;
            }
        }

        gpio_set_direction(cio_pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(cio_pin, GPIO_FLOATING);

        /*
         * Richting 2:
         * DSP -> CIO
         */
        gpio_set_level(dsp_pin, 0);
        gpio_set_direction(dsp_pin, GPIO_MODE_OUTPUT);

        for (int t = 0; t < 100; ++t) {
            state = !state;

            gpio_set_level(
                dsp_pin,
                state ? 1 : 0);

            esp_rom_delay_us(100);

            const bool received =
                gpio_get_level(cio_pin) != 0;

            if (received != state) {
                ++p.reverse_errors;
            }
        }

        gpio_set_direction(dsp_pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(dsp_pin, GPIO_FLOATING);

        p.pass =
            p.forward_errors == 0 &&
            p.reverse_errors == 0;

        if (!p.pass) {
            overall = false;
        }
    }

    /*
     * Na afloop alle zes lijnen opnieuw vrijgeven voordat
     * we de normale protocolhardware terug initialiseren.
     */
    for (int i = 0; i < 6; ++i) {
        const gpio_num_t pin =
            static_cast<gpio_num_t>(pins_[i]);

        gpio_set_direction(pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin, GPIO_FLOATING);
    }

    esp_rom_delay_us(2000);

    bool restored = false;

    if (model_info().family ==
        BestwayProtocolFamily::SixWireType1) {

        const bool cio_ok = init_type1_cio();
        const bool dsp_ok = init_type1_dsp();

        restored = cio_ok && dsp_ok;

    } else if (model_info().family ==
               BestwayProtocolFamily::SixWireType2) {

        const bool cio_ok = init_type2_cio();
        const bool dsp_ok = init_type2_dsp();

        restored = cio_ok && dsp_ok;
    }

    connector_test_active_ = false;

    result.overall_pass =
        overall && restored;

    /*
     * Na een benchtest beschouwen we de pompverbinding opnieuw als
     * onbekend. De gewone protocolcode zal hem vanzelf weer verbinden
     * zodra geldige data binnenkomt.
     */
    connected_ = false;
    spa.set_connection_state(false);

    ESP_LOGI(
        TAG,
        "Connectortest klaar: overall=%s restore=%s",
        result.overall_pass ? "GOED" : "FOUT",
        restored ? "OK" : "FOUT");

    return result;
}


void BestwayBus::simulation_loop(uint64_t now)
{
    bool changed = false;
    changed |= apply_pending(power_, spa.power, "power", now);
    changed |= apply_pending(filter_, spa.filter, "filter", now);
    changed |= apply_pending(bubbles_, spa.bubbles, "bubbles", now);
    changed |= apply_pending(jets_, spa.jets, "jets", now);
    changed |= apply_pending(heater_, spa.heater, "heater", now);

    // A heater can only be active when the spa itself and the heater request are on.
    const bool heater_active = spa.power && spa.heater && spa.temperature_c < spa.target_temperature_c;
    if (heater_active != spa.heater_active) {
        spa.heater_active = heater_active;
        changed = true;
    }

    const uint64_t step_ms = spa.heater_active ? SIM_HEAT_STEP_MS : SIM_COOL_STEP_MS;
    if (now - last_temp_step_ms_ >= step_ms) {
        last_temp_step_ms_ = now;
        const int before = spa.temperature_c;
        if (spa.heater_active && spa.temperature_c < spa.target_temperature_c) {
            ++spa.temperature_c;
        } else if (!spa.heater_active && spa.temperature_c > 25) {
            --spa.temperature_c;
        }
        if (spa.temperature_c != before) {
            changed = true;
            ESP_LOGI(TAG, "SIM pomp -> watertemperatuur %d C (doel %d C)",
                     spa.temperature_c, spa.target_temperature_c);
        }
    }

    if (now - last_packet_ms_ >= SIM_PACKET_INTERVAL_MS) {
        last_packet_ms_ = now;
        spa.mark_packet_received();
    }

    if (changed) bump_version();
}




bool BestwayBus::sync_4wire_state_from_dsp(
    bool bubbles,
    bool jets,
    bool pump,
    bool heat)
{
    if (model_info().family != BestwayProtocolFamily::Uart4Wire) {
        return false;
    }

    const uint8_t count = bestway_4wire_state_count(model_);

    for (uint8_t i = 0; i < count; ++i) {
        const Bestway4WireState state =
            bestway_4wire_state(model_, i);

        if ((state.bubbles != 0) == bubbles &&
            (state.jets != 0) == jets &&
            (state.pump != 0) == pump &&
            (state.heat != 0) == heat) {

            hw_state_index_ = i;

            ESP_LOGI(TAG,
                     "4-wire state gesynchroniseerd op DSP: state=%u bubbles=%u jets=%u pump=%u heat=%u",
                     static_cast<unsigned>(i),
                     static_cast<unsigned>(bubbles),
                     static_cast<unsigned>(jets),
                     static_cast<unsigned>(pump),
                     static_cast<unsigned>(heat));

            return true;
        }
    }

    ESP_LOGW(TAG,
             "Geen passende 4-wire state voor DSP: bubbles=%u jets=%u pump=%u heat=%u",
             static_cast<unsigned>(bubbles),
             static_cast<unsigned>(jets),
             static_cast<unsigned>(pump),
             static_cast<unsigned>(heat));

    return false;
}

void BestwayBus::apply_4wire_toggle(Bestway4WireToggle toggle)
{
    if (model_info().family != BestwayProtocolFamily::Uart4Wire) {
        return;
    }

    const uint8_t old_state = hw_state_index_;

    hw_state_index_ = bestway_4wire_next_state(
        model_,
        hw_state_index_,
        toggle);

    const Bestway4WireState state =
        bestway_4wire_state(model_, hw_state_index_);

    ESP_LOGI(TAG,
             "4-wire state %u -> %u | bubbles=%u jets=%u pump=%u heat=%u",
             static_cast<unsigned>(old_state),
             static_cast<unsigned>(hw_state_index_),
             static_cast<unsigned>(state.bubbles),
             static_cast<unsigned>(state.jets),
             static_cast<unsigned>(state.pump),
             static_cast<unsigned>(state.heat));

    if (have_dsp_frame_) {
        build_4wire_cio_frame();
    }
}



void BestwayBus::godmode_4wire_control(uint64_t now)
{
    if (!godmode_ ||
        model_info().family !=
            BestwayProtocolFamily::Uart4Wire) {
        return;
    }

    /*
     * ========================================================
     * Cooling delay
     * ========================================================
     *
     * v3.2.6:
     * als de pomp wordt uitgezet terwijl verwarming actief/
     * ingeschakeld is, eerst heater uit en pomp nog 5 s laten
     * circuleren.
     */
    if (godmode_pump_off_pending_ &&
        now >= godmode_pump_off_due_ms_) {

        godmode_pump_off_pending_ = false;
        godmode_pump_off_due_ms_ = 0;

        const Bestway4WireState state =
            bestway_4wire_state(
                model_,
                hw_state_index_);

        if (state.pump) {
            apply_4wire_toggle(
                Bestway4WireToggle::Pump);
        }

        ESP_LOGI(
            TAG,
            "4-wire Godmode cooling delay klaar -> Pump OFF");
    }

    /*
     * ========================================================
     * Antifreeze
     * ========================================================
     *
     * Exact principe uit v3.2.6:
     * onder 10 C:
     * - pomp aan
     * - heater aan
     * - target naar 10 C
     *
     * Deze toestand wordt niet automatisch teruggezet.
     */
    if (spa.temperature_c < 10) {
        Bestway4WireState state =
            bestway_4wire_state(
                model_,
                hw_state_index_);

        if (!state.pump) {
            apply_4wire_toggle(
                Bestway4WireToggle::Pump);
        }

        state =
            bestway_4wire_state(
                model_,
                hw_state_index_);

        if (!state.heat) {
            apply_4wire_toggle(
                Bestway4WireToggle::Heat);
        }

        if (!spa.heater) {
            spa.heater = true;
            bump_version();
        }

        if (spa.target_temperature_c != 10) {
            spa.set_target_temperature(10);
            bump_version();
        }

        ESP_LOGW(
            TAG,
            "4-wire Godmode antifreeze actief: temp=%d C target=10 C",
            spa.temperature_c);
    }

    /*
     * ========================================================
     * Anti-boil / overtemperature
     * ========================================================
     *
     * v3.2.6:
     * boven 41 C:
     * - pomp aan
     * - heater uit
     */
    if (spa.temperature_c > 41) {
        Bestway4WireState state =
            bestway_4wire_state(
                model_,
                hw_state_index_);

        if (!state.pump) {
            apply_4wire_toggle(
                Bestway4WireToggle::Pump);
        }

        state =
            bestway_4wire_state(
                model_,
                hw_state_index_);

        if (state.heat) {
            apply_4wire_toggle(
                Bestway4WireToggle::Heat);
        }

        if (spa.heater) {
            spa.heater = false;
            bump_version();
        }

        godmode_heater_stage_ = 0;
        godmode_heater2_due_ms_ = 0;
        godmode_hysteresis_ = false;

        if (spa.heater_active) {
            spa.heater_active = false;
            bump_version();
        }

        ESP_LOGW(
            TAG,
            "4-wire Godmode antiboil actief: temp=%d C",
            spa.temperature_c);

        return;
    }

    /*
     * ========================================================
     * Thermostaat met hysterese
     * ========================================================
     *
     * v3.2.6:
     *
     * heating:
     *   temperature + hysteresis <= target
     *
     * Zodra de temperatuur boven target komt:
     *   heater OFF
     *   hysteresis = 1
     *
     * Daarna start hij dus pas weer bij target - 1.
     */
    if (!spa.heater) {
        godmode_heater_stage_ = 0;
        godmode_heater2_due_ms_ = 0;

        if (spa.heater_active) {
            spa.heater_active = false;
            bump_version();
        }

        return;
    }

    const int hysteresis =
        godmode_hysteresis_ ? 1 : 0;

    const bool should_heat =
        (spa.temperature_c + hysteresis) <=
        spa.target_temperature_c;

    if (should_heat) {
        if (godmode_heater_stage_ == 0) {
            godmode_heater_stage_ = 1;

            // v3.2.6: tweede trap na 10 seconden.
            godmode_heater2_due_ms_ =
                now + 10000;

            ESP_LOGI(
                TAG,
                "4-wire Godmode heater stage 1 gestart");
        }

        godmode_hysteresis_ = false;

    } else {
        if (godmode_heater_stage_ != 0) {
            ESP_LOGI(
                TAG,
                "4-wire Godmode target bereikt -> heater uit");
        }

        godmode_heater_stage_ = 0;
        godmode_heater2_due_ms_ = 0;
        godmode_hysteresis_ = true;
    }

    /*
     * Tweede heatertrap.
     *
     * Alleen gebruiken wanneer dit model daadwerkelijk een
     * tweede heater-bitmask heeft.
     */
    if (godmode_heater_stage_ == 1 &&
        godmode_heater2_due_ms_ != 0 &&
        now >= godmode_heater2_due_ms_) {

        const Bestway4WireMasks masks =
            bestway_4wire_masks(model_);

        if (godmode_full_power_ &&
            masks.heat2 != 0) {

            godmode_heater_stage_ = 2;

            ESP_LOGI(
                TAG,
                "4-wire Godmode heater stage 2 gestart");
        }

        godmode_heater2_due_ms_ = 0;
    }

    const bool active =
        godmode_heater_stage_ > 0;

    if (spa.heater_active != active) {
        spa.heater_active = active;
        bump_version();
    }
}


bool BestwayBus::build_4wire_cio_frame()
{
    if (model_info().family != BestwayProtocolFamily::Uart4Wire) {
        return false;
    }

    if (!have_dsp_frame_) {
        return false;
    }

    const Bestway4WireState state =
        bestway_4wire_state(model_, hw_state_index_);

    const Bestway4WireMasks masks =
        bestway_4wire_masks(model_);

    for (size_t i = 0; i < 7; ++i) {
        to_cio_frame_[i] = last_dsp_frame_[i];
    }

    uint8_t command = 0;

    if (state.pump) {
        command |= masks.pump;
    }

    if (state.bubbles) {
        command |= masks.bubbles;
    }

    if (state.jets) {
        command |= masks.jets;
    }

    /*
     * Exact als oude v4 generatePayload():
     * alleen actieve heater-stage bits opnemen.
     */

    if (command != 0) {
        command |= masks.power;
    }

    to_cio_frame_[2] = command;

    to_cio_frame_[5] =
        static_cast<uint8_t>(
            to_cio_frame_[1] +
            to_cio_frame_[2] +
            to_cio_frame_[3] +
            to_cio_frame_[4]);

    have_to_cio_frame_ = true;

    ESP_LOGD(TAG,
             "4-wire CIO frame opgebouwd cmd=0x%02X checksum=0x%02X",
             static_cast<unsigned>(to_cio_frame_[2]),
             static_cast<unsigned>(to_cio_frame_[5]));

    return true;
}


void IRAM_ATTR BestwayBus::type2_cio_ld_isr(void *arg)
{
    auto *self = static_cast<BestwayBus *>(arg);
    if (self) {
        self->type2_cio_handle_ld_isr();
    }
}

void IRAM_ATTR BestwayBus::type2_cio_clk_isr(void *arg)
{
    auto *self = static_cast<BestwayBus *>(arg);
    if (self) {
        self->type2_cio_handle_clk_isr();
    }
}

void IRAM_ATTR BestwayBus::type2_cio_handle_ld_isr()
{
    const uint32_t clk_pin = pins_[1];
    const uint32_t ld_pin = pins_[2];

    /*
     * Type2 start/einde wordt alleen herkend wanneer LD verandert
     * terwijl CLK hoog is.
     */
    if (!gpio_ll_get_level(&GPIO, clk_pin)) {
        return;
    }

    const bool ld_high =
        gpio_ll_get_level(&GPIO, ld_pin) != 0;

    if (ld_high) {
        // LD hoog = einde packet
        type2_cio_packet_active_ = false;

        /*
         * Alleen een volledige DATAWRITE bewaren:
         * 1 command-byte + 5 payloadbytes = 48 bits.
         */
        if (type2_cio_received_cmd_ == 0xC0 &&
            type2_cio_bit_count_ >= 48) {

            portENTER_CRITICAL_ISR(&type2_cio_mux_);

            for (size_t i = 0; i < 5; ++i) {
                type2_cio_ready_payload_[i] =
                    type2_cio_payload_[i];
            }

            type2_cio_ready_brightness_ =
                type2_cio_brightness_;

            type2_cio_new_packet_ = true;

            portEXIT_CRITICAL_ISR(&type2_cio_mux_);
        }
    } else {
        // LD laag = begin packet
        type2_cio_packet_active_ = true;
    }

    type2_cio_byte_count_ = 0;
    type2_cio_bit_count_ = 0;
    type2_cio_received_cmd_ = 0;
}

void IRAM_ATTR BestwayBus::type2_cio_handle_clk_isr()
{
    if (!type2_cio_packet_active_) {
        return;
    }

    const uint32_t td_pin = pins_[0];
    const uint32_t clk_pin = pins_[1];
    const uint32_t ld_pin = pins_[2];

    const bool rising_edge =
        gpio_ll_get_level(&GPIO, clk_pin) != 0;

    const uint16_t td_bitnumber =
        static_cast<uint16_t>(type2_cio_bit_count_ % 10);

    const uint16_t ld_bitnumber =
        static_cast<uint16_t>(type2_cio_bit_count_ % 8);

    /*
     * Zelfde wrapper als oude v4:
     * startbit + buttonbits + stopbit.
     *
     * In deze eerste fase is button_code altijd NOBTN (= 0).
     */
    const uint16_t button_wrapper =
        static_cast<uint16_t>(
            (0xFEu << 8) |
            (type2_cio_button_code_ << 1));

    if (rising_edge) {
        type2_cio_byte_count_ =
            type2_cio_bit_count_ / 8;

        const uint8_t bit =
            gpio_ll_get_level(&GPIO, ld_pin) ? 1U : 0U;

        if (type2_cio_byte_count_ == 0) {
            if (bit) {
                type2_cio_received_cmd_ |=
                    static_cast<uint8_t>(1U << ld_bitnumber);
            }
        } else if (type2_cio_byte_count_ < 6 &&
                   type2_cio_received_cmd_ == 0xC0) {

            const size_t index =
                static_cast<size_t>(type2_cio_byte_count_ - 1);

            if (bit) {
                type2_cio_payload_[index] |=
                    static_cast<uint8_t>(1U << ld_bitnumber);
            } else {
                type2_cio_payload_[index] &=
                    static_cast<uint8_t>(
                        ~(1U << ld_bitnumber));
            }
        }

        /*
         * Brightness-command heeft 10xxxxxx-formaat.
         * Zelfde herkenning als v4.
         */
        if (type2_cio_bit_count_ == 7 &&
            (type2_cio_received_cmd_ & 0xC0) == 0x80) {

            type2_cio_brightness_ =
                type2_cio_received_cmd_;
        }

        ++type2_cio_bit_count_;
    } else {
        // Falling edge: antwoord richting pomp.
        const bool bit =
            (button_wrapper &
             (static_cast<uint16_t>(1U) << td_bitnumber)) != 0;

        gpio_ll_set_level(&GPIO, td_pin, bit ? 1 : 0);
    }
}

char BestwayBus::decode_type2_char(uint8_t value) const
{
    static constexpr uint8_t charcodes[] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
        0x00, 0x40, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x7D, 0x74,
        0x76, 0x30, 0x0E, 0x70, 0x38, 0x00, 0x54, 0x5C, 0x73, 0x67,
        0x50, 0x6D, 0x78, 0x1C, 0x3E, 0x00, 0x6E, 0x5B
    };

    static constexpr char chars[] = {
        '0','1','2','3','4','5','6','7','8','9',
        ' ','-','a','b','c','d','e','f','g','h',
        'H','i','j','k','l','m','n','o','p','q',
        'r','s','t','u','v','x','y','z'
    };

    static_assert(sizeof(charcodes) == sizeof(chars),
                  "Type2 charcode tabel mismatch");

    for (size_t i = 0; i < sizeof(charcodes); ++i) {
        if (value == charcodes[i]) {
            return chars[i];
        }
    }

    return '*';
}

void BestwayBus::process_type2_cio_packet()
{
    if (!type2_cio_new_packet_) {
        return;
    }

    uint8_t payload[5];
    uint8_t brightness = 7;

    portENTER_CRITICAL(&type2_cio_mux_);

    if (!type2_cio_new_packet_) {
        portEXIT_CRITICAL(&type2_cio_mux_);
        return;
    }

    for (size_t i = 0; i < 5; ++i) {
        payload[i] = type2_cio_ready_payload_[i];
    }

    brightness = type2_cio_ready_brightness_;
    type2_cio_new_packet_ = false;

    portEXIT_CRITICAL(&type2_cio_mux_);

    for (size_t i = 0; i < 5; ++i) {
        type2_cio_prev_payload_[i] = payload[i];

        // Transparante CIO -> DSP kopie.
        type2_dsp_payload_[i] = payload[i];
    }

    type2_dsp_brightness_ =
        static_cast<uint8_t>(brightness & 0x07);

    /*
     * Een volledige 5-byte DATAWRITE geldt als geldige communicatie
     * met de Type2-pomp.
     */
    last_packet_ms_ = now_ms();

    if (!connected_) {
        connected_ = true;
        spa.set_connection_state(true);
        bump_version();
        ESP_LOGI(TAG, "Type2 pomp verbonden");
    }

    spa.mark_packet_received();

    /*
     * Type2 payload exact volgens oude v4.
     *
     * byte 0..2 = displaydigits
     * byte 3    = timer/lock/heater/air
     * byte 4    = filter/C/F/power/jets
     *
     * MODEL54149E heeft AirJet en geen HydroJet.
     * Power werd in v4 voor Type2 altijd als AAN beschouwd.
     */
    const bool locked =
        (payload[3] & (1U << 5)) != 0;

    const bool timer_led2 =
        (payload[3] & (1U << 7)) != 0;

    const bool timer_led1 =
        (payload[3] & (1U << 6)) != 0;

    const bool timer_button_led =
        (payload[3] & (1U << 4)) != 0;

    const bool timer_active =
        timer_button_led || timer_led1 || timer_led2;

    const bool heat_red =
        (payload[3] & (1U << 2)) != 0;

    const bool heat_green =
        (payload[3] & (1U << 3)) != 0;

    const bool bubbles =
        (payload[3] & (1U << 1)) != 0;

    const bool pump =
        (payload[4] & (1U << 2)) != 0;

    const bool celsius_led =
        (payload[4] & (1U << 0)) != 0;

    const bool fahrenheit_led =
        (payload[4] & (1U << 1)) != 0;

    const bool power = true;
    const bool jets = false;

    const bool heater_enabled =
        heat_red || heat_green;

    const bool heater_active =
        heat_red;

    const char char1 = decode_type2_char(payload[0]);
    const char char2 = decode_type2_char(payload[1]);
    const char char3 = decode_type2_char(payload[2]);

    bool changed = false;
    int display_value = -1;

    if (char1 == 'e') {
        char error_text[3] = {char2, char3, '\0'};
        char *end = nullptr;

        const long parsed_error =
            strtol(error_text, &end, 10);

        if (end != error_text &&
            *end == '\0' &&
            parsed_error >= 0 &&
            parsed_error <= 99) {

            if (spa.error_code != static_cast<int>(parsed_error)) {
                spa.error_code =
                    static_cast<int>(parsed_error);
                changed = true;
            }
        }
    }
    else if (char1 != '*' &&
             char2 != '*' &&
             char3 != '*' &&
             char3 != 'H' &&
             char3 != ' ') {

        char temp_text[4] = {char1, char2, char3, '\0'};
        char *end = nullptr;

        const long parsed =
            strtol(temp_text, &end, 10);

        if (end != temp_text &&
            *end == '\0' &&
            parsed >= 0 &&
            parsed <= 199) {

            display_value = static_cast<int>(parsed);
        }

        if (spa.error_code != 0) {
            spa.error_code = 0;
            changed = true;
        }
    }

    /*
     * Target/current herkenning gelijk aan oude Type2 v4.
     * In deze eerste read-only fase blijft button_code NOBTN,
     * dus normaal lezen we uitsluitend de actuele temperatuur.
     */
    const uint64_t current_ms = now_ms();

    const uint16_t up_code = (1U << 0);
    const uint16_t down_code = (1U << 1);

    if (type2_cio_button_code_ == up_code ||
        type2_cio_button_code_ == down_code) {

        type2_button_release_ms_ = current_ms;

        if (power && !locked) {
            type2_capture_phase_ =
                Type2CapturePhase::ReadTarget;
        }
    }

    if (type2_button_release_ms_ != 0) {
        const uint64_t elapsed =
            current_ms - type2_button_release_ms_;

        if (elapsed > 6000) {
            type2_capture_phase_ =
                Type2CapturePhase::ReadTemperature;
        } else if (elapsed > 2000) {
            type2_capture_phase_ =
                Type2CapturePhase::Uncertain;
        }
    }

    if (spa.locked != locked) {
        spa.locked = locked;
        changed = true;
    }

    if (spa.timer_active != timer_active) {
        spa.timer_active = timer_active;
        changed = true;
    }

    if (spa.power != power) {
        spa.power = power;
        changed = true;
    }

    if (spa.filter != pump) {
        spa.filter = pump;
        changed = true;
    }

    if (spa.bubbles != bubbles) {
        spa.bubbles = bubbles;
        changed = true;
    }

    if (spa.jets != jets) {
        spa.jets = jets;
        changed = true;
    }

    if (spa.heater != heater_enabled) {
        spa.heater = heater_enabled;
        changed = true;
    }

    if (spa.heater_active != heater_active) {
        spa.heater_active = heater_active;
        changed = true;
    }

    if (celsius_led || fahrenheit_led) {
        const bool unit_fahrenheit = fahrenheit_led;

        if (type2_unit_fahrenheit_ != unit_fahrenheit) {
            type2_unit_fahrenheit_ = unit_fahrenheit;
        }

        if (spa.fahrenheit != unit_fahrenheit) {
            spa.fahrenheit = unit_fahrenheit;
            changed = true;
        }
    }

    if (display_value >= 0) {
        const int display_value_c =
            type2_unit_fahrenheit_
                ? (((display_value - 32) * 5 + 4) / 9)
                : display_value;

        if (type2_capture_phase_ ==
                Type2CapturePhase::ReadTarget) {

            // Ruwe pompwaarde bewaren voor exacte UP/DOWN-regeling,
            // vooral belangrijk in Fahrenheit-modus.
            type2_target_display_value_ = display_value;

            if (display_value_c > 19) {

                if (!spa.target_temperature_valid ||
                    display_value_c != spa.target_temperature_c) {

                    spa.target_temperature_c = display_value_c;
                    spa.target_temperature_valid = true;
                    changed = true;

                    ESP_LOGI(
                        TAG,
                        "Type1 echte pomp-target geleerd: %d C (display=%d %s)",
                        display_value_c,
                        display_value,
                        type1_unit_fahrenheit_ ? "F" : "C");
                }
            }

        } else if (type2_capture_phase_ ==
                       Type2CapturePhase::ReadTemperature) {

            if (display_value_c != spa.temperature_c) {
                spa.temperature_c = display_value_c;
                changed = true;
            }
        }
    }

    if (changed) {
        bump_version();
    }

    ESP_LOGD(TAG,
             "Type2 CIO status: power=%u filter=%u bubbles=%u heat=%u active=%u lock=%u brightness=%u",
             static_cast<unsigned>(power),
             static_cast<unsigned>(pump),
             static_cast<unsigned>(bubbles),
             static_cast<unsigned>(heater_enabled),
             static_cast<unsigned>(heater_active),
             static_cast<unsigned>(locked),
             static_cast<unsigned>(brightness & 7));
}


void IRAM_ATTR BestwayBus::type1_cio_cs_isr(void *arg)
{
    auto *self = static_cast<BestwayBus *>(arg);
    if (self) {
        self->type1_cio_handle_cs_isr();
    }
}

void IRAM_ATTR BestwayBus::type1_cio_clk_isr(void *arg)
{
    auto *self = static_cast<BestwayBus *>(arg);
    if (self) {
        self->type1_cio_handle_clk_isr();
    }
}

void IRAM_ATTR BestwayBus::type1_cio_handle_cs_isr()
{
    const uint32_t cs_pin = pins_[2];

    if (gpio_ll_get_level(&GPIO, cs_pin)) {
        // CS hoog = einde packet
        type1_cio_packet_active_ = false;

        /*
         * De Type1 CIO-bus eindigt byte-aligned.
         *
         * Op de ESP32-C6 kunnen de laatste CLK-rising-edge en CS-rising-edge
         * vrijwel tegelijk binnenkomen. Als CS als eerste wordt afgehandeld,
         * zien we dan 7 bits terwijl de achtste DATA-bit al stabiel op de
         * bus staat.
         *
         * Alleen dat heel specifieke 7-bit geval herstellen we hier.
         * We veranderen niets aan normale complete bytes of TX-responses.
         */
        if (!type1_cio_data_is_output_ &&
            type1_cio_bit_count_ == 7) {

            const uint8_t data =
                gpio_ll_get_level(&GPIO, pins_[0]) ? 1 : 0;

            type1_cio_received_byte_ =
                static_cast<uint8_t>(
                    (type1_cio_received_byte_ >> 1) |
                    (data << 7));

            ++type1_cio_bit_count_;

            if (type1_cio_bit_count_ == 8) {
                type1_cio_bit_count_ = 0;

                /*
                 * Als we midden in de 11-byte payload zaten,
                 * hoort deze herstelde byte ook nog in de payload.
                 *
                 * Voor de losse commandobytes is niets extra nodig:
                 * type1_cio_received_byte_ wordt hieronder als msg
                 * door de bestaande state-machine verwerkt.
                 */
                if (type1_cio_cmd_matches_ == 2) {
                    if (type1_cio_byte_count_ < 11) {
                        type1_cio_payload_[type1_cio_byte_count_] =
                            type1_cio_received_byte_;

                        ++type1_cio_byte_count_;
                    } else {
                        type1_cio_packet_error_ |= 4;
                    }
                }
            }
        }

        /*
         * Nu pas vastleggen hoe de transactie werkelijk eindigde,
         * dus ná eventuele recovery van de achtste bit.
         */
        const int end_byte_count = type1_cio_byte_count_;
        const int end_bit_count = type1_cio_bit_count_;
        const int end_cmd_matches = type1_cio_cmd_matches_;
        const uint8_t end_received_byte = type1_cio_received_byte_;
        const bool end_was_output = type1_cio_data_is_output_;

        type1_cio_data_is_output_ = false;

        // Normale ontvangst gebruikt alleen rising edges.
        gpio_ll_set_intr_type(
            &GPIO,
            pins_[1],
            GPIO_INTR_POSEDGE);

        // DATA weer vrijgeven richting input
        gpio_ll_output_disable(&GPIO, pins_[0]);

        uint8_t end_error = 0;

        if (end_byte_count != 11 &&
            end_byte_count != 0) {
            end_error |= 2;
        }

        if (end_bit_count != 0) {
            end_error |= 1;
        }

        if (end_error != 0) {
            /*
             * Een losse CLK-glitch buiten een herkend CIO-packet
             * (geen payloadbytes en geen command match) mag het
             * volgende geldige spa-packet niet besmetten.
             *
             * Echte fouten tijdens een herkende/payload-transactie
             * blijven gewoon als packetfout gelden.
             */
            const bool idle_glitch =
                end_byte_count == 0 &&
                end_cmd_matches == 0;

            if (!idle_glitch) {
                type1_cio_packet_error_ |= end_error;

                type1_cio_error_byte_count_ = end_byte_count;
                type1_cio_error_bit_count_ = end_bit_count;
                type1_cio_error_cmd_matches_ = end_cmd_matches;
                type1_cio_error_received_byte_ = end_received_byte;
                type1_cio_error_was_output_ = end_was_output;
            }
        }

        type1_cio_byte_count_ = 0;
        type1_cio_bit_count_ = 0;

        const uint8_t msg = type1_cio_received_byte_;

        switch (msg) {
            case 0x01:
            case 0x05:
                type1_cio_cmd_matches_ = 1;
                break;

            case 0x40:
                if (type1_cio_cmd_matches_ == 1) {
                    type1_cio_cmd_matches_ = 2;
                } else {
                    type1_cio_cmd_matches_ = 0;
                }
                break;

            default:
                if (type1_cio_cmd_matches_ == 3) {
                    type1_cio_brightness_ = msg;
                    type1_cio_cmd_matches_ = 0;

                    // Maak hier in de ISR één complete snapshot.
                    // Dezelfde spinlock wordt gebruikt door de task.
                    portENTER_CRITICAL_ISR(&type1_cio_mux_);

                    for (size_t i = 0; i < 11; ++i) {
                        type1_cio_ready_payload_[i] =
                            type1_cio_payload_[i];
                    }

                    type1_cio_ready_brightness_ =
                        type1_cio_brightness_;

                    type1_cio_ready_packet_error_ =
                        type1_cio_packet_error_;

                    type1_cio_ready_error_byte_count_ =
                        type1_cio_error_byte_count_;
                    type1_cio_ready_error_bit_count_ =
                        type1_cio_error_bit_count_;
                    type1_cio_ready_error_cmd_matches_ =
                        type1_cio_error_cmd_matches_;
                    type1_cio_ready_error_received_byte_ =
                        type1_cio_error_received_byte_;
                    type1_cio_ready_error_was_output_ =
                        type1_cio_error_was_output_;

                    type1_cio_packet_error_ = 0;
                    type1_cio_new_packet_ = true;

                    portEXIT_CRITICAL_ISR(&type1_cio_mux_);
                }

                if (type1_cio_cmd_matches_ == 2) {
                    type1_cio_cmd_matches_ = 3;
                }
                break;
        }
    } else {
        // CS laag = begin packet
        type1_cio_packet_active_ = true;
    }
}

void IRAM_ATTR BestwayBus::type1_cio_handle_clk_isr()
{
    if (!type1_cio_packet_active_) {
        return;
    }

    const uint32_t data_pin = pins_[0];
    const uint32_t clk_pin = pins_[1];

    const bool clockstate =
        gpio_ll_get_level(&GPIO, clk_pin) != 0;

    // Falling edge: 16-bit buttoncode uitsturen
    if (!clockstate && type1_cio_data_is_output_) {
        const bool bit =
            (type1_cio_tx_button_code_ &
             (static_cast<uint16_t>(1U) << type1_cio_send_bit_)) != 0;

        gpio_ll_set_level(&GPIO, data_pin, bit ? 1 : 0);

        ++type1_cio_send_bit_;
        if (type1_cio_send_bit_ > 15) {
            type1_cio_send_bit_ = 0;
        }
    }

    // Rising edge: data inlezen
    if (clockstate && !type1_cio_data_is_output_) {
        const uint8_t data =
            gpio_ll_get_level(&GPIO, data_pin) ? 1 : 0;

        type1_cio_received_byte_ =
            static_cast<uint8_t>(
                (type1_cio_received_byte_ >> 1) |
                (data << 7));

        ++type1_cio_bit_count_;

        if (type1_cio_bit_count_ == 8) {
            type1_cio_bit_count_ = 0;

            if (type1_cio_cmd_matches_ == 2) {
                if (type1_cio_byte_count_ < 11) {
                    type1_cio_payload_[type1_cio_byte_count_] =
                        type1_cio_received_byte_;
                    ++type1_cio_byte_count_;
                } else {
                    type1_cio_packet_error_ |= 4;
                }
            }
            else if (type1_cio_received_byte_ == 0x42) {
                // Eén atomische snapshot voor deze complete
                // 16-bit button-response.
                type1_cio_tx_button_code_ =
                    type1_cio_button_code_;

                type1_cio_send_bit_ = 8;
                type1_cio_data_is_output_ = true;

                // Tijdens de 16-bit button-response alleen falling edges nodig.
                gpio_ll_set_intr_type(
                    &GPIO,
                    clk_pin,
                    GPIO_INTR_NEGEDGE);

                gpio_ll_output_enable(&GPIO, data_pin);
            }
        }
    }
}





void BestwayBus::type2_dsp_send_bits(
    uint32_t bits,
    int count)
{
    /*
     * Type2 displayprotocol uit oude v4:
     * - CLK idle HIGH
     * - data op LD
     * - LSB first
     * - ongeveer 50 us per halve klokperiode
     */
    const gpio_num_t ld_pin =
        static_cast<gpio_num_t>(pins_[5]);

    const gpio_num_t clk_pin =
        static_cast<gpio_num_t>(pins_[4]);

    for (int i = 0; i < count; ++i) {
        gpio_set_level(clk_pin, 0);
        esp_rom_delay_us(5);

        gpio_set_level(
            ld_pin,
            (bits & (1UL << i)) ? 1 : 0);

        esp_rom_delay_us(45);

        gpio_set_level(clk_pin, 1);
        esp_rom_delay_us(50);
    }
}


bool BestwayBus::init_type2_dsp()
{
    if (model_info().family !=
        BestwayProtocolFamily::SixWireType2) {
        return false;
    }

    /*
     * Oude Type2 DSP pinvolgorde:
     * TD, CLK, LD, AUDIO
     *
     * Onze hardwareconfig:
     * pins_[3], pins_[4], pins_[5], pins_[6]
     */
    const gpio_num_t td_pin =
        static_cast<gpio_num_t>(pins_[3]);

    const gpio_num_t clk_pin =
        static_cast<gpio_num_t>(pins_[4]);

    const gpio_num_t ld_pin =
        static_cast<gpio_num_t>(pins_[5]);

    const gpio_num_t audio_pin =
        static_cast<gpio_num_t>(pins_[6]);

    gpio_config_t io = {};

    // TD komt vanaf het originele display en is dus input.
    io.pin_bit_mask = 1ULL << td_pin;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(TAG,
                 "Type2 DSP TD GPIO configuratie mislukt");
        return false;
    }

    // CLK + LD + AUDIO worden door de ESP aangestuurd.
    io = {};
    io.pin_bit_mask =
        (1ULL << clk_pin) |
        (1ULL << ld_pin) |
        (1ULL << audio_pin);

    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(TAG,
                 "Type2 DSP CLK/LD/AUDIO configuratie mislukt");
        return false;
    }

    // Zelfde idle states als oude v4.
    gpio_set_level(ld_pin, 1);
    gpio_set_level(clk_pin, 1);
    gpio_set_level(audio_pin, 0);

    type2_dsp_last_refresh_ms_ = 0;
    type2_dsp_last_button_ms_ = 0;
    type2_dsp_brightness_ = 7;

    type2_dsp_button_code_ =
        bestway_type2_button_code(
            model_,
            BestwayButton::NoButton);

    type2_dsp_old_button_ =
        BestwayButton::NoButton;

    type2_dsp_prev_button_ =
        BestwayButton::NoButton;

    type2_cio_button_code_ =
        type2_dsp_button_code_;

    type2_remote_button_active_ = false;
    type2_remote_button_code_ = 0;
    type2_remote_button_until_ms_ = 0;
    type2_remote_button_ =
        BestwayButton::NoButton;
    type2_remote_wait_for_state_ = false;
    type2_remote_target_value_ = false;

    type2_remote_sequence_active_ = false;
    type2_remote_sequence_step_ = 0;
    type2_remote_sequence_button_ =
        BestwayButton::NoButton;
    type2_remote_sequence_target_value_ = false;
    type2_remote_sequence_momentary_ = false;
    type2_remote_sequence_duration_ms_ = 100;

    type2_target_sequence_active_ = false;
    type2_target_sequence_target_c_ = 0;
    type2_target_display_value_ = -1;
    type2_target_step_in_progress_ = false;
    type2_target_release_active_ = false;
    type2_target_release_until_ms_ = 0;

    for (auto &b : type2_dsp_payload_) {
        b = 0;
    }

    ESP_LOGI(TAG,
             "6-wire Type2 DSP klaar: TD=%u CLK=%u LD=%u AUDIO=%u",
             static_cast<unsigned>(td_pin),
             static_cast<unsigned>(clk_pin),
             static_cast<unsigned>(ld_pin),
             static_cast<unsigned>(audio_pin));

    return true;
}


void BestwayBus::type2_dsp_poll_button(
    uint64_t now_ms)
{
    /*
     * Exact dezelfde pollfrequentie als oude v4 Type2:
     * maximaal één uitlezing per 20 ms.
     */
    if ((now_ms - type2_dsp_last_button_ms_) < 20) {
        return;
    }

    type2_dsp_last_button_ms_ = now_ms;

    const gpio_num_t td_pin =
        static_cast<gpio_num_t>(pins_[3]);

    const gpio_num_t clk_pin =
        static_cast<gpio_num_t>(pins_[4]);

    uint16_t new_button_code = 0;

    /*
     * Oude v4 Type2 buttonprotocol:
     *
     * startbit
     * 8 databits, LSB first
     * stopbit
     *
     * TD wordt door het display geleverd, CLK door de ESP.
     */

    // Startbit
    gpio_set_level(clk_pin, 0);
    esp_rom_delay_us(50);

    gpio_set_level(clk_pin, 1);
    esp_rom_delay_us(50);

    // 8 databits
    for (int i = 0; i < 8; ++i) {
        gpio_set_level(clk_pin, 0);
        esp_rom_delay_us(50);

        gpio_set_level(clk_pin, 1);

        if (gpio_get_level(td_pin)) {
            new_button_code |=
                static_cast<uint16_t>(1U << i);
        }

        esp_rom_delay_us(50);
    }

    // Stopbit
    gpio_set_level(clk_pin, 0);
    esp_rom_delay_us(50);

    gpio_set_level(clk_pin, 1);
    esp_rom_delay_us(50);

    /*
     * Eerst naar onze centrale Type2 button-enum vertalen.
     *
     * Bij een onbekende code behandelen we hem als NOBTN zodat een
     * beschadigde uitlezing nooit een knop richting pomp laat hangen.
     */
    BestwayButton new_button =
        BestwayButton::NoButton;

    if (!bestway_type2_decode_button(
            model_,
            new_button_code,
            new_button)) {

        new_button =
            BestwayButton::NoButton;
    }

    /*
     * Zelfde glitchfilter als oude v4:
     * alleen accepteren wanneer twee opeenvolgende decodes gelijk zijn.
     */
    if (new_button == type2_dsp_prev_button_) {
        type2_dsp_old_button_ = new_button;
    } else {
        type2_dsp_prev_button_ = new_button;
    }

    type2_dsp_button_code_ =
        bestway_type2_button_code(
            model_,
            type2_dsp_old_button_);

    /*
     * Transparante fysieke display -> pomp doorvoer.
     *
     * Tijdens een remote actie heeft de virtuele knop tijdelijk
     * voorrang. Zodra die loslaat neemt de fysieke knop het weer over.
     */
    if (!type2_remote_button_active_) {
        type2_cio_button_code_ =
            type2_dsp_button_code_;
    }
}


void BestwayBus::type2_dsp_upload_payload(
    uint64_t now_ms)
{
    /*
     * Oude v4 ververst Type2 ongeveer iedere 100 ms.
     */
    if ((now_ms - type2_dsp_last_refresh_ms_) < 100) {
        return;
    }

    type2_dsp_last_refresh_ms_ = now_ms;

    const gpio_num_t clk_pin =
        static_cast<gpio_num_t>(pins_[4]);

    const gpio_num_t ld_pin =
        static_cast<gpio_num_t>(pins_[5]);

    uint8_t brightness =
        static_cast<uint8_t>(
            type2_dsp_brightness_ & 0x07);

    uint8_t enable_led = 0;

    if (brightness > 0) {
        // DSP_DIM_ON
        enable_led = 0x08;
        --brightness;
    }

    /*
     * Packet 1:
     * CMD1 = 0x40, normal mode / auto-increment.
     */
    gpio_set_level(ld_pin, 0);
    esp_rom_delay_us(50);

    type2_dsp_send_bits(0x40, 8);

    // STOP-conditie: CLK hoog, daarna LD hoog.
    gpio_set_level(clk_pin, 0);
    gpio_set_level(ld_pin, 0);
    esp_rom_delay_us(50);

    gpio_set_level(clk_pin, 1);
    esp_rom_delay_us(50);

    gpio_set_level(ld_pin, 1);
    esp_rom_delay_us(50);

    /*
     * Packet 2:
     * CMD2 = 0xC0 gevolgd door 5-byte displaypayload.
     */
    gpio_set_level(ld_pin, 0);
    esp_rom_delay_us(50);

    type2_dsp_send_bits(0xC0, 8);

    for (size_t i = 0; i < 5; ++i) {
        type2_dsp_send_bits(
            type2_dsp_payload_[i],
            8);
    }

    gpio_set_level(clk_pin, 0);
    gpio_set_level(ld_pin, 0);
    esp_rom_delay_us(50);

    gpio_set_level(clk_pin, 1);
    esp_rom_delay_us(50);

    gpio_set_level(ld_pin, 1);
    esp_rom_delay_us(50);

    /*
     * Packet 3:
     * brightness/display-enable.
     */
    gpio_set_level(ld_pin, 0);
    esp_rom_delay_us(50);

    type2_dsp_send_bits(
        static_cast<uint8_t>(
            0x80 |
            enable_led |
            brightness),
        8);

    gpio_set_level(clk_pin, 0);
    gpio_set_level(ld_pin, 0);
    esp_rom_delay_us(50);

    gpio_set_level(clk_pin, 1);
    esp_rom_delay_us(50);

    gpio_set_level(ld_pin, 1);
    esp_rom_delay_us(50);
}



void BestwayBus::type1_dsp_send_bits(uint32_t bits, int count)
{
    const gpio_num_t data_pin = static_cast<gpio_num_t>(pins_[3]);
    const gpio_num_t clk_pin  = static_cast<gpio_num_t>(pins_[4]);

    gpio_set_direction(data_pin, GPIO_MODE_OUTPUT);

    esp_rom_delay_us(20);

    for (int i = 0; i < count; ++i) {
        gpio_set_level(clk_pin, 0);

        gpio_set_level(
            data_pin,
            (bits & (1UL << i)) ? 1 : 0);

        esp_rom_delay_us(20);

        gpio_set_level(clk_pin, 1);

        esp_rom_delay_us(20);
    }
}

uint16_t BestwayBus::type1_dsp_receive_bits()
{
    const gpio_num_t data_pin = static_cast<gpio_num_t>(pins_[3]);
    const gpio_num_t clk_pin  = static_cast<gpio_num_t>(pins_[4]);

    uint16_t result = 0;

    gpio_set_direction(data_pin, GPIO_MODE_INPUT);

    for (int i = 0; i < 16; ++i) {
        gpio_set_level(clk_pin, 0);
        esp_rom_delay_us(20);

        gpio_set_level(clk_pin, 1);
        esp_rom_delay_us(20);

        const int j = (i + 8) % 16;

        if (gpio_get_level(data_pin)) {
            result |= static_cast<uint16_t>(1U << j);
        }
    }

    return result;
}


void BestwayBus::type1_audio_set_frequency(int frequency_hz)
{
    if (!type1_audio_ready_) {
        return;
    }

    if (frequency_hz == type1_audio_frequency_hz_) {
        return;
    }

    if (frequency_hz <= 0) {
        ledc_set_duty(
            LEDC_LOW_SPEED_MODE,
            LEDC_CHANNEL_0,
            0);

        ledc_update_duty(
            LEDC_LOW_SPEED_MODE,
            LEDC_CHANNEL_0);

        type1_audio_frequency_hz_ = 0;
        return;
    }

    esp_err_t err = ledc_set_freq(
        LEDC_LOW_SPEED_MODE,
        LEDC_TIMER_0,
        static_cast<uint32_t>(frequency_hz));

    if (err != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Type1 AUDIO frequentie %d Hz instellen mislukt: %s",
            frequency_hz,
            esp_err_to_name(err));
        return;
    }

    ledc_set_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0,
        512);

    ledc_update_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0);

    type1_audio_frequency_hz_ = frequency_hz;
}


void BestwayBus::type1_audio_start_temp_sweep(bool up)
{
    if (!type1_audio_ready_) {
        return;
    }

    type1_audio_sweep_up_ = up;
    type1_audio_sweep_started_us_ = esp_timer_get_time();
    type1_audio_sweep_active_ = true;

    /*
     * Oude v4:
     *
     * UP:
     *   128 x 2 ms
     *   werkelijk afgespeeld van circa 984 -> 2000 Hz
     *
     * DOWN:
     *   128 x 2 ms
     *   werkelijk afgespeeld van circa 2016 -> 1000 Hz
     */
    type1_audio_set_frequency(up ? 984 : 2016);

    ESP_LOGI(
        TAG,
        "Type1 AUDIO temp %s sweep gestart",
        up ? "UP" : "DOWN");
}


void BestwayBus::type1_audio_start_beep()
{
    if (!type1_audio_ready_) {
        return;
    }

    type1_audio_sweep_active_ = false;
    type1_audio_beep_active_ = true;
    type1_audio_beep_until_us_ =
        esp_timer_get_time() + 100000ULL;

    type1_audio_set_frequency(2000);
}


void BestwayBus::type1_audio_loop()
{
    const uint64_t now_us = esp_timer_get_time();

    if (type1_audio_beep_active_) {
        if (now_us >= type1_audio_beep_until_us_) {
            type1_audio_beep_active_ = false;
            type1_audio_set_frequency(0);
        }

        return;
    }

    if (!type1_audio_sweep_active_) {
        return;
    }

    const uint64_t elapsed_us =
        now_us - type1_audio_sweep_started_us_;

    /*
     * 128 stappen * 2 ms = 256 ms.
     */
    if (elapsed_us >= 256000ULL) {
        type1_audio_sweep_active_ = false;
        type1_audio_set_frequency(0);
        return;
    }

    int step = static_cast<int>(elapsed_us / 2000ULL);

    if (step < 0) {
        step = 0;
    }

    if (step > 127) {
        step = 127;
    }

    int frequency_hz;

    if (type1_audio_sweep_up_) {
        /*
         * Exact dezelfde hoorbare richting als oude _sweepup():
         * 984, 992, 1000 ... 2000 Hz
         */
        frequency_hz = 984 + (8 * step);
    } else {
        /*
         * Exact dezelfde hoorbare richting als oude _sweepdown():
         * 2016, 2008, 2000 ... 1000 Hz
         */
        frequency_hz = 2016 - (8 * step);
    }

    type1_audio_set_frequency(frequency_hz);
}


bool BestwayBus::init_type1_dsp()
{
    if (model_info().family != BestwayProtocolFamily::SixWireType1) {
        return false;
    }

    const gpio_num_t data_pin  = static_cast<gpio_num_t>(pins_[3]);
    const gpio_num_t clk_pin   = static_cast<gpio_num_t>(pins_[4]);
    const gpio_num_t cs_pin    = static_cast<gpio_num_t>(pins_[5]);
    const gpio_num_t audio_pin = static_cast<gpio_num_t>(pins_[6]);

    gpio_config_t io = {};

    // DATA start als input
    io.pin_bit_mask = 1ULL << data_pin;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(TAG, "Type1 DSP DATA GPIO configuratie mislukt");
        return false;
    }

    // CLK + CS + AUDIO als outputs
    io = {};
    io.pin_bit_mask =
        (1ULL << clk_pin) |
        (1ULL << cs_pin) |
        (1ULL << audio_pin);
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(TAG, "Type1 DSP CLK/CS/AUDIO configuratie mislukt");
        return false;
    }

    // Zelfde idle states als oude v4
    gpio_set_level(cs_pin, 1);     // CS active-low
    gpio_set_level(clk_pin, 1);    // idle high
    gpio_set_level(audio_pin, 0);

    /*
     * Type1 AUDIO via LEDC.
     * Zelfde principe als oude v4 tone()/noTone(), maar nu ESP-IDF.
     */
    ledc_timer_config_t audio_timer = {};
    audio_timer.speed_mode = LEDC_LOW_SPEED_MODE;
    audio_timer.duty_resolution = LEDC_TIMER_10_BIT;
    audio_timer.timer_num = LEDC_TIMER_0;
    audio_timer.freq_hz = 2000;
    audio_timer.clk_cfg = LEDC_AUTO_CLK;

    esp_err_t audio_err = ledc_timer_config(&audio_timer);

    if (audio_err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Type1 AUDIO LEDC timer configuratie mislukt: %s",
            esp_err_to_name(audio_err));
        type1_audio_ready_ = false;
    } else {
        ledc_channel_config_t audio_channel = {};
        audio_channel.gpio_num = audio_pin;
        audio_channel.speed_mode = LEDC_LOW_SPEED_MODE;
        audio_channel.channel = LEDC_CHANNEL_0;
        audio_channel.intr_type = LEDC_INTR_DISABLE;
        audio_channel.timer_sel = LEDC_TIMER_0;
        audio_channel.duty = 0;
        audio_channel.hpoint = 0;

        audio_err = ledc_channel_config(&audio_channel);

        if (audio_err != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Type1 AUDIO LEDC kanaal configuratie mislukt: %s",
                esp_err_to_name(audio_err));
            type1_audio_ready_ = false;
        } else {
            type1_audio_ready_ = true;
            type1_audio_frequency_hz_ = 0;

            ESP_LOGI(
                TAG,
                "Type1 AUDIO klaar op GPIO=%u",
                static_cast<unsigned>(audio_pin));
        }
    }

    type1_dsp_last_refresh_ms_ = 0;
    type1_dsp_last_button_ms_ = 0;
    type1_dsp_button_code_ = 0;
    type1_dsp_brightness_ = 7;

    for (auto &b : type1_dsp_payload_) {
        b = 0;
    }

    ESP_LOGI(TAG,
             "6-wire Type1 DSP klaar: DATA=%u CLK=%u CS=%u AUDIO=%u",
             static_cast<unsigned>(data_pin),
             static_cast<unsigned>(clk_pin),
             static_cast<unsigned>(cs_pin),
             static_cast<unsigned>(audio_pin));

    return true;
}




void BestwayBus::type2_start_state_action(
    BestwayButton button,
    bool target_value)
{
    if (model_info().family !=
        BestwayProtocolFamily::SixWireType2) {
        return;
    }

    /*
     * Nooit een lopende Type2-knop/sequentie halverwege
     * overschrijven. De pomp blijft master en één fysieke
     * knopactie wordt eerst volledig afgehandeld.
     */
    if (type2_remote_sequence_active_ ||
        type2_remote_button_active_) {

        ESP_LOGW(TAG,
                 "Type2 state-opdracht genegeerd: andere actie loopt nog");
        return;
    }

    /*
     * Lock zelf hoeft uiteraard niet eerst te unlocken.
     */
    if (button == BestwayButton::Lock) {
        type2_remote_press_state(
            button,
            target_value,
            5000);
        return;
    }

    /*
     * Oude v4 riep unlock() aan vóór UNIT, BUBBLES,
     * HEAT, PUMP en HYDROJETS.
     *
     * Bij Type2 is power altijd als AAN beschouwd,
     * dus de relevante stap is hier alleen LOCK uit.
     */
    type2_remote_sequence_active_ = true;
    type2_remote_sequence_step_ = 0;
    type2_remote_sequence_button_ = button;
    type2_remote_sequence_target_value_ = target_value;
    type2_remote_sequence_momentary_ = false;
    type2_remote_sequence_duration_ms_ = 100;

    ESP_LOGI(TAG,
             "Type2 sequence gestart knop=%u doel=%s",
             static_cast<unsigned>(button),
             target_value ? "on" : "off");
}


void BestwayBus::type2_start_momentary_action(
    BestwayButton button,
    uint32_t duration_ms)
{
    if (model_info().family !=
        BestwayProtocolFamily::SixWireType2) {
        return;
    }

    if (type2_remote_sequence_active_ ||
        type2_remote_button_active_) {

        ESP_LOGW(TAG,
                 "Type2 momentary-opdracht genegeerd: andere actie loopt nog");
        return;
    }

    type2_remote_sequence_active_ = true;
    type2_remote_sequence_step_ = 0;
    type2_remote_sequence_button_ = button;
    type2_remote_sequence_target_value_ = false;
    type2_remote_sequence_momentary_ = true;
    type2_remote_sequence_duration_ms_ = duration_ms;

    ESP_LOGI(TAG,
             "Type2 momentary sequence gestart knop=%u duur=%u ms",
             static_cast<unsigned>(button),
             static_cast<unsigned>(duration_ms));
}


void BestwayBus::type2_remote_sequence_loop(
    uint64_t now)
{
    (void)now;

    if (!type2_remote_sequence_active_) {
        return;
    }

    /*
     * Eerst wachten totdat een lopende knopdruk volledig
     * is afgehandeld.
     */
    if (type2_remote_button_active_) {
        return;
    }

    switch (type2_remote_sequence_step_) {

        case 0:
            /*
             * Eerst unlocken als de pomp/display gelocked is.
             */
            if (spa.locked) {
                type2_remote_sequence_step_ = 1;

                type2_remote_press_state(
                    BestwayButton::Lock,
                    false,
                    5000);

                ESP_LOGI(TAG,
                         "Type2 sequence: eerst unlock");
                return;
            }

            type2_remote_sequence_step_ = 2;
            break;

        case 1:
            /*
             * De unlock-knop is inmiddels losgelaten.
             * Als LOCK nog steeds actief is, is de actie
             * blijkbaar niet bevestigd binnen de timeout.
             */
            if (spa.locked) {
                ESP_LOGW(TAG,
                         "Type2 sequence afgebroken: unlock niet bevestigd");

                type2_remote_sequence_active_ = false;
                type2_remote_sequence_step_ = 0;
                type2_remote_sequence_button_ =
                    BestwayButton::NoButton;
                return;
            }

            type2_remote_sequence_step_ = 2;
            break;

        case 2:
            /*
             * Nu pas de werkelijk gevraagde functie bedienen.
             */
            type2_remote_sequence_step_ = 3;

            if (type2_remote_sequence_momentary_) {
                type2_remote_press(
                    type2_remote_sequence_button_,
                    type2_remote_sequence_duration_ms_);
            } else {
                type2_remote_press_state(
                    type2_remote_sequence_button_,
                    type2_remote_sequence_target_value_,
                    5000);
            }

            return;

        case 3: {
            /*
             * Een momentane knop heeft geen afzonderlijke doelstatus.
             * Zodra de knopdruk klaar is, is de sequence klaar.
             */
            if (type2_remote_sequence_momentary_) {
                ESP_LOGI(TAG,
                         "Type2 momentary sequence klaar knop=%u",
                         static_cast<unsigned>(
                             type2_remote_sequence_button_));

                type2_remote_sequence_active_ = false;
                type2_remote_sequence_step_ = 0;
                type2_remote_sequence_button_ =
                    BestwayButton::NoButton;
                type2_remote_sequence_momentary_ = false;
                return;
            }

            /*
             * State-aware actie is klaar of heeft zijn timeout bereikt.
             * Alleen nog controleren wat de pomp werkelijk meldt.
             */
            bool current_value = false;
            bool known = true;

            switch (type2_remote_sequence_button_) {
                case BestwayButton::Pump:
                    current_value = spa.filter;
                    break;

                case BestwayButton::Bubbles:
                    current_value = spa.bubbles;
                    break;

                case BestwayButton::Heat:
                    current_value = spa.heater;
                    break;

                case BestwayButton::Unit:
                    current_value = spa.fahrenheit;
                    break;

                case BestwayButton::HydroJets:
                    current_value = spa.jets;
                    break;

                default:
                    known = false;
                    break;
            }

            if (known &&
                current_value ==
                    type2_remote_sequence_target_value_) {

                ESP_LOGI(TAG,
                         "Type2 sequence klaar knop=%u",
                         static_cast<unsigned>(
                             type2_remote_sequence_button_));
            } else {
                ESP_LOGW(TAG,
                         "Type2 sequence klaar zonder bevestigde doelstatus knop=%u",
                         static_cast<unsigned>(
                             type2_remote_sequence_button_));
            }

            type2_remote_sequence_active_ = false;
            type2_remote_sequence_step_ = 0;
            type2_remote_sequence_button_ =
                BestwayButton::NoButton;
            return;
        }

        default:
            type2_remote_sequence_active_ = false;
            type2_remote_sequence_step_ = 0;
            type2_remote_sequence_button_ =
                BestwayButton::NoButton;
            return;
    }
}


void BestwayBus::type2_target_sequence_loop(
    uint64_t now)
{
    if (!type2_target_sequence_active_) {
        return;
    }

    /*
     * Als de vorige UP/DOWN-stap nog via de algemene Type2
     * remote-sequence loopt, eerst wachten tot die helemaal klaar is.
     */
    if (type2_target_step_in_progress_) {

        if (type2_remote_sequence_active_ ||
            type2_remote_button_active_) {
            return;
        }

        /*
         * Oude v4 gebruikt na een targetknop ongeveer 400 ms NOBTN.
         * Daardoor krijgt de pomp/display tijd om de nieuwe target
         * terug te melden voordat we opnieuw beslissen.
         */
        type2_target_step_in_progress_ = false;
        type2_target_release_active_ = true;
        type2_target_release_until_ms_ =
            now + 400;

        return;
    }

    if (type2_target_release_active_) {
        if (now < type2_target_release_until_ms_) {
            return;
        }

        type2_target_release_active_ = false;
    }

    /*
     * Geen nieuwe targetstap starten zolang een andere Type2
     * remote-actie nog bezig is.
     */
    if (type2_remote_sequence_active_ ||
        type2_remote_button_active_) {
        return;
    }

    const int requested_display_target =
        type2_unit_fahrenheit_
            ? ((type2_target_sequence_target_c_ * 9 + 2) / 5) + 32
            : type2_target_sequence_target_c_;

    /*
     * In Fahrenheit vergelijken we bij voorkeur de ruwe displaywaarde,
     * omdat twee naburige F-waarden naar dezelfde afgeronde C-waarde
     * kunnen converteren.
     */
    const bool target_reached =
        (type2_unit_fahrenheit_ &&
         type2_target_display_value_ >= 0)
            ? (type2_target_display_value_ ==
               requested_display_target)
            : (spa.target_temperature_c ==
               type2_target_sequence_target_c_);

    if (target_reached) {
        type2_target_sequence_active_ = false;
        type2_target_step_in_progress_ = false;
        type2_target_release_active_ = false;

        ESP_LOGI(TAG,
                 "Type2 target bereikt: display=%d %s (%d C)",
                 type2_target_display_value_,
                 type2_unit_fahrenheit_ ? "F" : "C",
                 type2_target_sequence_target_c_);

        return;
    }

    BestwayButton direction;

    if (type2_target_display_value_ >= 0) {
        direction =
            requested_display_target >
                    type2_target_display_value_
                ? BestwayButton::Up
                : BestwayButton::Down;
    } else {
        direction =
            type2_target_sequence_target_c_ >
                    spa.target_temperature_c
                ? BestwayButton::Up
                : BestwayButton::Down;
    }

    /*
     * Dit is bewust 800 ms en niet de 100 ms van een losse web +/-
     * druk. De oude v4 target-regeling gebruikte 800 ms per stap,
     * gevolgd door 400 ms release.
     */
    type2_start_momentary_action(
        direction,
        800);

    type2_target_step_in_progress_ = true;

    ESP_LOGI(TAG,
             "Type2 target stap %s -> doel %d C / display %d",
             direction == BestwayButton::Up ? "UP" : "DOWN",
             type2_target_sequence_target_c_,
             requested_display_target);
}


void BestwayBus::type2_remote_press(
    BestwayButton button,
    uint32_t duration_ms)
{
    if (model_info().family !=
        BestwayProtocolFamily::SixWireType2) {
        return;
    }

    const uint16_t code =
        bestway_type2_button_code(
            model_,
            button);

    /*
     * Type2 gebruikt 0x0000 als geldige NOBTN-code.
     * Alle echte knoppen moeten daarom een niet-nul code hebben.
     */
    if (code == 0 &&
        button != BestwayButton::NoButton) {

        ESP_LOGW(TAG,
                 "Type2 remote knop niet beschikbaar voor model %s",
                 model_name());
        return;
    }

    type2_remote_button_code_ = code;
    type2_remote_button_active_ = true;

    type2_remote_button_until_ms_ =
        now_ms() + duration_ms;

    // CIO antwoordt tijdens de remote actie met deze knopcode.
    type2_cio_button_code_ = code;

    ESP_LOGI(TAG,
             "Type2 remote knop index=%u code=0x%04X duur=%u ms",
             static_cast<unsigned>(button),
             static_cast<unsigned>(code),
             static_cast<unsigned>(duration_ms));
}


void BestwayBus::type2_remote_press_state(
    BestwayButton button,
    bool target_value,
    uint32_t max_duration_ms)
{
    bool current_value = false;
    bool supported = true;

    switch (button) {
        case BestwayButton::Power:
            current_value = spa.power;
            break;

        case BestwayButton::Pump:
            current_value = spa.filter;
            break;

        case BestwayButton::Bubbles:
            current_value = spa.bubbles;
            break;

        case BestwayButton::HydroJets:
            current_value = spa.jets;
            break;

        case BestwayButton::Heat:
            current_value = spa.heater;
            break;

        case BestwayButton::Lock:
            current_value = spa.locked;
            break;

        case BestwayButton::Unit:
            current_value = spa.fahrenheit;
            break;

        default:
            supported = false;
            break;
    }

    if (!supported) {
        ESP_LOGW(TAG,
                 "Type2 state-aware knop index=%u niet ondersteund",
                 static_cast<unsigned>(button));
        return;
    }

    if (current_value == target_value) {
        ESP_LOGI(TAG,
                 "Type2 remote knop index=%u: gewenste status al bereikt",
                 static_cast<unsigned>(button));
        return;
    }

    /*
     * Controleer eerst of deze knop werkelijk voor dit Type2-model
     * beschikbaar is. Zo laten we geen state-aware vlaggen achter
     * wanneer type2_remote_press() de knop zou weigeren.
     */
    const uint16_t code =
        bestway_type2_button_code(
            model_,
            button);

    if (code == 0 &&
        button != BestwayButton::NoButton) {

        ESP_LOGW(TAG,
                 "Type2 state-aware knop index=%u niet beschikbaar voor model %s",
                 static_cast<unsigned>(button),
                 model_name());
        return;
    }

    type2_remote_button_ = button;
    type2_remote_target_value_ = target_value;
    type2_remote_wait_for_state_ = true;

    type2_remote_press(
        button,
        max_duration_ms);
}


void BestwayBus::type2_remote_button_loop(
    uint64_t now)
{
    if (!type2_remote_button_active_) {
        return;
    }

    bool target_reached = false;

    if (type2_remote_wait_for_state_) {
        switch (type2_remote_button_) {
            case BestwayButton::Power:
                target_reached =
                    spa.power == type2_remote_target_value_;
                break;

            case BestwayButton::Pump:
                target_reached =
                    spa.filter == type2_remote_target_value_;
                break;

            case BestwayButton::Bubbles:
                target_reached =
                    spa.bubbles == type2_remote_target_value_;
                break;

            case BestwayButton::HydroJets:
                target_reached =
                    spa.jets == type2_remote_target_value_;
                break;

            case BestwayButton::Heat:
                target_reached =
                    spa.heater == type2_remote_target_value_;
                break;

            case BestwayButton::Lock:
                target_reached =
                    spa.locked == type2_remote_target_value_;
                break;

            case BestwayButton::Unit:
                target_reached =
                    spa.fahrenheit == type2_remote_target_value_;
                break;

            default:
                break;
        }
    }

    if (!target_reached &&
        now < type2_remote_button_until_ms_) {

        /*
         * Remote knop heeft tijdelijk voorrang
         * boven de fysieke displayknop.
         */
        type2_cio_button_code_ =
            type2_remote_button_code_;

        return;
    }

    type2_remote_button_active_ = false;
    type2_remote_wait_for_state_ = false;
    type2_remote_button_ =
        BestwayButton::NoButton;

    /*
     * Na loslaten terug naar de laatst stabiel
     * gedecodeerde fysieke knop.
     */
    type2_cio_button_code_ =
        bestway_type2_button_code(
            model_,
            type2_dsp_old_button_);

    ESP_LOGI(TAG,
             "Type2 remote knop losgelaten");
}



void BestwayBus::type1_start_state_action(
    BestwayButton button,
    bool target_value)
{
    if (model_info().family !=
        BestwayProtocolFamily::SixWireType1) {
        return;
    }

    bool current_value = false;
    bool supported = true;

    switch (button) {
        case BestwayButton::Power:
            current_value = spa.power;
            break;
        case BestwayButton::Pump:
            current_value = spa.filter;
            break;
        case BestwayButton::Bubbles:
            current_value = spa.bubbles;
            break;
        case BestwayButton::HydroJets:
            current_value = spa.jets;
            break;
        case BestwayButton::Heat:
            current_value = spa.heater;
            break;
        case BestwayButton::Lock:
            current_value = spa.locked;
            break;
        case BestwayButton::Unit:
            current_value = spa.fahrenheit;
            break;
        default:
            supported = false;
            break;
    }

    if (!supported) {
        ESP_LOGW(TAG,
                 "Type1 auto-action: state-knop index=%u niet ondersteund",
                 static_cast<unsigned>(button));
        return;
    }

    if (current_value == target_value) {
        ESP_LOGI(TAG,
                 "Type1 auto-action: knop index=%u gewenste status al bereikt",
                 static_cast<unsigned>(button));
        return;
    }

    /*
     * Power en Lock zelf worden direct bediend. Voor alle overige
     * functies volgen we het oude ESP8266/v4 unlock()-gedrag:
     * eerst Power AAN indien nodig, daarna LOCK UIT, daarna pas
     * de gevraagde knop.
     */
    if (button == BestwayButton::Power ||
        button == BestwayButton::Lock ||
        (spa.power && !spa.locked)) {

        type1_remote_press_state(
            button,
            target_value,
            5000);
        return;
    }

    if (type1_auto_action_active_ ||
        type1_remote_button_active_ ||
        type1_remote_sequence_ != Type1RemoteSequence::None ||
        type1_target_sequence_active_) {

        ESP_LOGW(TAG,
                 "Type1 auto-action genegeerd: andere remote actie loopt nog");
        return;
    }

    type1_auto_action_active_ = true;
    type1_auto_action_step_ = 0;
    type1_auto_action_button_ = button;
    type1_auto_action_target_value_ = target_value;
    type1_auto_action_momentary_ = false;
    type1_auto_action_duration_ms_ = 100;

    ESP_LOGI(TAG,
             "Type1 auto-action gestart knop=%u doel=%s",
             static_cast<unsigned>(button),
             target_value ? "on" : "off");
}


void BestwayBus::type1_start_momentary_action(
    BestwayButton button,
    uint32_t duration_ms)
{
    if (model_info().family !=
        BestwayProtocolFamily::SixWireType1) {
        return;
    }

    if (spa.power && !spa.locked) {
        type1_remote_button_ = button;
        type1_remote_wait_for_state_ = false;
        type1_remote_press(button, duration_ms);
        return;
    }

    if (type1_auto_action_active_ ||
        type1_remote_button_active_ ||
        type1_remote_sequence_ != Type1RemoteSequence::None ||
        type1_target_sequence_active_) {

        ESP_LOGW(TAG,
                 "Type1 momentary auto-action genegeerd: andere remote actie loopt nog");
        return;
    }

    type1_auto_action_active_ = true;
    type1_auto_action_step_ = 0;
    type1_auto_action_button_ = button;
    type1_auto_action_target_value_ = false;
    type1_auto_action_momentary_ = true;
    type1_auto_action_duration_ms_ = duration_ms;

    ESP_LOGI(TAG,
             "Type1 momentary auto-action gestart knop=%u duur=%u ms",
             static_cast<unsigned>(button),
             static_cast<unsigned>(duration_ms));
}


void BestwayBus::type1_auto_action_loop(uint64_t now)
{
    (void)now;

    if (!type1_auto_action_active_) {
        return;
    }

    if (type1_remote_button_active_) {
        return;
    }

    switch (type1_auto_action_step_) {
        case 0:
            if (!spa.power) {
                type1_auto_action_step_ = 1;
                type1_remote_press_state(
                    BestwayButton::Power,
                    true,
                    5000);
                ESP_LOGI(TAG,
                         "Type1 auto-action: eerst Power AAN");
                return;
            }
            type1_auto_action_step_ = 2;
            [[fallthrough]];

        case 1:
            if (!spa.power) {
                ESP_LOGW(TAG,
                         "Type1 auto-action afgebroken: Power AAN niet bevestigd");
                type1_auto_action_active_ = false;
                type1_auto_action_step_ = 0;
                type1_auto_action_button_ = BestwayButton::NoButton;
                return;
            }
            type1_auto_action_step_ = 2;
            [[fallthrough]];

        case 2:
            if (spa.locked) {
                type1_auto_action_step_ = 3;
                type1_remote_press_state(
                    BestwayButton::Lock,
                    false,
                    5000);
                ESP_LOGI(TAG,
                         "Type1 auto-action: automatisch ontgrendelen");
                return;
            }
            type1_auto_action_step_ = 4;
            [[fallthrough]];

        case 3:
            if (spa.locked) {
                ESP_LOGW(TAG,
                         "Type1 auto-action afgebroken: ontgrendelen niet bevestigd");
                type1_auto_action_active_ = false;
                type1_auto_action_step_ = 0;
                type1_auto_action_button_ = BestwayButton::NoButton;
                return;
            }
            type1_auto_action_step_ = 4;
            [[fallthrough]];

        case 4:
            type1_auto_action_step_ = 5;

            if (type1_auto_action_momentary_) {
                type1_remote_button_ = type1_auto_action_button_;
                type1_remote_wait_for_state_ = false;
                type1_remote_press(
                    type1_auto_action_button_,
                    type1_auto_action_duration_ms_);
            } else {
                type1_remote_press_state(
                    type1_auto_action_button_,
                    type1_auto_action_target_value_,
                    5000);
            }
            return;

        case 5: {
            if (!type1_auto_action_momentary_) {
                bool current_value = false;
                bool known = true;

                switch (type1_auto_action_button_) {
                    case BestwayButton::Pump:
                        current_value = spa.filter;
                        break;
                    case BestwayButton::Bubbles:
                        current_value = spa.bubbles;
                        break;
                    case BestwayButton::HydroJets:
                        current_value = spa.jets;
                        break;
                    case BestwayButton::Heat:
                        current_value = spa.heater;
                        break;
                    case BestwayButton::Unit:
                        current_value = spa.fahrenheit;
                        break;
                    default:
                        known = false;
                        break;
                }

                if (!known ||
                    current_value != type1_auto_action_target_value_) {
                    ESP_LOGW(TAG,
                             "Type1 auto-action klaar zonder bevestigde doelstatus knop=%u",
                             static_cast<unsigned>(type1_auto_action_button_));
                }
            }

            type1_auto_action_active_ = false;
            type1_auto_action_step_ = 0;
            type1_auto_action_button_ = BestwayButton::NoButton;
            type1_auto_action_momentary_ = false;
            return;
        }

        default:
            type1_auto_action_active_ = false;
            type1_auto_action_step_ = 0;
            type1_auto_action_button_ = BestwayButton::NoButton;
            type1_auto_action_momentary_ = false;
            return;
    }
}


void BestwayBus::type1_remote_press(
    BestwayButton button,
    uint32_t duration_ms)
{
    if (model_info().family !=
        BestwayProtocolFamily::SixWireType1) {
        return;
    }

    const uint16_t code =
        bestway_type1_button_code(model_, button);

    /*
     * Bij Type1 is 0x0000 een geldige POWER-code.
     * Voor andere knoppen betekent 0x0000 dat die functie op dit
     * model niet beschikbaar is.
     */
    if (code == 0x0000 &&
        button != BestwayButton::Power) {

        ESP_LOGW(TAG,
                 "Type1 remote knop niet beschikbaar voor model %s",
                 model_name());
        return;
    }

    type1_remote_button_code_ = code;
    type1_remote_button_active_ = true;
    type1_remote_button_until_ms_ =
        now_ms() + duration_ms;

    // CIO ISR gebruikt type1_cio_button_code_ wanneer de pomp 0x42 vraagt.
    type1_cio_button_code_ = code;

    ESP_LOGI(TAG,
             "Type1 remote knop index=%u code=0x%04X duur=%u ms",
             static_cast<unsigned>(button),
             static_cast<unsigned>(code),
             static_cast<unsigned>(duration_ms));
}


void BestwayBus::type1_remote_press_state(
    BestwayButton button,
    bool target_value,
    uint32_t max_duration_ms)
{
    bool current_value = false;
    bool supported = true;

    switch (button) {
        case BestwayButton::Power:
            current_value = spa.power;
            break;

        case BestwayButton::Pump:
            current_value = spa.filter;
            break;

        case BestwayButton::Bubbles:
            current_value = spa.bubbles;
            break;

        case BestwayButton::HydroJets:
            current_value = spa.jets;
            break;

        case BestwayButton::Heat:
            current_value = spa.heater;
            break;

        case BestwayButton::Lock:
            current_value = spa.locked;
            break;

        case BestwayButton::Unit:
            current_value = spa.fahrenheit;
            break;

        default:
            supported = false;
            break;
    }

    if (!supported) {
        ESP_LOGW(TAG,
                 "Type1 state-aware knop index=%u niet ondersteund",
                 static_cast<unsigned>(button));
        return;
    }

    if (current_value == target_value) {
        ESP_LOGI(TAG,
                 "Type1 remote knop index=%u: gewenste status al bereikt",
                 static_cast<unsigned>(button));
        return;
    }

    type1_remote_button_ = button;
    type1_remote_target_value_ = target_value;
    type1_remote_wait_for_state_ = true;

    type1_remote_press(button, max_duration_ms);
}



void BestwayBus::type1_target_sequence_loop(uint64_t now)
{
    if (!type1_target_sequence_active_) {
        return;
    }

    /*
     * Zolang de 800 ms UP/DOWN-druk nog actief is absoluut
     * niets aan de CIO-buttoncode veranderen.
     *
     * De release-timer wordt al bij het starten van de druk gezet
     * op 800 ms druk + 400 ms NOBTN. Daarom moet deze controle
     * VOOR de releasefase staan.
     */
    if (type1_remote_button_active_ || type1_auto_action_active_) {
        return;
    }

    // Na de druk nog circa 400 ms expliciet NOBTN.
    if (type1_target_release_active_) {
        if (now < type1_target_release_until_ms_) {
            type1_target_force_nobtn_ = true;

            type1_cio_button_code_ =
                bestway_type1_button_code(
                    model_,
                    BestwayButton::NoButton);

            return;
        }

        type1_target_release_active_ = false;
        type1_target_force_nobtn_ = false;
    }

    /*
     * Oude ESP8266/v4 unlock() deed vóór targetregeling eerst Power AAN
     * en daarna LOCK UIT. Zo werkt +/- ook na automatische vergrendeling
     * of wanneer het display eerst nog uit stond.
     */
    if (!spa.power) {
        type1_target_force_nobtn_ = false;

        type1_remote_press_state(
            BestwayButton::Power,
            true,
            5000);

        ESP_LOGI(TAG,
                 "Type1 target: eerst Power AAN");

        return;
    }

    if (spa.locked) {
        type1_target_force_nobtn_ = false;

        type1_remote_press_state(
            BestwayButton::Lock,
            false,
            5000);

        ESP_LOGI(TAG,
                 "Type1 target: eerst unlock");

        return;
    }

    const int current_target_c =
        spa.target_temperature_c;

    /*
     * SpaState blijft altijd Celsius voor Matter/Web.
     *
     * De echte Type1-pomp werkt echter in de gekozen displayeenheid.
     * In Fahrenheit moet de UP/DOWN-sequence daarom met de ruwe
     * Fahrenheit-doelwaarde vergelijken, anders kunnen twee naburige
     * Fahrenheitwaarden na afronding dezelfde Celsiuswaarde geven.
     */
    const int requested_display_target =
        type1_unit_fahrenheit_
            ? ((type1_target_sequence_target_c_ * 9 + 2) / 5) + 32
            : type1_target_sequence_target_c_;

    if (type1_target_display_value_ >= 0) {
        if (type1_target_display_value_ == requested_display_target) {
            type1_target_sequence_active_ = false;
            type1_target_release_active_ = false;
            type1_target_force_nobtn_ = false;

            ESP_LOGI(TAG,
                     "Type1 target bereikt: display=%d %s (%d C)",
                     type1_target_display_value_,
                     type1_unit_fahrenheit_ ? "F" : "C",
                     type1_target_sequence_target_c_);
            return;
        }
    } else if (current_target_c ==
               type1_target_sequence_target_c_) {

        type1_target_sequence_active_ = false;
        type1_target_release_active_ = false;
        type1_target_force_nobtn_ = false;

        ESP_LOGI(TAG,
                 "Type1 target bereikt: %d C",
                 current_target_c);
        return;
    }

    /*
     * Failsafe:
     * als de pomp de target na vijf echte UP/DOWN-pogingen niet
     * terugmeldt, stoppen. Voorkomt eindeloos drukken vanuit Matter.
     */
    if (type1_target_attempts_ >= 5) {
        type1_target_sequence_active_ = false;
        type1_target_release_active_ = false;
        type1_target_force_nobtn_ = false;

        type1_cio_button_code_ =
            bestway_type1_button_code(
                model_,
                BestwayButton::NoButton);

        ESP_LOGW(
            TAG,
            "Type1 target AFGEBROKEN na %u pogingen: pomp reageert niet, gewenst=%d C huidig=%d C display=%d",
            static_cast<unsigned>(type1_target_attempts_),
            type1_target_sequence_target_c_,
            current_target_c,
            type1_target_display_value_);

        return;
    }

    const BestwayButton dir =
        type1_target_display_value_ >= 0
            ? (requested_display_target > type1_target_display_value_
                   ? BestwayButton::Up
                   : BestwayButton::Down)
            : (type1_target_sequence_target_c_ > current_target_c
                   ? BestwayButton::Up
                   : BestwayButton::Down);

    type1_remote_button_ = dir;
    type1_remote_target_value_ = false;
    type1_remote_wait_for_state_ = false;

    ++type1_target_attempts_;

    ESP_LOGI(
        TAG,
        "Type1 target poging %u/5: %s",
        static_cast<unsigned>(type1_target_attempts_),
        dir == BestwayButton::Up ? "UP" : "DOWN");

    type1_remote_press(
        dir,
        800);

    type1_target_release_active_ = true;
    type1_target_release_until_ms_ =
        now + 1200; // 800 ms druk + circa 400 ms release
}

void BestwayBus::type1_remote_sequence_loop(uint64_t now)
{
    if (type1_remote_sequence_ == Type1RemoteSequence::None) {
        return;
    }

    if (type1_remote_button_active_ || type1_auto_action_active_) {
        return;
    }

    if (type1_remote_sequence_ == Type1RemoteSequence::HeatEnable) {
        switch (type1_remote_sequence_step_) {
            case 0:
                if (!spa.power) {
                    type1_remote_press_state(
                        BestwayButton::Power,
                        true,
                        5000);
                    return;
                }
                type1_remote_sequence_step_ = 1;
                [[fallthrough]];

            case 1:
                if (spa.locked) {
                    type1_remote_press_state(
                        BestwayButton::Lock,
                        false,
                        5000);
                    type1_remote_sequence_step_ = 2;
                    return;
                }
                type1_remote_sequence_step_ = 2;
                [[fallthrough]];

            case 2:
                if (!spa.heater) {
                    type1_remote_press_state(
                        BestwayButton::Heat,
                        true,
                        5000);
                    type1_remote_sequence_step_ = 3;
                    return;
                }
                type1_remote_sequence_step_ = 3;
                [[fallthrough]];

            default:
                type1_remote_sequence_ =
                    Type1RemoteSequence::None;
                type1_remote_sequence_step_ = 0;
                ESP_LOGI(TAG, "Type1 Heat enable sequence klaar");
                return;
        }
    }

    if (type1_remote_sequence_ == Type1RemoteSequence::HeatDisable) {
        switch (type1_remote_sequence_step_) {
            case 0:
                if (spa.locked) {
                    type1_remote_press_state(
                        BestwayButton::Lock,
                        false,
                        5000);
                    type1_remote_sequence_step_ = 1;
                    return;
                }
                type1_remote_sequence_step_ = 1;
                [[fallthrough]];

            case 1:
                if (spa.heater) {
                    type1_remote_press_state(
                        BestwayButton::Heat,
                        false,
                        5000);
                    type1_remote_sequence_step_ = 2;
                    return;
                }
                type1_remote_sequence_step_ = 2;
                [[fallthrough]];

            default:
                type1_remote_sequence_ =
                    Type1RemoteSequence::None;
                type1_remote_sequence_step_ = 0;
                ESP_LOGI(TAG, "Type1 Heat disable sequence klaar");
                return;
        }
    }
}

void BestwayBus::type1_remote_button_loop(uint64_t now)
{
    if (!type1_remote_button_active_) {
        return;
    }

    bool target_reached = false;

    if (type1_remote_wait_for_state_) {
        switch (type1_remote_button_) {
            case BestwayButton::Power:
                target_reached =
                    spa.power == type1_remote_target_value_;
                break;

            case BestwayButton::Pump:
                target_reached =
                    spa.filter == type1_remote_target_value_;
                break;

            case BestwayButton::Bubbles:
                target_reached =
                    spa.bubbles == type1_remote_target_value_;
                break;

            case BestwayButton::HydroJets:
                target_reached =
                    spa.jets == type1_remote_target_value_;
                break;

            case BestwayButton::Heat:
                target_reached =
                    spa.heater == type1_remote_target_value_;
                break;

            case BestwayButton::Lock:
                target_reached =
                    spa.locked == type1_remote_target_value_;
                break;

            case BestwayButton::Unit:
                target_reached =
                    spa.fahrenheit == type1_remote_target_value_;
                break;

            default:
                break;
        }
    }

    if (!target_reached &&
        now < type1_remote_button_until_ms_) {

        // Remote knop heeft tijdelijk voorrang boven fysieke displayknop.
        type1_cio_button_code_ =
            type1_remote_button_code_;
        return;
    }

    type1_remote_button_active_ = false;
    type1_remote_wait_for_state_ = false;
    type1_remote_button_ = BestwayButton::NoButton;

    /*
     * De oude werkende v4 stuurt na een target-UP/DOWN
     * expliciet ongeveer 400 ms NOBTN.
     *
     * Tijdens zo'n target-release dus NIET meteen de fysieke
     * displayknop teruggeven aan CIO.
     */
    if (type1_target_force_nobtn_) {
        type1_cio_button_code_ =
            bestway_type1_button_code(
                model_,
                BestwayButton::NoButton);
    } else {
        BestwayButton decoded = BestwayButton::NoButton;

        if (bestway_type1_decode_button(
                model_,
                type1_dsp_button_code_,
                decoded)) {

            type1_cio_button_code_ =
                type1_dsp_button_code_;
        } else {
            type1_cio_button_code_ =
                bestway_type1_button_code(
                    model_,
                    BestwayButton::NoButton);
        }
    }

    ESP_LOGI(TAG, "Type1 remote knop losgelaten");
}

void BestwayBus::type1_dsp_poll_button(uint64_t now_ms)
{
    // Zelfde begrenzing als oude v4
    if ((now_ms - type1_dsp_last_button_ms_) < 90) {
        return;
    }

    type1_dsp_last_button_ms_ = now_ms;

    const gpio_num_t cs_pin =
        static_cast<gpio_num_t>(pins_[5]);

    // Start packet
    gpio_set_level(cs_pin, 0);
    esp_rom_delay_us(50);

    // Vraag fysieke knopstatus op
    type1_dsp_send_bits(0x42, 8);

    const uint16_t code =
        type1_dsp_receive_bits();

    // End packet
    gpio_set_level(cs_pin, 1);
    esp_rom_delay_us(30);

    type1_dsp_button_code_ = code;

    type1_dsp_raw_button_[0] =
        static_cast<uint8_t>(code >> 8);

    type1_dsp_raw_button_[1] =
        static_cast<uint8_t>(code & 0xFF);

    /*
     * Transparante display -> pomp richting.
     *
     * Oude v4-logica:
     * - bekende code -> echte knop
     * - onbekende code (o.a. 0xFFFF) -> NOBTN
     *
     * Daardoor kan een knop nooit "blijven hangen".
     */
    BestwayButton decoded_button = BestwayButton::NoButton;

    if (bestway_type1_decode_button(
            model_,
            code,
            decoded_button)) {

        /*
         * Alleen het oude temperatuur UP/DOWN geluid terugbrengen.
         *
         * Eén sweep per fysieke knopdruk; zolang de knop vastgehouden
         * wordt starten we niet iedere 90 ms opnieuw.
         */
        if (decoded_button != type1_last_physical_button_) {
            if (decoded_button == BestwayButton::Up) {
                type1_audio_start_temp_sweep(true);
            } else if (decoded_button == BestwayButton::Down) {
                type1_audio_start_temp_sweep(false);
            } else if (decoded_button != BestwayButton::NoButton) {
                type1_audio_start_beep();
            }
        }

        type1_last_physical_button_ = decoded_button;

        if (!type1_remote_button_active_) {
            type1_cio_button_code_ = code;
        }
    } else {
        type1_last_physical_button_ =
            BestwayButton::NoButton;

        if (!type1_remote_button_active_) {
            type1_cio_button_code_ =
                bestway_type1_button_code(
                    model_,
                    BestwayButton::NoButton);
        }
    }
}

void BestwayBus::type1_dsp_upload_payload(uint64_t now_ms)
{
    // Zelfde refreshlimiet als oude v4: ongeveer 11 Hz
    if ((now_ms - type1_dsp_last_refresh_ms_) < 90) {
        return;
    }

    type1_dsp_last_refresh_ms_ = now_ms;
    ++type1_dsp_upload_count_;

    const gpio_num_t cs_pin =
        static_cast<gpio_num_t>(pins_[5]);

    uint8_t brightness =
        static_cast<uint8_t>(type1_dsp_brightness_ & 0x07);

    uint8_t enable_led = 0;

    if (brightness > 0) {
        enable_led = 0x08;  // DSP_DIM_ON
        --brightness;
    }

    // CMD1 = mode 6 / 11 bytes / 7 segments
    esp_rom_delay_us(30);
    gpio_set_level(cs_pin, 0);
    type1_dsp_send_bits(0x05, 8);
    gpio_set_level(cs_pin, 1);

    // DATAWRITE
    esp_rom_delay_us(50);
    gpio_set_level(cs_pin, 0);
    type1_dsp_send_bits(0x40, 8);
    gpio_set_level(cs_pin, 1);

    // 11-byte display payload
    esp_rom_delay_us(50);
    gpio_set_level(cs_pin, 0);

    for (size_t i = 0; i < 11; ++i) {
        type1_dsp_send_bits(type1_dsp_payload_[i], 8);
    }

    gpio_set_level(cs_pin, 1);

    // Brightness / display enable
    esp_rom_delay_us(50);
    gpio_set_level(cs_pin, 0);

    type1_dsp_send_bits(
        static_cast<uint8_t>(0x80 | enable_led | brightness),
        8);

    gpio_set_level(cs_pin, 1);
    esp_rom_delay_us(50);
}

char BestwayBus::decode_type1_char(uint8_t value) const
{
    static constexpr uint8_t charcodes[] = {
        0x7F, 0x0D, 0xB7, 0x9F, 0xCD, 0xDB, 0xFB, 0x0F, 0xFF, 0xDF,
        0x01, 0x81, 0xEF, 0xF9, 0x73, 0xBD, 0xF3, 0xE3, 0xFB, 0xE9,
        0xED, 0x61, 0x1D, 0xE1, 0x71, 0x01, 0xA9, 0xB9, 0xE7, 0xCF,
        0xA1, 0xDB, 0xF1, 0x39, 0x7D, 0x01, 0xDD, 0xB7
    };

    static constexpr char chars[] = {
        '0','1','2','3','4','5','6','7','8','9',
        ' ','-','A','b','C','d','E','F','G','H',
        'I','J','L','n','o',' ','P','r','S','t',
        'U','y','c','h','u',' ','e','2'
    };

    static_assert(sizeof(charcodes) == sizeof(chars),
                  "Type1 charcode tabel mismatch");

    for (size_t i = 0; i < sizeof(charcodes); ++i) {
        /*
         * Zelfde als oude v4:
         * LSB maskeren omdat die tussen pompen kan verschillen.
         */
        if ((value & 0xFE) == (charcodes[i] & 0xFE)) {
            return chars[i];
        }
    }

    return '*';
}

void BestwayBus::process_type1_cio_packet()
{
    if (!type1_cio_new_packet_) {
        return;
    }

    uint8_t payload[11];
    uint8_t packet_error;
    uint8_t brightness;

    int error_byte_count;
    int error_bit_count;
    int error_cmd_matches;
    uint8_t error_received_byte;
    bool error_was_output;

    /*
     * Ready-snapshot atomair overnemen.
     * Zo kan de ISR deze buffer tijdens het kopieren niet wijzigen.
     */
    portENTER_CRITICAL(&type1_cio_mux_);

    if (!type1_cio_new_packet_) {
        portEXIT_CRITICAL(&type1_cio_mux_);
        return;
    }

    packet_error = type1_cio_ready_packet_error_;
    brightness = type1_cio_ready_brightness_;

    error_byte_count =
        type1_cio_ready_error_byte_count_;
    error_bit_count =
        type1_cio_ready_error_bit_count_;
    error_cmd_matches =
        type1_cio_ready_error_cmd_matches_;
    error_received_byte =
        type1_cio_ready_error_received_byte_;
    error_was_output =
        type1_cio_ready_error_was_output_;

    for (size_t i = 0; i < 11; ++i) {
        payload[i] = type1_cio_ready_payload_[i];
    }

    type1_cio_new_packet_ = false;

    portEXIT_CRITICAL(&type1_cio_mux_);

    if (packet_error != 0) {
        ESP_LOGW(
            TAG,
            "Type1 CIO packetfout: err=0x%02X bytes=%d bits=%d msg=0x%02X matches=%d output=%u",
            static_cast<unsigned>(packet_error),
            error_byte_count,
            error_bit_count,
            static_cast<unsigned>(error_received_byte),
            error_cmd_matches,
            static_cast<unsigned>(error_was_output ? 1 : 0));
        return;
    }

    /*
     * Een foutloos CIO-pakket bewijst dat de pomp communiceert.
     * Het spikefilter bepaalt alleen of we de inhoud accepteren.
     */
    last_packet_ms_ = now_ms();

    if (!connected_) {
        connected_ = true;
        spa.set_connection_state(true);
        bump_version();
        ESP_LOGI(TAG, "Type1 pomp verbonden");
    }

    /*
     * Type1 spikefilter.
     *
     * Oude v4 accepteerde een payload pas nadat twee opeenvolgende
     * berichten dezelfde 8-bit checksum-som hadden.
     *
     * Hier vergelijken we alle 11 bytes. Daarmee kunnen twee
     * verschillende payloads met toevallig dezelfde som niet als
     * gelijk worden gezien.
     */
    static uint8_t previous_candidate[11] = {};
    static bool have_previous_candidate = false;

    bool same_as_previous = have_previous_candidate;

    if (same_as_previous) {
        for (size_t i = 0; i < 11; ++i) {
            if (payload[i] != previous_candidate[i]) {
                same_as_previous = false;
                break;
            }
        }
    }

    if (!same_as_previous) {
        for (size_t i = 0; i < 11; ++i) {
            previous_candidate[i] = payload[i];
        }

        have_previous_candidate = true;
        return;
    }

    for (size_t i = 0; i < 11; ++i) {
        type1_cio_prev_payload_[i] = payload[i];
    }

    /*
     * Type1 DSP-payload opbouwen zoals oude v4.
     * Alleen de CIO-bytes 1,3,5,7,9 horen transparant naar het display.
     */
    type1_dsp_payload_[0] = 0xC0;
    for (size_t i = 1; i < 11; ++i) {
        type1_dsp_payload_[i] = 0;
    }

    const bool display_power =
        (payload[9] & (1U << 4)) != 0;

    if (display_power) {
        type1_dsp_payload_[1] = payload[1];
        type1_dsp_payload_[3] = payload[3];
        type1_dsp_payload_[5] = payload[5];
        type1_dsp_payload_[7] = payload[7];
        type1_dsp_payload_[9] = payload[9];
    }

    spa.mark_packet_received();

    type1_dsp_brightness_ =
        static_cast<uint8_t>(brightness & 0x07);

    /*
     * Statusbits exact volgens CIO_TYPE1.h uit de oude v4.
     */
    const bool locked  = (payload[7] & (1U << 3)) != 0;
    const bool power   = (payload[9] & (1U << 4)) != 0;
    const bool bubbles = (payload[7] & (1U << 7)) != 0;

    const bool timer_led2 =
        (payload[7] & (1U << 1)) != 0;

    const bool timer_led1 =
        (payload[7] & (1U << 2)) != 0;

    const bool timer_button_led =
        (payload[7] & (1U << 4)) != 0;

    const bool timer_active =
        timer_button_led ||
        timer_led1 ||
        timer_led2;

    const bool heat_red   = (payload[7] & (1U << 5)) != 0;
    const bool heat_green = (payload[7] & (1U << 6)) != 0;
    const bool pump     = (payload[9] & (1U << 1)) != 0;

    const bool celsius_led =
        (payload[9] & (1U << 2)) != 0;

    const bool fahrenheit_led =
        (payload[9] & (1U << 3)) != 0;

    bool jets = false;
    if (has_jets()) {
        jets = (payload[9] & (1U << 5)) != 0;
    }

    const bool heater_enabled =
        heat_red || heat_green;

    const bool heater_active =
        heat_red;

    const char char1 = decode_type1_char(payload[1]);
    const char char2 = decode_type1_char(payload[3]);
    const char char3 = decode_type1_char(payload[5]);

    int display_value = -1;
    bool changed = false;

    // Zelfde display-validatie als oude v4.
    if (char1 == 'e') {
        char error_text[3] = {char2, char3, '\0'};

        char *end = nullptr;
        const long parsed_error =
            strtol(error_text, &end, 10);

        if (end != error_text &&
            *end == '\0' &&
            parsed_error >= 0 &&
            parsed_error <= 99) {

            if (spa.error_code != static_cast<int>(parsed_error)) {
                spa.error_code = static_cast<int>(parsed_error);
                changed = true;
            }
        }
    }
    else if (char1 != '*' &&
             char2 != '*' &&
             char3 != '*' &&
             char3 != 'H' &&
             char3 != ' ') {

        char temp_text[4] = {char1, char2, char3, '\0'};

        char *end = nullptr;
        const long parsed = strtol(temp_text, &end, 10);

        if (end != temp_text &&
            *end == '\0' &&
            parsed >= 0 &&
            parsed <= 199) {

            display_value = static_cast<int>(parsed);
        }

        // Geldige normale displayweergave wist oude foutstatus.
        if (spa.error_code != 0) {
            spa.error_code = 0;
            changed = true;
        }
    }

    /*
     * Oude v4 target/temperature herkenning:
     *
     * - UP/DOWN actief: display toont target
     * - na 2 s: displaywaarde onzeker
     * - na 6 s: display toont weer actuele temperatuur
     */
    const uint64_t current_ms = now_ms();

    const uint16_t up_code =
        bestway_type1_button_code(model_, BestwayButton::Up);

    const uint16_t down_code =
        bestway_type1_button_code(model_, BestwayButton::Down);

    if (type1_cio_button_code_ == up_code ||
        type1_cio_button_code_ == down_code) {

        type1_button_release_ms_ = current_ms;

        if (power && !locked) {
            type1_capture_phase_ =
                Type1CapturePhase::ReadTarget;
        }
    }

    if (type1_button_release_ms_ != 0) {
        const uint64_t elapsed =
            current_ms - type1_button_release_ms_;

        if (elapsed > 6000) {
            type1_capture_phase_ =
                Type1CapturePhase::ReadTemperature;
        } else if (elapsed > 2000) {
            type1_capture_phase_ =
                Type1CapturePhase::Uncertain;
        }
    }

    if (spa.locked != locked) {
        spa.locked = locked;
        changed = true;
    }

    if (spa.timer_active != timer_active) {
        spa.timer_active = timer_active;
        changed = true;
    }

    if (spa.power != power) {
        spa.power = power;
        changed = true;
    }

    if (spa.filter != pump) {
        spa.filter = pump;
        changed = true;
    }

    if (spa.bubbles != bubbles) {
        spa.bubbles = bubbles;
        changed = true;
    }

    if (spa.jets != jets) {
        spa.jets = jets;
        changed = true;
    }

    if (spa.heater != heater_enabled) {
        spa.heater = heater_enabled;
        changed = true;
    }

    if (spa.heater_active != heater_active) {
        spa.heater_active = heater_active;
        changed = true;
    }

    /*
     * Zelfde gedrag als oude v4:
     * tijdens bepaalde displaymodi kunnen beide unit-leds uit zijn.
     * Dan houden we de vorige geldige unit vast.
     */
    if (celsius_led || fahrenheit_led) {
        const bool unit_fahrenheit = fahrenheit_led;

        if (type1_unit_fahrenheit_ != unit_fahrenheit) {
            type1_unit_fahrenheit_ = unit_fahrenheit;

            // Oude ruwe target hoorde bij de vorige eenheid.
            type1_target_display_value_ = -1;
        }

        if (spa.fahrenheit != unit_fahrenheit) {
            spa.fahrenheit = unit_fahrenheit;
            changed = true;
        }
    }

    if (display_value >= 0) {
        /*
         * De pomp toont de waarde in zijn eigen actieve eenheid.
         * SpaState blijft bewust altijd Celsius omdat Matter en de
         * rest van de controller temperature_c gebruiken.
         *
         * Alleen positieve spa-temperaturen zijn hier van toepassing,
         * dus integer afronding naar dichtstbijzijnde °C is voldoende.
         */
        const int display_value_c =
            type1_unit_fahrenheit_
                ? (((display_value - 32) * 5 + 4) / 9)
                : display_value;

        if (type1_capture_phase_ ==
                Type1CapturePhase::ReadTarget) {

            // Ruwe pompwaarde bewaren voor exacte UP/DOWN-regeling.
            type1_target_display_value_ = display_value;

            // Zelfde plausibiliteitsgrens als oude v4, maar nu in °C.
            if (display_value_c > 19) {

                if (!spa.target_temperature_valid ||
                    display_value_c != spa.target_temperature_c) {

                    spa.target_temperature_c = display_value_c;
                    spa.target_temperature_valid = true;
                    changed = true;

                    ESP_LOGI(
                        TAG,
                        "Type1 echte pomp-target geleerd: %d C (display=%d %s)",
                        display_value_c,
                        display_value,
                        type1_unit_fahrenheit_ ? "F" : "C");
                }
            }

        } else if (type1_capture_phase_ ==
                       Type1CapturePhase::ReadTemperature) {

            if (display_value_c != spa.temperature_c) {
                spa.temperature_c = display_value_c;
                changed = true;
            }
        }

        // Tijdens Uncertain bewust niets opslaan.
    }

    if (changed) {
        bump_version();
    }

    ESP_LOGD(TAG,
             "Type1 CIO status: power=%u filter=%u bubbles=%u jets=%u heat=%u active=%u lock=%u brightness=%u",
             static_cast<unsigned>(power),
             static_cast<unsigned>(pump),
             static_cast<unsigned>(bubbles),
             static_cast<unsigned>(jets),
             static_cast<unsigned>(heater_enabled),
             static_cast<unsigned>(heater_active),
             static_cast<unsigned>(locked),
             static_cast<unsigned>(brightness & 7));
}

bool BestwayBus::init_type2_cio()
{
    if (model_info().family !=
        BestwayProtocolFamily::SixWireType2) {
        return false;
    }

    const gpio_num_t td_pin =
        static_cast<gpio_num_t>(pins_[0]);

    const gpio_num_t clk_pin =
        static_cast<gpio_num_t>(pins_[1]);

    const gpio_num_t ld_pin =
        static_cast<gpio_num_t>(pins_[2]);

    gpio_config_t io = {};

    // TD = antwoord richting pomp, in rust HIGH.
    io.pin_bit_mask = 1ULL << td_pin;
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(TAG,
                 "Type2 CIO TD GPIO configuratie mislukt");
        return false;
    }

    gpio_set_level(td_pin, 1);

    // CLK + LD zijn ingangen vanuit de pomp.
    io = {};
    io.pin_bit_mask =
        (1ULL << clk_pin) |
        (1ULL << ld_pin);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(TAG,
                 "Type2 CIO CLK/LD GPIO configuratie mislukt");
        return false;
    }

    esp_err_t err =
        gpio_install_isr_service(ESP_INTR_FLAG_IRAM);

    if (err != ESP_OK &&
        err != ESP_ERR_INVALID_STATE) {

        ESP_LOGE(TAG,
                 "GPIO ISR service starten mislukt: %s",
                 esp_err_to_name(err));
        return false;
    }

    err = gpio_set_intr_type(
        ld_pin,
        GPIO_INTR_ANYEDGE);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Type2 CIO LD interrupt instellen mislukt: %s",
                 esp_err_to_name(err));
        return false;
    }

    err = gpio_set_intr_type(
        clk_pin,
        GPIO_INTR_ANYEDGE);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Type2 CIO CLK interrupt instellen mislukt: %s",
                 esp_err_to_name(err));
        return false;
    }

    err = gpio_isr_handler_add(
        ld_pin,
        &BestwayBus::type2_cio_ld_isr,
        this);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Type2 CIO LD ISR toevoegen mislukt: %s",
                 esp_err_to_name(err));
        return false;
    }

    err = gpio_isr_handler_add(
        clk_pin,
        &BestwayBus::type2_cio_clk_isr,
        this);

    if (err != ESP_OK) {
        gpio_isr_handler_remove(ld_pin);

        ESP_LOGE(TAG,
                 "Type2 CIO CLK ISR toevoegen mislukt: %s",
                 esp_err_to_name(err));
        return false;
    }

    type2_cio_byte_count_ = 0;
    type2_cio_bit_count_ = 0;
    type2_cio_received_cmd_ = 0;
    type2_cio_brightness_ = 7;
    type2_cio_packet_active_ = false;
    type2_cio_new_packet_ = false;

    // MODEL54149E: NOBTN = 0
    type2_cio_button_code_ = 0;

    for (size_t i = 0; i < 5; ++i) {
        type2_cio_payload_[i] = 0;
        type2_cio_ready_payload_[i] = 0;
        type2_cio_prev_payload_[i] = 0;
    }

    ESP_LOGI(TAG,
             "6-wire Type2 CIO klaar: TD=%u CLK=%u LD=%u",
             static_cast<unsigned>(td_pin),
             static_cast<unsigned>(clk_pin),
             static_cast<unsigned>(ld_pin));

    return true;
}


bool BestwayBus::init_type1_cio()
{
    if (model_info().family != BestwayProtocolFamily::SixWireType1) {
        return false;
    }

    const gpio_num_t data_pin = static_cast<gpio_num_t>(pins_[0]);
    const gpio_num_t clk_pin  = static_cast<gpio_num_t>(pins_[1]);
    const gpio_num_t cs_pin   = static_cast<gpio_num_t>(pins_[2]);

    gpio_config_t io = {};

    // DATA: input actief houden; output-driver schakelen we later
    // rechtstreeks met gpio_ll_output_enable/disable().
    io.pin_bit_mask = 1ULL << data_pin;
    io.mode = GPIO_MODE_INPUT_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(TAG, "Type1 CIO DATA GPIO configuratie mislukt");
        return false;
    }

    gpio_ll_output_disable(&GPIO, static_cast<uint32_t>(data_pin));

    // CLK + CS als inputs
    io = {};
    io.pin_bit_mask =
        (1ULL << clk_pin) |
        (1ULL << cs_pin);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(TAG, "Type1 CIO CLK/CS GPIO configuratie mislukt");
        return false;
    }

    /*
     * Type1 gebruikt een snelle synchrone CIO-bus.
     * Geef GPIO interrupts daarom een hogere interruptprioriteit,
     * zodat CLK/CS-flanken zo min mogelijk vertraging oplopen door
     * WiFi/Matter/MQTT/Web taken.
     */
    esp_err_t err = gpio_install_isr_service(
        ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3);

    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(
            TAG,
            "GPIO ISR service bestond al - Type1 kon IRAM/LEVEL3 flags niet opnieuw instellen");
    } else if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "GPIO ISR service starten mislukt: %s",
            esp_err_to_name(err));
        return false;
    } else {
        ESP_LOGI(
            TAG,
            "GPIO ISR service door Type1 gestart met IRAM + LEVEL3");
    }

    err = gpio_set_intr_type(cs_pin, GPIO_INTR_ANYEDGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Type1 CIO CS interrupt instellen mislukt: %s",
                 esp_err_to_name(err));
        return false;
    }

    err = gpio_set_intr_type(clk_pin, GPIO_INTR_POSEDGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Type1 CIO CLK interrupt instellen mislukt: %s",
                 esp_err_to_name(err));
        return false;
    }

    err = gpio_isr_handler_add(
        cs_pin,
        &BestwayBus::type1_cio_cs_isr,
        this);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Type1 CIO CS ISR toevoegen mislukt: %s",
                 esp_err_to_name(err));
        return false;
    }

    err = gpio_isr_handler_add(
        clk_pin,
        &BestwayBus::type1_cio_clk_isr,
        this);

    if (err != ESP_OK) {
        gpio_isr_handler_remove(cs_pin);

        ESP_LOGE(TAG,
                 "Type1 CIO CLK ISR toevoegen mislukt: %s",
                 esp_err_to_name(err));
        return false;
    }

    type1_cio_byte_count_ = 0;
    type1_cio_bit_count_ = 0;
    type1_cio_cmd_matches_ = 0;
    type1_cio_send_bit_ = 8;
    type1_cio_received_byte_ = 0;
    type1_cio_brightness_ = 7;
    type1_cio_packet_error_ = 0;
    type1_cio_data_is_output_ = false;
    type1_cio_packet_active_ = false;
    type1_cio_new_packet_ = false;

    // Veilige beginwaarde richting pomp.
    type1_cio_button_code_ =
        bestway_type1_button_code(
            model_,
            BestwayButton::NoButton);

    ESP_LOGI(TAG,
             "6-wire Type1 CIO klaar: DATA=%u CLK=%u CS=%u",
             static_cast<unsigned>(data_pin),
             static_cast<unsigned>(clk_pin),
             static_cast<unsigned>(cs_pin));

    return true;
}

bool BestwayBus::init_uart4wire()
{
    if (model_info().family != BestwayProtocolFamily::Uart4Wire) {
        ESP_LOGW(TAG, "UART4 init overgeslagen: model %s is geen 4-wire model",
                 model_name());
        return false;
    }

    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {
            .allow_pd = false,
            .backup_before_sleep = false,
        },
    };

    // UART0 = CIO / pomp
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(
        UART_NUM_0,
        pins_[1],   // CIO TX
        pins_[0],   // CIO RX
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_driver_install(
        UART_NUM_0,
        256,
        256,
        0,
        nullptr,
        0));

    // UART1 = DSP / display
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(
        UART_NUM_1,
        pins_[3],   // DSP TX
        pins_[4],   // DSP RX
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_driver_install(
        UART_NUM_1,
        256,
        256,
        0,
        nullptr,
        0));

    ESP_LOGI(TAG,
             "4-wire UART voorbereid: CIO RX=%u TX=%u | DSP RX=%u TX=%u",
             pins_[0], pins_[1], pins_[4], pins_[3]);

    return true;
}

void BestwayBus::uart4wire_loop(uint64_t now)
{
    if (godmode_) {
        godmode_4wire_control(now);
    }

    static constexpr size_t FRAME_SIZE = 7;
    static constexpr size_t TEMP_INDEX = 2;
    static constexpr size_t ERROR_INDEX = 3;
    static constexpr size_t CHECKSUM_INDEX = 5;

    // CIO / pomp -> ESP
    size_t cio_buffered = 0;

    if (uart_get_buffered_data_len(UART_NUM_0, &cio_buffered) == ESP_OK &&
        cio_buffered >= FRAME_SIZE) {

        uint8_t frame[FRAME_SIZE];

        const int len = uart_read_bytes(
            UART_NUM_0,
            frame,
            FRAME_SIZE,
            0);

        if (len == static_cast<int>(FRAME_SIZE)) {
            const uint8_t calculated_checksum =
                static_cast<uint8_t>(
                    frame[1] +
                    frame[2] +
                    frame[3] +
                    frame[4]);

            if (frame[CHECKSUM_INDEX] == calculated_checksum) {
                for (size_t i = 0; i < FRAME_SIZE; ++i) {
                    last_cio_frame_[i] = frame[i];
                }
                have_cio_frame_ = true;

                const int temperature_c = frame[TEMP_INDEX];
                const uint8_t error_code = frame[ERROR_INDEX];

                if (spa.temperature_c != temperature_c) {
                    spa.temperature_c = temperature_c;
                    bump_version();
                }

                if (spa.error_code != static_cast<int>(error_code)) {
                    spa.error_code = static_cast<int>(error_code);
                    bump_version();
                }

                last_packet_ms_ = now_ms();

                if (!connected_) {
                    connected_ = true;
                    spa.set_connection_state(true);
                    bump_version();
                    ESP_LOGI(TAG, "4-wire pomp verbonden");
                }

                spa.mark_packet_received();

                uint8_t display_frame[FRAME_SIZE];

                /*
                 * Spa Control v3.2.6 DSP_4W::generatePayload():
                 *
                 * Anti-Godmode:
                 *   transparant het echte CIO-frame doorgeven.
                 *
                 * Godmode:
                 *   onbekende/protocolbytes behouden,
                 *   temperatuur en error vanuit onze SpaState invullen,
                 *   checksum opnieuw berekenen.
                 *
                 * Let op:
                 * het 4-wire protocol heeft hier géén apart
                 * target- of Celsius/Fahrenheit-veld.
                 * De buswaarde voor temperatuur blijft Celsius.
                 */
                for (size_t i = 0; i < FRAME_SIZE; ++i) {
                    display_frame[i] = frame[i];
                }

                if (godmode_) {
                    display_frame[TEMP_INDEX] =
                        static_cast<uint8_t>(
                            spa.temperature_c);

                    display_frame[ERROR_INDEX] =
                        static_cast<uint8_t>(
                            spa.error_code);

                    display_frame[CHECKSUM_INDEX] =
                        static_cast<uint8_t>(
                            display_frame[1] +
                            display_frame[2] +
                            display_frame[3] +
                            display_frame[4]);
                }

                const int written = uart_write_bytes(
                    UART_NUM_1,
                    reinterpret_cast<const char *>(display_frame),
                    FRAME_SIZE);

                if (written != static_cast<int>(FRAME_SIZE)) {
                    ESP_LOGW(
                        TAG,
                        "4-wire CIO->DSP frame niet volledig doorgestuurd");
                }

                ESP_LOGD(TAG,
                         "4-wire CIO geldig: temp=%d C error=%u",
                         temperature_c,
                         static_cast<unsigned>(error_code));
            } else {
                ESP_LOGW(TAG,
                         "4-wire CIO checksum fout: ontvangen=%u verwacht=%u",
                         frame[CHECKSUM_INDEX],
                         calculated_checksum);
            }
        }
    }

    // DSP / display -> ESP
    size_t dsp_buffered = 0;

    if (uart_get_buffered_data_len(UART_NUM_1, &dsp_buffered) == ESP_OK &&
        dsp_buffered >= FRAME_SIZE) {

        uint8_t frame[FRAME_SIZE];

        const int len = uart_read_bytes(
            UART_NUM_1,
            frame,
            FRAME_SIZE,
            0);

        if (len == static_cast<int>(FRAME_SIZE)) {
            const uint8_t calculated_checksum =
                static_cast<uint8_t>(
                    frame[1] +
                    frame[2] +
                    frame[3] +
                    frame[4]);

            if (frame[CHECKSUM_INDEX] == calculated_checksum) {
                for (size_t i = 0; i < FRAME_SIZE; ++i) {
                    last_dsp_frame_[i] = frame[i];
                }
                have_dsp_frame_ = true;

                const auto masks = bestway_4wire_masks(model_);
                const uint8_t command = frame[2];

                const bool pump =
                    masks.pump != 0 && (command & masks.pump) != 0;
                const bool bubbles =
                    masks.bubbles != 0 && (command & masks.bubbles) != 0;
                const bool jets =
                    masks.jets != 0 && (command & masks.jets) != 0;
                const bool power =
                    masks.power != 0 && (command & masks.power) != 0;

                const bool heat_stage1 =
                    masks.heat1 != 0 && (command & masks.heat1) != 0;

                const bool heat_stage2 =
                    masks.heat2 != 0 && (command & masks.heat2) != 0;

                /*
                 * Betrouwbare status uit het echte 4-wire DSP-frame
                 * terugkoppelen naar SpaState.
                 *
                 * Let op:
                 * - filter/bubbles/jets zijn rechtstreeks uit command byte 2 afleidbaar
                 * - heater_active = minstens één echte heatertrap actief
                 * - spa.heater en spa.power zijn bij 4-wire niet betrouwbaar
                 *   uit dit frame af te leiden en laten we daarom ongemoeid
                 */
                bool state_changed = false;

                const bool physical_heat =
                    heat_stage1 || heat_stage2;

                /*
                 * Zonder Godmode is het fysieke display de bron
                 * van waarheid, precies zoals voorheen.
                 *
                 * Met Godmode is de interne 4-wire state-machine
                 * de bron van waarheid. Het fysieke display wordt
                 * dan hieronder als INPUT verwerkt.
                 */
                if (!godmode_) {
                    if (spa.filter != pump) {
                        spa.filter = pump;
                        state_changed = true;
                    }

                    if (spa.bubbles != bubbles) {
                        spa.bubbles = bubbles;
                        state_changed = true;
                    }

                    if (spa.jets != jets) {
                        spa.jets = jets;
                        state_changed = true;
                    }

                    if (spa.heater_active != physical_heat) {
                        spa.heater_active = physical_heat;
                        state_changed = true;
                    }

                    if (state_changed) {
                        bump_version();
                    }
                }

                /*
                 * Fysieke displaywijzigingen hebben voorrang.
                 * Als een gebruiker op het echte display een functie verandert,
                 * laten we een eventuele remote overlay voor die functie los.
                 */
                if (!have_dsp_state_) {
                    dsp_pump_ = pump;
                    dsp_bubbles_ = bubbles;
                    dsp_jets_ = jets;
                    dsp_heat_ = physical_heat;
                    have_dsp_state_ = true;

                    /*
                     * Als Godmode al actief is wanneer het eerste
                     * geldige DSP-frame binnenkomt, begin dan bij
                     * de echte fysieke toestand.
                     */
                    if (godmode_) {
                        if (sync_4wire_state_from_dsp(
                                bubbles,
                                jets,
                                pump,
                                physical_heat)) {

                            spa.heater = physical_heat;

                            ESP_LOGI(
                                TAG,
                                "4-wire Godmode init uit fysiek display");
                        }
                    }

                } else if (godmode_) {
                    const bool physical_changed =
                        pump != dsp_pump_ ||
                        bubbles != dsp_bubbles_ ||
                        jets != dsp_jets_ ||
                        physical_heat != dsp_heat_;

                    if (physical_changed) {
                        /*
                         * Het fysieke display mag in Godmode nog
                         * steeds bedienen.
                         *
                         * We zoeken de exacte model-specifieke
                         * toestand in de bestaande state-table.
                         * Zo hoeven we niet te gokken welke knop
                         * verantwoordelijk was voor meerdere
                         * tegelijk veranderde bits.
                         */
                        if (sync_4wire_state_from_dsp(
                                bubbles,
                                jets,
                                pump,
                                physical_heat)) {

                            if (spa.heater != physical_heat) {
                                spa.heater = physical_heat;

                                if (!physical_heat) {
                                    godmode_heater_stage_ = 0;
                                    godmode_heater2_due_ms_ = 0;
                                    godmode_hysteresis_ = false;

                                    if (spa.heater_active) {
                                        spa.heater_active = false;
                                    }
                                }

                                bump_version();
                            }

                            /*
                             * Fysieke bediening annuleert eventuele
                             * oude remote overlays/doelen.
                             */
                            pending_pump_ = false;
                            pending_bubbles_ = false;
                            pending_jets_ = false;

                            godmode_pump_off_pending_ = false;
                            godmode_pump_off_due_ms_ = 0;

                            ESP_LOGI(
                                TAG,
                                "4-wire Godmode: fysieke displaybediening overgenomen");
                        } else {
                            ESP_LOGW(
                                TAG,
                                "4-wire Godmode: fysieke DSP-state niet in modeltabel");
                        }
                    }

                    dsp_pump_ = pump;
                    dsp_bubbles_ = bubbles;
                    dsp_jets_ = jets;
                    dsp_heat_ = physical_heat;

                } else {
                    /*
                     * Bestaand non-Godmode gedrag:
                     * fysieke displaywijzigingen hebben voorrang op
                     * tijdelijke remote overlays.
                     */
                    if (pump != dsp_pump_ && pending_pump_) {
                        pending_pump_ = false;
                        ESP_LOGI(
                            TAG,
                            "4-wire Pump: fysieke bediening neemt over");
                    }

                    if (bubbles != dsp_bubbles_ &&
                        pending_bubbles_) {

                        pending_bubbles_ = false;
                        ESP_LOGI(
                            TAG,
                            "4-wire Bubbles: fysieke bediening neemt over");
                    }

                    if (jets != dsp_jets_ &&
                        pending_jets_) {

                        pending_jets_ = false;
                        ESP_LOGI(
                            TAG,
                            "4-wire Jets: fysieke bediening neemt over");
                    }

                    dsp_pump_ = pump;
                    dsp_bubbles_ = bubbles;
                    dsp_jets_ = jets;
                    dsp_heat_ = physical_heat;
                }

                uint8_t outgoing_frame[FRAME_SIZE];

                /*
                 * Godmode:
                 * de ESP-state-machine is leidend richting CIO/pomp.
                 *
                 * Anti-Godmode:
                 * behoud exact het bestaande DSP-frame en pas alleen
                 * tijdelijke remote overlays toe.
                 */
                if (godmode_) {
                    build_4wire_cio_frame();

                    for (size_t i = 0; i < FRAME_SIZE; ++i) {
                        outgoing_frame[i] = to_cio_frame_[i];
                    }
                } else {
                    for (size_t i = 0; i < FRAME_SIZE; ++i) {
                        outgoing_frame[i] = frame[i];
                    }
                }

                uint8_t outgoing_command = outgoing_frame[2];

                /*
                 * In Godmode bepaalt de thermostaat de echte
                 * heater-stage bits.
                 *
                 * state.heat betekent heater ENABLED/request.
                 * godmode_heater_stage_ betekent werkelijk verwarmen.
                 */
                if (godmode_) {
                    outgoing_command &=
                        static_cast<uint8_t>(
                            ~(masks.heat1 | masks.heat2));

                    if (godmode_heater_stage_ >= 1 &&
                        masks.heat1 != 0) {
                        outgoing_command |= masks.heat1;
                    }

                    if (godmode_heater_stage_ >= 2 &&
                        masks.heat2 != 0) {
                        outgoing_command |= masks.heat2;
                    }

                    /*
                     * Zelfde generatePayload-principe als v3.2.6:
                     * zodra een functie actief is, Power-bit mee.
                     */
                    if (outgoing_command != 0 &&
                        masks.power != 0) {
                        outgoing_command |= masks.power;
                    }
                }

                if (!godmode_ &&
                    pending_pump_ &&
                    masks.pump != 0) {
                    if (target_pump_) {
                        outgoing_command |= masks.pump;
                    } else {
                        outgoing_command &= static_cast<uint8_t>(~masks.pump);
                    }
                }

                if (!godmode_ &&
                    pending_bubbles_ &&
                    masks.bubbles != 0) {
                    if (target_bubbles_) {
                        outgoing_command |= masks.bubbles;
                    } else {
                        outgoing_command &= static_cast<uint8_t>(~masks.bubbles);
                    }
                }

                if (!godmode_ &&
                    pending_jets_ &&
                    masks.jets != 0) {
                    if (target_jets_) {
                        outgoing_command |= masks.jets;
                    } else {
                        outgoing_command &= static_cast<uint8_t>(~masks.jets);
                    }
                }

                /*
                 * Als wij een functie inschakelen, moet ook het Power-bit
                 * aanwezig zijn zoals in de oude 4-wire generatePayload().
                 */
                if (!godmode_ &&
                    ((pending_pump_ && target_pump_) ||
                     (pending_bubbles_ && target_bubbles_) ||
                     (pending_jets_ && target_jets_))) {

                    if (masks.power != 0) {
                        outgoing_command |= masks.power;
                    }
                }

                if (godmode_) {
                    const Bestway4WireState state =
                        bestway_4wire_state(
                            model_,
                            hw_state_index_);

                    bool god_state_changed = false;

                    if (spa.filter != state.pump) {
                        spa.filter = state.pump;
                        god_state_changed = true;
                    }

                    if (spa.bubbles != state.bubbles) {
                        spa.bubbles = state.bubbles;
                        god_state_changed = true;
                    }

                    if (spa.jets != state.jets) {
                        spa.jets = state.jets;
                        god_state_changed = true;
                    }

                    const bool active =
                        godmode_heater_stage_ > 0;

                    if (spa.heater_active != active) {
                        spa.heater_active = active;
                        god_state_changed = true;
                    }

                    if (god_state_changed) {
                        bump_version();
                    }
                }

                outgoing_frame[2] = outgoing_command;

                outgoing_frame[CHECKSUM_INDEX] =
                    static_cast<uint8_t>(
                        outgoing_frame[1] +
                        outgoing_frame[2] +
                        outgoing_frame[3] +
                        outgoing_frame[4]);

                const int written = uart_write_bytes(
                    UART_NUM_0,
                    reinterpret_cast<const char *>(outgoing_frame),
                    FRAME_SIZE);

                if (written != static_cast<int>(FRAME_SIZE)) {
                    ESP_LOGW(TAG, "4-wire DSP->CIO frame niet volledig doorgestuurd");
                }

                ESP_LOGD(TAG,
                         "4-wire DSP geldig: raw=0x%02X tx=0x%02X power=%s pump=%s bubbles=%s jets=%s heat1=%s heat2=%s",
                         command,
                         outgoing_command,
                         power ? "aan" : "uit",
                         pump ? "aan" : "uit",
                         bubbles ? "aan" : "uit",
                         jets ? "aan" : "uit",
                         heat_stage1 ? "aan" : "uit",
                         heat_stage2 ? "aan" : "uit");
            } else {
                ESP_LOGW(TAG,
                         "4-wire DSP checksum fout: ontvangen=%u verwacht=%u",
                         frame[CHECKSUM_INDEX],
                         calculated_checksum);
            }
        }
    }
}

void BestwayBus::hardware_loop(uint64_t now)
{
    if (model_info().family == BestwayProtocolFamily::Uart4Wire) {
        uart4wire_loop(now);

        if (connected_ &&
            last_packet_ms_ != 0 &&
            (now - last_packet_ms_) > 3000) {

            connected_ = false;
            spa.set_connection_state(false);
            bump_version();
            ESP_LOGW(TAG, "4-wire pompverbinding verloren");
        }

        return;
    }

    if (model_info().family == BestwayProtocolFamily::SixWireType1) {
        process_type1_cio_packet();

        // Fysieke knoppen op het originele display uitlezen.
        type1_dsp_poll_button(now);

        // Non-blocking oude v4 temperatuur UP/DOWN sweep.
        type1_audio_loop();

        // Remote knop ingedrukt houden / netjes loslaten.
        type1_remote_button_loop(now);

        // ESP8266/v4 gedrag: normale remote bediening zet eerst
        // Power aan (indien nodig) en ontgrendelt automatisch.
        type1_auto_action_loop(now);

        // Eventuele meerstaps Type1 heater-actie uitvoeren.
        type1_remote_sequence_loop(now);

        // Doeltemperatuur via UP/DOWN naar de pomp sturen.
        type1_target_sequence_loop(now);

        // Pas displaydata sturen nadat een echte CIO-payload ontvangen is.
        if (type1_cio_prev_payload_[0] != 0 ||
            type1_cio_prev_payload_[1] != 0 ||
            type1_cio_prev_payload_[2] != 0 ||
            type1_cio_prev_payload_[3] != 0 ||
            type1_cio_prev_payload_[4] != 0 ||
            type1_cio_prev_payload_[5] != 0 ||
            type1_cio_prev_payload_[6] != 0 ||
            type1_cio_prev_payload_[7] != 0 ||
            type1_cio_prev_payload_[8] != 0 ||
            type1_cio_prev_payload_[9] != 0 ||
            type1_cio_prev_payload_[10] != 0) {

            type1_dsp_upload_payload(now);
        }

        if (connected_ &&
            last_packet_ms_ != 0 &&
            (now - last_packet_ms_) > 8000) {

            connected_ = false;
            spa.set_connection_state(false);
            bump_version();
            ESP_LOGW(TAG, "Type1 pompverbinding verloren");
        }

        return;
    }

    if (model_info().family == BestwayProtocolFamily::SixWireType2) {
        process_type2_cio_packet();

        // Fysieke knop op het originele display uitlezen.
        type2_dsp_poll_button(now);

        // Eventuele virtuele Type2-knop vasthouden / loslaten.
        type2_remote_button_loop(now);

        // Unlock + gevraagde Type2 functie als nette sequence.
        type2_remote_sequence_loop(now);

        // Absolute doeltemperatuur via herhaalde echte UP/DOWN-stappen.
        type2_target_sequence_loop(now);

        /*
         * Pas iets naar het display sturen nadat werkelijk
         * een CIO-payload van de pomp ontvangen is.
         */
        if (type2_cio_prev_payload_[0] != 0 ||
            type2_cio_prev_payload_[1] != 0 ||
            type2_cio_prev_payload_[2] != 0 ||
            type2_cio_prev_payload_[3] != 0 ||
            type2_cio_prev_payload_[4] != 0) {

            type2_dsp_upload_payload(now);
        }

        if (connected_ &&
            last_packet_ms_ != 0 &&
            (now - last_packet_ms_) > 3000) {

            connected_ = false;
            spa.set_connection_state(false);
            bump_version();
            ESP_LOGW(TAG, "Type2 pompverbinding verloren");
        }

        return;
    }
}

