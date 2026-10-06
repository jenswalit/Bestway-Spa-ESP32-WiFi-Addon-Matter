#include "esp_system.h"
#include "esp_mac.h"
#include <cstdio>
#include <cstring>
#include <inttypes.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <mdns.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include "nvs.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_matter.h>
#include <esp_matter_providers.h>
#include <esp_matter_bridge.h>
#include <app/server/Server.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Attributes.h>

#include "spa_state.h"
#include "bestway_bus.h"
#include "scheduler_manager.h"
#include "mqtt_manager.h"
#include "web_server.h"
#include "matter_commissioning.h"
#include "status_led.h"
#include <platform/ESP32/NetworkCommissioningDriver.h>
#include <platform/ESP32/ESP32FactoryDataProvider.h>
#include <platform/DeviceInstanceInfoProvider.h>
#include <platform/internal/GenericDeviceInstanceInfoProvider.h>
#include <platform/ESP32/ConfigurationManagerImpl.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;

namespace MatterThermostat = chip::app::Clusters::Thermostat;
namespace MatterThermostatAttributes = chip::app::Clusters::Thermostat::Attributes;

static const char *TAG = "bestway_matter_v5";
static node_t *g_node = nullptr;
static uint16_t g_aggregator_ep = 0;
static bool g_syncing_from_bus = false;
static bool g_matter_started = false;

static esp_err_t matter_wifi_sync_for_web(const char *ssid,
                                          const char *password)
{
    if (!g_matter_started) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid) {
        ssid = "";
    }

    if (!password) {
        password = "";
    }

    auto &driver =
        chip::DeviceLayer::NetworkCommissioning::
            ESPWiFiDriver::GetInstance();

    /*
     * De webserver draait niet op de CHIP-thread.
     * Matter Network Commissioning daarom onder CHIP stack-lock wijzigen.
     */
    chip::DeviceLayer::PlatformMgr().LockChipStack();

    auto *networks = driver.GetNetworks();

    chip::DeviceLayer::NetworkCommissioning::Network
        current_network = {};

    bool have_current = false;

    if (networks) {
        have_current = networks->Next(current_network);
        networks->Release();
    }

    char debug_buffer[128] = {};
    chip::MutableCharSpan debug_text(debug_buffer);
    uint8_t network_index = 0;

    /*
     * ESP32 Matter ondersteunt hier maar één WiFi-netwerk.
     * Wanneer het SSID verandert moet het oude staging-network
     * dus eerst worden verwijderd.
     */
    if (have_current) {
        const bool same_network =
            current_network.networkIDLen == strlen(ssid) &&
            memcmp(current_network.networkID,
                   ssid,
                   current_network.networkIDLen) == 0;

        if (!same_network || ssid[0] == '\0') {
            chip::ByteSpan old_network(
                current_network.networkID,
                current_network.networkIDLen);

            auto remove_status =
                driver.RemoveNetwork(
                    old_network,
                    debug_text,
                    network_index);

            if (remove_status != decltype(remove_status)::kSuccess) {
                chip::DeviceLayer::PlatformMgr().UnlockChipStack();

                ESP_LOGE(TAG,
                         "Matter oud WiFi-netwerk verwijderen mislukt");

                return ESP_FAIL;
            }
        }
    }

    /*
     * Leeg SSID betekent: WiFi vergeten.
     */
    if (ssid[0] == '\0') {
        const CHIP_ERROR commit_err =
            driver.CommitConfiguration();

        chip::DeviceLayer::PlatformMgr().UnlockChipStack();

        if (commit_err != CHIP_NO_ERROR) {
            ESP_LOGE(TAG,
                     "Matter lege WiFi-config commit mislukt");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG,
                 "Matter WiFi-netwerk gewist");

        return ESP_OK;
    }

    /*
     * Nieuw/huidig netwerk in Matter staging zetten.
     */
    chip::ByteSpan ssid_span(
        reinterpret_cast<const uint8_t *>(ssid),
        strlen(ssid));

    chip::ByteSpan password_span(
        reinterpret_cast<const uint8_t *>(password),
        strlen(password));

    debug_text = chip::MutableCharSpan(debug_buffer);
    network_index = 0;

    auto add_status =
        driver.AddOrUpdateNetwork(
            ssid_span,
            password_span,
            debug_text,
            network_index);

    if (add_status != decltype(add_status)::kSuccess) {
        chip::DeviceLayer::PlatformMgr().UnlockChipStack();

        ESP_LOGE(TAG,
                 "Matter AddOrUpdateNetwork mislukt");

        return ESP_FAIL;
    }

    const CHIP_ERROR commit_err =
        driver.CommitConfiguration();

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();

    if (commit_err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG,
                 "Matter WiFi-config commit mislukt");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "Matter WiFi-config gesynchroniseerd: SSID '%s'",
             ssid);

    return ESP_OK;
}

static bool matter_runtime_active_for_web()
{
    return g_matter_started;
}

static bool matter_is_commissioned_for_web()
{
    if (!g_matter_started) {
        return false;
    }
    chip::DeviceLayer::PlatformMgr().LockChipStack();
    const bool commissioned = chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    return commissioned;
}

static esp_err_t matter_factory_reset_for_web()
{
    if (!g_matter_started) {
        ESP_LOGW(TAG, "Matter reset geweigerd: Matter is uitgeschakeld");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGW(TAG, "Matter decommissioning: fabrics + bridgegegevens wissen");
    ESP_LOGW(TAG, "WiFi/MQTT/webconfiguratie blijft behouden");

    /*
     * Eerst alleen de dynamische bridge-metadata wissen.
     * Dit staat in de aparte bridge_info-partitie.
     */
    const esp_err_t bridge_err = esp_matter_bridge::factory_reset();
    if (bridge_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Matter bridge reset mislukt: %s",
                 esp_err_to_name(bridge_err));
        return bridge_err;
    }

    /*
     * Alleen de Matter fabrics verwijderen.
     *
     * GEEN esp_matter::factory_reset() gebruiken:
     * die reset ook CHIP/Matter persistente configuratie en kan daardoor
     * de Matter-WiFi credentials verwijderen.
     */
    chip::DeviceLayer::PlatformMgr().LockChipStack();

    auto &fabric_table =
        chip::Server::GetInstance().GetFabricTable();

    const uint8_t fabric_count = fabric_table.FabricCount();

    ESP_LOGW(TAG,
             "%u Matter fabric(s) verwijderen",
             static_cast<unsigned>(fabric_count));

    fabric_table.DeleteAllFabrics();
    const bool deleted = fabric_table.FabricCount() == 0;

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    if (!deleted) return ESP_FAIL;
    const esp_err_t credentials_err = bestway_matter_rotate_credentials();
    if (credentials_err != ESP_OK) return credentials_err;

    ESP_LOGW(TAG,
             "Matter gedecommissioned; WiFi blijft behouden");
    ESP_LOGW(TAG,
             "Controller wordt opnieuw gestart voor schone commissioning");

    /*
     * Korte vertraging zodat logs/NVS-writes netjes afgerond worden.
     * Na reboot opent Matter met 0 fabrics automatisch commissioning.
     */
    vTaskDelay(pdMS_TO_TICKS(750));
    esp_restart();

    return ESP_OK;
}

enum class SpaFunction : uint8_t {
    Power,
    Filter,
    Bubbles,
    Jets,
    Heater,
};

struct BridgeItem {
    const char *name;
    SpaFunction function;
    bool state;
    esp_matter_bridge::device_t *device;
    uint16_t endpoint_id;
};

struct ThermostatItem {
    const char *name;
    esp_matter_bridge::device_t *device;
    uint16_t endpoint_id;
    int last_temperature_c;
    int last_target_c;
    bool last_heat_enabled;
};

static BridgeItem g_items[] = {
    {"Spa Power",  SpaFunction::Power,   false, nullptr, 0},
    {"Filter",     SpaFunction::Filter,  false, nullptr, 0},
    {"Bubbels",    SpaFunction::Bubbles, false, nullptr, 0},
    {"Jets",       SpaFunction::Jets,    false, nullptr, 0},
    {"Verwarming", SpaFunction::Heater,  false, nullptr, 0},
};

static ThermostatItem g_thermostat = {
    "Spa Thermostaat", nullptr, 0, -100, -100, false
};

static constexpr int16_t THERMOSTAT_MIN_CENTI_C = 2000;
static constexpr int16_t THERMOSTAT_MAX_CENTI_C = 4000;
static constexpr uint8_t THERMOSTAT_MODE_OFF = 0;
static constexpr uint8_t THERMOSTAT_MODE_HEAT = 4;

static bool spa_value_for(const BridgeItem &item)
{
    switch (item.function) {
        case SpaFunction::Power:   return spa.power;
        case SpaFunction::Filter:  return spa.filter;
        case SpaFunction::Bubbles: return spa.bubbles;
        case SpaFunction::Jets:    return spa.jets;
        case SpaFunction::Heater:
            if (bestway_bus.model_info().family == BestwayProtocolFamily::Uart4Wire) {
                return spa.heater_active;
            }
            return spa.heater;
    }
    return false;
}

static esp_err_t update_i16_attribute(uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, int16_t value)
{
    attribute_t *attr = attribute::get(endpoint_id, cluster_id, attribute_id);
    if (!attr) return ESP_ERR_NOT_FOUND;
    esp_matter_attr_val_t attr_val = esp_matter_invalid(nullptr);
    esp_err_t err = attribute::get_val(attr, &attr_val);
    if (err != ESP_OK) return err;
    attr_val.val.i16 = value;
    return attribute::update(endpoint_id, cluster_id, attribute_id, &attr_val);
}

static esp_err_t update_u8_attribute(uint16_t endpoint_id, uint32_t cluster_id,
                                     uint32_t attribute_id, uint8_t value)
{
    attribute_t *attr = attribute::get(endpoint_id, cluster_id, attribute_id);
    if (!attr) return ESP_ERR_NOT_FOUND;
    esp_matter_attr_val_t attr_val = esp_matter_invalid(nullptr);
    esp_err_t err = attribute::get_val(attr, &attr_val);
    if (err != ESP_OK) return err;
    attr_val.val.u8 = value;
    return attribute::update(endpoint_id, cluster_id, attribute_id, &attr_val);
}

static void sync_thermostat_from_spa()
{
    if (g_thermostat.endpoint_id == 0) return;

    const int temp_c = spa.temperature_c;
    const int target_c = spa.target_temperature_c;
    const bool heat_enabled = spa.heater;

    if (temp_c != g_thermostat.last_temperature_c) {
        const esp_err_t err = update_i16_attribute(
            g_thermostat.endpoint_id,
            MatterThermostat::Id,
            MatterThermostatAttributes::LocalTemperature::Id,
            static_cast<int16_t>(temp_c * 100));
        if (err == ESP_OK) {
            g_thermostat.last_temperature_c = temp_c;
            ESP_LOGI(TAG, "SpaState -> Matter thermostaat: water=%d C", temp_c);
        } else {
            ESP_LOGE(TAG, "LocalTemperature update mislukt: %s", esp_err_to_name(err));
        }
    }

    if (spa.target_temperature_valid &&
        target_c != g_thermostat.last_target_c) {

        const esp_err_t err = update_i16_attribute(
            g_thermostat.endpoint_id,
            MatterThermostat::Id,
            MatterThermostatAttributes::OccupiedHeatingSetpoint::Id,
            static_cast<int16_t>(target_c * 100));

        if (err == ESP_OK) {
            g_thermostat.last_target_c = target_c;

            ESP_LOGI(
                TAG,
                "SpaState -> Matter thermostaat: doel=%d C",
                target_c);
        } else {
            ESP_LOGE(
                TAG,
                "Heating setpoint update mislukt: %s",
                esp_err_to_name(err));
        }
    }

    if (heat_enabled != g_thermostat.last_heat_enabled) {
        const esp_err_t err = update_u8_attribute(
            g_thermostat.endpoint_id,
            MatterThermostat::Id,
            MatterThermostatAttributes::SystemMode::Id,
            heat_enabled ? THERMOSTAT_MODE_HEAT : THERMOSTAT_MODE_OFF);
        if (err == ESP_OK) {
            g_thermostat.last_heat_enabled = heat_enabled;
            ESP_LOGI(TAG, "SpaState -> Matter thermostaat: %s", heat_enabled ? "HEAT" : "OFF");
        } else {
            ESP_LOGE(TAG, "SystemMode update mislukt: %s", esp_err_to_name(err));
        }
    }
}

static void sync_matter_from_spa()
{
    if (!g_matter_started) {
        return;
    }

    g_syncing_from_bus = true;

    for (auto &item : g_items) {
        const bool actual = spa_value_for(item);
        if (actual == item.state || item.endpoint_id == 0) continue;

        esp_matter_attr_val_t value = esp_matter_bool(actual);
        const esp_err_t err = attribute::update(
            item.endpoint_id,
            chip::app::Clusters::OnOff::Id,
            chip::app::Clusters::OnOff::Attributes::OnOff::Id,
            &value);
        if (err == ESP_OK) {
            item.state = actual;
            ESP_LOGI(TAG, "SpaState -> Matter: %s = %s", item.name, actual ? "AAN" : "UIT");
        } else {
            ESP_LOGE(TAG, "Matter statusupdate mislukt voor %s: %s", item.name, esp_err_to_name(err));
        }
    }

    sync_thermostat_from_spa();
    g_syncing_from_bus = false;
}

static esp_err_t add_bridge_device_type(endpoint_t *ep, uint32_t device_type_id, void *)
{
    if (device_type_id == ESP_MATTER_ON_OFF_PLUG_IN_UNIT_DEVICE_TYPE_ID) {
        on_off_plug_in_unit::config_t cfg;
        return on_off_plug_in_unit::add(ep, &cfg);
    }

    if (device_type_id == ESP_MATTER_THERMOSTAT_DEVICE_TYPE_ID) {
        thermostat::config_t cfg;

        cfg.thermostat.local_temperature =
            static_cast<int16_t>(spa.temperature_c * 100);

        cfg.thermostat.control_sequence_of_operation = 2; // heating only
        cfg.thermostat.system_mode =
            spa.heater ? THERMOSTAT_MODE_HEAT : THERMOSTAT_MODE_OFF;

        // ESP-Matter v1.5:
        // Heat/Cool is O.a+ and at least one feature must be enabled
        // BEFORE the Thermostat cluster is created.
        cfg.thermostat.feature_flags =
            cluster::thermostat::feature::heating::get_id();

        /*
         * Matter vereist bij creatie een geldige setpointwaarde.
         * Zolang de echte pomp-target nog niet geleerd is gebruiken
         * we 20 C uitsluitend als protocol-fallback.
         *
         * sync_thermostat_from_spa() publiceert pas zodra
         * target_temperature_valid == true.
         */
        const int initial_target_c =
            spa.target_temperature_valid
                ? spa.target_temperature_c
                : 20;

        cfg.thermostat.features.heating.occupied_heating_setpoint =
            static_cast<int16_t>(initial_target_c * 100);

        return thermostat::add(ep, &cfg);
    }

    ESP_LOGE(TAG, "Unsupported bridged device type: 0x%08" PRIx32, device_type_id);
    return ESP_ERR_NOT_SUPPORTED;
}

static bool apply_bridge_identity(endpoint_t *ep, const char *name)
{
    cluster_t *info_cluster = cluster::get(
        ep, chip::app::Clusters::BridgedDeviceBasicInformation::Id);
    if (!info_cluster) {
        ESP_LOGE(TAG, "BridgedDeviceBasicInformation ontbreekt voor %s", name);
        return false;
    }

    char mutable_name[33] = {};
    std::snprintf(mutable_name, sizeof(mutable_name), "%s", name);

    auto *node_label = cluster::bridged_device_basic_information::attribute::create_node_label(
        info_cluster, mutable_name, std::strlen(mutable_name));
    auto *product_name = cluster::bridged_device_basic_information::attribute::create_product_name(
        info_cluster, mutable_name, std::strlen(mutable_name));

    if (!node_label || !product_name) {
        ESP_LOGE(TAG, "Naam-attributen konden niet worden gemaakt voor %s", name);
        return false;
    }
    return true;
}


static void apply_matter_model_capabilities()
{
    for (auto &item : g_items) {
        bool should_enable = true;

        if (item.function == SpaFunction::Bubbles) {
            should_enable = bestway_bus.has_air();

        } else if (item.function == SpaFunction::Jets) {
            should_enable = bestway_bus.has_jets();

        } else if (item.function == SpaFunction::Power) {
            /*
             * Power is alleen bewezen bestuurbaar voor 6-wire Type1.
             */
            should_enable =
                bestway_bus.model_info().family ==
                    BestwayProtocolFamily::SixWireType1;

        } else if (item.function == SpaFunction::Filter) {
            /*
             * Filter is voor alle ondersteunde modellen beschikbaar.
             */
            should_enable = true;

        } else if (item.function == SpaFunction::Heater) {
            /*
             * 6-wire: verwarming normaal beschikbaar.
             * 4-wire: alleen bestuurbaar wanneer Godmode actief is.
             */
            should_enable =
                bestway_bus.model_info().family !=
                    BestwayProtocolFamily::Uart4Wire ||
                bestway_bus.godmode();

        } else {
            continue;
        }

        if (!item.device || !item.device->endpoint) {
            continue;
        }

        endpoint_t *ep = item.device->endpoint;
        esp_err_t err = should_enable
            ? endpoint::enable(ep)
            : endpoint::disable(ep);

        if (err == ESP_OK) {
            ESP_LOGI(TAG,
                     "Matter endpoint %s: %s",
                     item.name,
                     should_enable ? "ingeschakeld" : "uitgeschakeld");
        } else {
            ESP_LOGE(TAG,
                     "Matter endpoint %s wijzigen mislukt: %s",
                     item.name,
                     esp_err_to_name(err));
        }
    }

    /*
     * De thermostaat wordt, net als de schakel-endpoints, pas nadat
     * ESP-Matter gestart is hervat/aangemaakt. Daarom ook deze endpoint
     * expliciet activeren zodat hij in Matter zichtbaar wordt.
     */
    if (g_thermostat.device && g_thermostat.device->endpoint) {
        esp_err_t err = endpoint::enable(g_thermostat.device->endpoint);

        if (err == ESP_OK) {
            ESP_LOGI(TAG,
                     "Matter endpoint Spa Thermostaat: ingeschakeld");
        } else {
            ESP_LOGE(TAG,
                     "Matter endpoint Spa Thermostaat inschakelen mislukt: %s",
                     esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG,
                 "Matter endpoint Spa Thermostaat ontbreekt");
    }
}

static void apply_spa_command(BridgeItem &item, bool on)
{
    switch (item.function) {
        case SpaFunction::Power:   bestway_bus.set_power(on); break;
        case SpaFunction::Filter:  bestway_bus.set_filter(on); break;
        case SpaFunction::Bubbles: bestway_bus.set_bubbles(on); break;
        case SpaFunction::Jets:    bestway_bus.set_jets(on); break;
        case SpaFunction::Heater:  bestway_bus.set_heater(on); break;
    }
}

static esp_err_t attribute_update_cb(attribute::callback_type_t type,
                                     uint16_t endpoint_id,
                                     uint32_t cluster_id,
                                     uint32_t attribute_id,
                                     esp_matter_attr_val_t *val,
                                     void *priv_data)
{
    if (type != esp_matter::attribute::PRE_UPDATE) return ESP_OK;

    if (cluster_id == chip::app::Clusters::OnOff::Id &&
        attribute_id == chip::app::Clusters::OnOff::Attributes::OnOff::Id &&
        priv_data) {
        auto *item = static_cast<BridgeItem *>(priv_data);
        const bool requested = val->val.b;
        if (g_syncing_from_bus) {
            item->state = requested;
            return ESP_OK;
        }

        if (bestway_bus.model_info().family ==
            BestwayProtocolFamily::Uart4Wire) {

            if (item->function == SpaFunction::Power) {
                ESP_LOGW(
                    TAG,
                    "Matter Power schrijven geweigerd: "
                    "4-wire heeft geen zelfstandige bewezen Power-bediening");

                return ESP_ERR_NOT_SUPPORTED;
            }

            if (item->function == SpaFunction::Heater &&
                !bestway_bus.godmode()) {

                ESP_LOGW(
                    TAG,
                    "Matter Heater schrijven geweigerd: "
                    "4-wire vereist Godmode");

                return ESP_ERR_NOT_SUPPORTED;
            }
        }

        ESP_LOGI(TAG, "Google Home -> %s = %s", item->name, requested ? "AAN" : "UIT");
        apply_spa_command(*item, requested);
        return ESP_OK;
    }

    if (endpoint_id == g_thermostat.endpoint_id &&
        cluster_id == chip::app::Clusters::Thermostat::Id) {
        if (g_syncing_from_bus) return ESP_OK;

        if (attribute_id == MatterThermostatAttributes::OccupiedHeatingSetpoint::Id) {
            if (bestway_bus.model_info().family ==
                    BestwayProtocolFamily::Uart4Wire &&
                !bestway_bus.godmode()) {

                ESP_LOGW(
                    TAG,
                    "Matter temperatuurdoel schrijven geweigerd: "
                    "4-wire vereist Godmode");

                return ESP_ERR_NOT_SUPPORTED;
            }

            int16_t requested = val->val.i16;
            if (requested < THERMOSTAT_MIN_CENTI_C) requested = THERMOSTAT_MIN_CENTI_C;
            if (requested > THERMOSTAT_MAX_CENTI_C) requested = THERMOSTAT_MAX_CENTI_C;
            val->val.i16 = requested;

            const int target_c = (requested + 50) / 100;
            ESP_LOGI(TAG, "Google Home -> Spa Thermostaat doel = %d C", target_c);
            bestway_bus.set_target_temperature(target_c);
            return ESP_OK;
        }

        if (attribute_id == MatterThermostatAttributes::SystemMode::Id) {
            if (bestway_bus.model_info().family ==
                    BestwayProtocolFamily::Uart4Wire &&
                !bestway_bus.godmode()) {

                ESP_LOGW(
                    TAG,
                    "Matter thermostaatmodus schrijven geweigerd: "
                    "4-wire vereist Godmode");

                return ESP_ERR_NOT_SUPPORTED;
            }

            const uint8_t mode = val->val.u8;
            if (mode == THERMOSTAT_MODE_OFF) {
                ESP_LOGI(TAG, "Google Home -> Spa Thermostaat = OFF");
                bestway_bus.set_heater(false);
                return ESP_OK;
            }
            if (mode == THERMOSTAT_MODE_HEAT) {
                ESP_LOGI(TAG, "Google Home -> Spa Thermostaat = HEAT");
                bestway_bus.set_heater(true);
                return ESP_OK;
            }
            ESP_LOGW(TAG, "Niet-ondersteunde thermostaatmodus %u; alleen OFF/HEAT", mode);
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_OK;
}

static esp_err_t identification_cb(identification::callback_type_t type,
                                   uint16_t endpoint_id,
                                   uint8_t effect_id,
                                   uint8_t effect_variant,
                                   void *)
{
    ESP_LOGI(TAG, "Identify endpoint=%u type=%u effect=%u variant=%u",
             endpoint_id, static_cast<unsigned>(type), effect_id, effect_variant);
    return ESP_OK;
}

static void device_event_cb(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t)
{
    switch (event->Type) {
        case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
            ESP_LOGI(TAG, "Matter commissioning COMPLEET");
            break;
        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
            ESP_LOGI(TAG, "Matter commissioning window OPEN");
            break;
        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
            ESP_LOGI(TAG, "Matter commissioning window DICHT");
            break;
        case chip::DeviceLayer::DeviceEventType::kInternetConnectivityChange:
            ESP_LOGI(TAG, "Netwerkstatus gewijzigd");
            break;
        default:
            break;
    }
}

static bool create_named_bridge_item(BridgeItem &item)
{
    item.device = esp_matter_bridge::create_device(
        g_node, g_aggregator_ep, ESP_MATTER_ON_OFF_PLUG_IN_UNIT_DEVICE_TYPE_ID, &item);
    if (!item.device) {
        ESP_LOGE(TAG, "Bridged device maken mislukt: %s", item.name);
        return false;
    }

    endpoint_t *ep = item.device->endpoint;
    if (!apply_bridge_identity(ep, item.name)) return false;

    item.endpoint_id = endpoint::get_id(ep);
    ESP_LOGI(TAG, "Bridged endpoint %u = %s", item.endpoint_id, item.name);
    return true;
}


static bool configure_thermostat_limits(endpoint_t *ep)
{
    cluster_t *thermostat_cluster =
        cluster::get(ep, chip::app::Clusters::Thermostat::Id);

    if (!thermostat_cluster) {
        ESP_LOGE(TAG, "Thermostat cluster ontbreekt");
        return false;
    }

    if (!esp_matter::attribute::get(
            thermostat_cluster,
            chip::app::Clusters::Thermostat::Attributes::AbsMinHeatSetpointLimit::Id)) {
        esp_matter::cluster::thermostat::attribute::
            create_abs_min_heat_setpoint_limit(
                thermostat_cluster, THERMOSTAT_MIN_CENTI_C);
    }

    if (!esp_matter::attribute::get(
            thermostat_cluster,
            chip::app::Clusters::Thermostat::Attributes::AbsMaxHeatSetpointLimit::Id)) {
        esp_matter::cluster::thermostat::attribute::
            create_abs_max_heat_setpoint_limit(
                thermostat_cluster, THERMOSTAT_MAX_CENTI_C);
    }

    if (!esp_matter::attribute::get(
            thermostat_cluster,
            chip::app::Clusters::Thermostat::Attributes::MinHeatSetpointLimit::Id)) {
        esp_matter::cluster::thermostat::attribute::
            create_min_heat_setpoint_limit(
                thermostat_cluster, THERMOSTAT_MIN_CENTI_C);
    }

    if (!esp_matter::attribute::get(
            thermostat_cluster,
            chip::app::Clusters::Thermostat::Attributes::MaxHeatSetpointLimit::Id)) {
        esp_matter::cluster::thermostat::attribute::
            create_max_heat_setpoint_limit(
                thermostat_cluster, THERMOSTAT_MAX_CENTI_C);
    }

    ESP_LOGI(TAG, "Thermostaat grenzen actief: 20-40 C");
    return true;
}

static bool create_named_thermostat()
{
    g_thermostat.device = esp_matter_bridge::create_device(
        g_node, g_aggregator_ep, ESP_MATTER_THERMOSTAT_DEVICE_TYPE_ID, &g_thermostat);
    if (!g_thermostat.device) {
        ESP_LOGE(TAG, "Spa Thermostaat bridged device maken mislukt");
        return false;
    }

    endpoint_t *ep = g_thermostat.device->endpoint;
    if (!apply_bridge_identity(ep, g_thermostat.name)) return false;

    if (!configure_thermostat_limits(ep)) {
        return false;
    }

    g_thermostat.endpoint_id = endpoint::get_id(ep);
    ESP_LOGI(TAG, "Bridged endpoint %u = %s (20-40 C)",
             g_thermostat.endpoint_id, g_thermostat.name);
    return true;
}


static bool restore_or_create_bridge_devices()
{
    constexpr size_t switch_count = sizeof(g_items) / sizeof(g_items[0]);
    constexpr size_t total_count = switch_count + 1; // + thermostaat

    uint16_t saved[MAX_BRIDGED_DEVICE_COUNT];

    for (size_t i = 0; i < MAX_BRIDGED_DEVICE_COUNT; ++i) {
        saved[i] = chip::kInvalidEndpointId;
    }

    esp_err_t err = esp_matter_bridge::get_bridged_endpoint_ids(saved);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Geen opgeslagen bridge-endpoints gevonden: %s",
                 esp_err_to_name(err));
    }

    uint16_t valid[total_count];
    size_t valid_count = 0;

    for (size_t i = 0;
         i < MAX_BRIDGED_DEVICE_COUNT && valid_count < total_count;
         ++i) {

        if (saved[i] == chip::kInvalidEndpointId) {
            continue;
        }

        bool duplicate = false;

        for (size_t j = 0; j < valid_count; ++j) {
            if (valid[j] == saved[i]) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            valid[valid_count++] = saved[i];
        }
    }

    if (valid_count == total_count) {
        ESP_LOGI(TAG, "%u bestaande bridge-endpoints gevonden -> resume",
                 (unsigned)valid_count);


        for (size_t i = 0; i < switch_count; ++i) {
            g_items[i].device = esp_matter_bridge::resume_device(
                g_node,
                valid[i],
                &g_items[i]);

            if (!g_items[i].device) {
                ESP_LOGE(TAG,
                         "Resume mislukt voor %s op endpoint %u",
                         g_items[i].name,
                         valid[i]);
                return false;
            }

            endpoint_t *ep = g_items[i].device->endpoint;
            g_items[i].endpoint_id = endpoint::get_id(ep);

            if (!apply_bridge_identity(ep, g_items[i].name)) {
                return false;
            }

            ESP_LOGI(TAG,
                     "Bridged endpoint %u hervat = %s",
                     g_items[i].endpoint_id,
                     g_items[i].name);
        }

        g_thermostat.device = esp_matter_bridge::resume_device(
            g_node,
            valid[switch_count],
            &g_thermostat);

        if (!g_thermostat.device) {
            ESP_LOGE(TAG,
                     "Resume Spa Thermostaat mislukt op endpoint %u",
                     valid[switch_count]);
            return false;
        }

        endpoint_t *thermo_ep = g_thermostat.device->endpoint;
        g_thermostat.endpoint_id = endpoint::get_id(thermo_ep);

        if (!apply_bridge_identity(thermo_ep, g_thermostat.name)) {
            return false;
        }

        if (!configure_thermostat_limits(thermo_ep)) {
            return false;
        }

        ESP_LOGI(TAG,
                 "Bridged endpoint %u hervat = %s",
                 g_thermostat.endpoint_id,
                 g_thermostat.name);

        return true;
    }

    if (valid_count != 0) {
        ESP_LOGW(TAG,
                 "Onvolledige bridge-inhoud: %u unieke endpoints, verwacht %u -> bridge-opslag herstellen",
                 (unsigned)valid_count,
                 (unsigned)total_count);

        esp_err_t reset_err = esp_matter_bridge::factory_reset();
        if (reset_err != ESP_OK) {
            ESP_LOGE(TAG,
                     "Bridge-opslag herstellen mislukt: %s",
                     esp_err_to_name(reset_err));
            return false;
        }

        ESP_LOGI(TAG,
                 "Oude bridge-endpoints gewist -> complete bridge opnieuw aanmaken");
    }

    ESP_LOGI(TAG, "Bridge-endpoints nieuw aanmaken");

    for (auto &item : g_items) {
        if (!create_named_bridge_item(item)) {
            return false;
        }
    }

    if (!create_named_thermostat()) {
        return false;
    }

    return true;
}


static esp_timer_handle_t g_wifi_recovery_timer = nullptr;
static bool g_recovery_ap_active = false;
static esp_netif_t *g_recovery_ap_netif = nullptr;

static void stop_recovery_ap()
{
    wifi_mode_t mode = WIFI_MODE_NULL;

    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        return;
    }

    if (mode == WIFI_MODE_APSTA || mode == WIFI_MODE_AP) {
        ESP_LOGI(TAG, "WiFi verbonden -> LazySpaSetup uitschakelen");

        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Recovery AP uitschakelen mislukt: %s",
                     esp_err_to_name(err));
            return;
        }
    }

    g_recovery_ap_active = false;
    ESP_LOGI(TAG, "Recovery AP uit; alleen WiFi STA actief");
}

static bool ensure_recovery_ap_netif()
{
    if (!g_recovery_ap_netif) {
        g_recovery_ap_netif =
            esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    }

    if (!g_recovery_ap_netif) {
        g_recovery_ap_netif =
            esp_netif_create_default_wifi_ap();
    }

    if (!g_recovery_ap_netif) {
        ESP_LOGE(TAG, "Recovery AP netwerkinterface aanmaken mislukt");
        return false;
    }

    esp_netif_ip_info_t ip_info = {};

    if (esp_netif_get_ip_info(g_recovery_ap_netif,
                              &ip_info) == ESP_OK) {

        ESP_LOGI(TAG,
                 "Recovery AP netwerkinterface: " IPSTR,
                 IP2STR(&ip_info.ip));
    }

    return true;
}

static void start_recovery_ap()
{
    // Controleer eerst of STA ondertussen alweer verbonden is.
    wifi_ap_record_t ap_info = {};
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi is alweer verbonden; recovery AP niet nodig");
        return;
    }

    wifi_config_t ap_cfg = {};

    const char *ssid = "LazySpaSetup";
    strlcpy(reinterpret_cast<char *>(ap_cfg.ap.ssid),
            ssid,
            sizeof(ap_cfg.ap.ssid));

    ap_cfg.ap.ssid_len = strlen(ssid);
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    ap_cfg.ap.pmf_cfg.required = false;

    ESP_LOGW(TAG,
             "WiFi al 45 seconden weg -> recovery AP LazySpaSetup starten");

    if (!ensure_recovery_ap_netif()) {
        return;
    }

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AP+STA mode instellen mislukt: %s",
                 esp_err_to_name(err));
        return;
    }

    err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LazySpaSetup configureren mislukt: %s",
                 esp_err_to_name(err));
        return;
    }

    /*
     * Matter gebruikt dezelfde WiFi-driver/AP-netif.
     * Zorg daarom expliciet dat onze recovery-AP een vast
     * 192.168.4.1/24 adres én een actieve DHCP-server heeft.
     */
    esp_err_t dhcp_stop_err =
        esp_netif_dhcps_stop(g_recovery_ap_netif);

    if (dhcp_stop_err != ESP_OK &&
        dhcp_stop_err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGW(TAG,
                 "Recovery AP DHCP stoppen gaf: %s",
                 esp_err_to_name(dhcp_stop_err));
    }

    esp_netif_ip_info_t ap_ip = {};
    IP4_ADDR(&ap_ip.ip,      192, 168, 4, 1);
    IP4_ADDR(&ap_ip.gw,      192, 168, 4, 1);
    IP4_ADDR(&ap_ip.netmask, 255, 255, 255, 0);

    err = esp_netif_set_ip_info(g_recovery_ap_netif, &ap_ip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Recovery AP IP 192.168.4.1 instellen mislukt: %s",
                 esp_err_to_name(err));
        return;
    }

    err = esp_netif_dhcps_start(g_recovery_ap_netif);
    if (err != ESP_OK &&
        err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_LOGE(TAG,
                 "Recovery AP DHCP starten mislukt: %s",
                 esp_err_to_name(err));
        return;
    }

    esp_netif_ip_info_t check_ip = {};
    if (esp_netif_get_ip_info(g_recovery_ap_netif, &check_ip) == ESP_OK) {
        ESP_LOGI(TAG,
                 "Recovery AP IP actief: " IPSTR,
                 IP2STR(&check_ip.ip));
    }

    g_recovery_ap_active = true;

    ESP_LOGI(TAG,
             "Recovery AP actief: LazySpaSetup | DHCP actief");
}

static void wifi_recovery_timer_cb(void *)
{
    start_recovery_ap();
}

static void schedule_recovery_ap()
{
    if (!g_wifi_recovery_timer) {
        return;
    }

    // Als LazySpaSetup al actief is, hoeven we niets te doen.
    if (g_recovery_ap_active) {
        return;
    }

    // Belangrijk:
    // bij iedere WiFi reconnect-poging krijgen we opnieuw DISCONNECTED.
    // De 45s timer mag daardoor NIET telkens opnieuw beginnen.
    if (esp_timer_is_active(g_wifi_recovery_timer)) {
        return;
    }

    esp_err_t err = esp_timer_start_once(
        g_wifi_recovery_timer,
        45ULL * 1000000ULL);

    if (err == ESP_OK) {
        ESP_LOGW(TAG,
                 "WiFi verbinding weg -> LazySpaSetup over 45 seconden indien nodig");
    } else {
        ESP_LOGE(TAG,
                 "Recovery timer starten mislukt: %s",
                 esp_err_to_name(err));
    }
}

static void configure_mdns_web_name()
{
    // Zonder Matter is er geen CHIP mDNS-server die de web-hostname voor ons
    // publiceert. Gebruik dan de normale ESP-IDF mDNS-hostname.
    if (!g_matter_started) {
        esp_err_t err = mdns_hostname_set("bestwayspa");
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "mDNS hostname actief zonder Matter: bestwayspa.local");
        } else {
            ESP_LOGW(TAG,
                     "mDNS hostname instellen zonder Matter mislukt: %s",
                     esp_err_to_name(err));
        }
        return;
    }

    esp_netif_t *sta_netif =
        esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    if (!sta_netif) {
        ESP_LOGW(TAG, "Geen STA netif voor mDNS");
        return;
    }

    esp_netif_ip_info_t ip_info = {};

    if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK ||
        ip_info.ip.addr == 0) {

        ESP_LOGW(TAG, "Geen IPv4 adres beschikbaar voor mDNS");
        return;
    }

    // Alleen onze delegated hostname beheren.
    // Matter beheert zijn eigen mDNS hostname en services.
    esp_err_t err =
        mdns_delegate_hostname_remove("bestwayspa");

    if (err != ESP_OK &&
        err != ESP_ERR_NOT_FOUND &&
        err != ESP_ERR_INVALID_STATE) {

        ESP_LOGW(TAG,
                 "Oude bestwayspa mDNS hostname verwijderen gaf: %s",
                 esp_err_to_name(err));
    }

    mdns_ip_addr_t addr = {};
    addr.addr.type = ESP_IPADDR_TYPE_V4;
    addr.addr.u_addr.ip4.addr = ip_info.ip.addr;
    addr.next = nullptr;

    err = mdns_delegate_hostname_add(
        "bestwayspa",
        &addr);

    if (err == ESP_OK) {
        ESP_LOGI(TAG,
                 "mDNS hostname actief: bestwayspa.local -> " IPSTR,
                 IP2STR(&ip_info.ip));
    } else {
        ESP_LOGE(TAG,
                 "bestwayspa.local toevoegen mislukt: %s",
                 esp_err_to_name(err));
    }
}

static bool g_ipv4_recovery_in_progress = false;

static void wifi_recovery_event_handler(void *,
                                        esp_event_base_t event_base,
                                        int32_t event_id,
                                        void *)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_DISCONNECTED) {

        if (g_ipv4_recovery_in_progress) {
            ESP_LOGI(
                TAG,
                "WiFi disconnect hoort bij IPv4 recovery; recovery-AP niet plannen");
            return;
        }

        schedule_recovery_ap();
        return;
    }

    if (event_base == IP_EVENT &&
        event_id == IP_EVENT_STA_GOT_IP) {

        g_ipv4_recovery_in_progress = false;

        if (g_wifi_recovery_timer &&
            esp_timer_is_active(g_wifi_recovery_timer)) {
            esp_timer_stop(g_wifi_recovery_timer);
        }

        stop_recovery_ap();

        // bestwayspa.local opnieuw koppelen aan het actuele STA-IP.
        configure_mdns_web_name();

        // Lokale tijd synchroniseren volgens de gekozen regio/tijdzone.
        web_time_sync_start();

        /*
         * MQTT pas starten wanneer IPv4 werkelijk beschikbaar is.
         * Dit voorkomt een vroege "Host is unreachable" tijdens boot.
         */
        if (!mqtt_manager.connected()) {
            if (!mqtt_manager.start()) {
                ESP_LOGE(TAG, "MQTT starten na verkrijgen IPv4 mislukt");
            }
        }

        ESP_LOGI(TAG, "Netwerkstatus gewijzigd");
    }
}

static esp_err_t init_wifi_recovery()
{
    const esp_timer_create_args_t timer_args = {
        .callback = &wifi_recovery_timer_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wifi_recovery",
        .skip_unhandled_events = true,
    };

    esp_err_t err = esp_timer_create(
        &timer_args,
        &g_wifi_recovery_timer);

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT,
        WIFI_EVENT_STA_DISCONNECTED,
        &wifi_recovery_event_handler,
        nullptr));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_recovery_event_handler,
        nullptr));

    // Als we bij registratie al verbonden zijn: AP meteen uit.
    // Zo niet: recovery-timer starten.
    wifi_ap_record_t ap_info = {};
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        stop_recovery_ap();
        web_time_sync_start();
    } else {
        schedule_recovery_ap();
    }

    return ESP_OK;
}



static void configure_network_names()
{
    const char *hostname = "bestwayspa";

    esp_netif_t *sta_netif =
        esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    if (sta_netif) {
        esp_err_t err = esp_netif_set_hostname(sta_netif, hostname);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Netwerk hostname ingesteld: %s", hostname);
        } else {
            ESP_LOGW(TAG,
                     "Hostname instellen mislukt: %s",
                     esp_err_to_name(err));
        }
    }
}


static void apply_saved_network_config()
{
    nvs_handle_t handle = 0;

    esp_err_t err =
        nvs_open("spa_net", NVS_READONLY, &handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG,
                 "Geen opgeslagen IP-configuratie");
        return;
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "spa_net NVS openen mislukt: %s",
                 esp_err_to_name(err));
        return;
    }

    uint8_t mode = 0;

    err = nvs_get_u8(handle, "mode", &mode);

    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Geen geldige opgeslagen IP-modus");
        nvs_close(handle);
        return;
    }

    size_t ssid_len = 0;

    err = nvs_get_str(
        handle,
        "ssid",
        nullptr,
        &ssid_len);

    if (err != ESP_OK || ssid_len == 0) {
        ESP_LOGW(TAG,
                 "Geen opgeslagen SSID bij IP-configuratie");
        nvs_close(handle);
        return;
    }

    std::string saved_ssid(ssid_len, '\0');

    err = nvs_get_str(
        handle,
        "ssid",
        saved_ssid.data(),
        &ssid_len);

    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Opgeslagen SSID lezen mislukt");
        nvs_close(handle);
        return;
    }

    if (!saved_ssid.empty() &&
        saved_ssid.back() == '\0') {
        saved_ssid.pop_back();
    }

    wifi_config_t wifi_cfg = {};

    err = esp_wifi_get_config(
        WIFI_IF_STA,
        &wifi_cfg);

    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "WiFi-config lezen mislukt: %s",
                 esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    const std::string configured_ssid(
        reinterpret_cast<const char *>(
            wifi_cfg.sta.ssid));

    if (configured_ssid != saved_ssid) {
        ESP_LOGW(TAG,
                 "Opgeslagen IP-config hoort bij ander SSID; niet toegepast");
        nvs_close(handle);
        return;
    }

    esp_netif_t *sta_netif =
        esp_netif_get_handle_from_ifkey(
            "WIFI_STA_DEF");

    if (!sta_netif) {
        ESP_LOGE(TAG,
                 "WIFI_STA_DEF niet gevonden");
        nvs_close(handle);
        return;
    }

    if (mode == 0) {
        esp_err_t dhcp_err =
            esp_netif_dhcpc_start(sta_netif);

        if (dhcp_err == ESP_OK) {
            ESP_LOGI(TAG,
                     "Opgeslagen netwerkconfig hersteld: DHCP gestart");
        } else if (dhcp_err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
            ESP_LOGI(TAG,
                     "Opgeslagen netwerkconfig: DHCP was al actief");
        } else {
            ESP_LOGW(TAG,
                     "DHCP starten gaf: %s",
                     esp_err_to_name(dhcp_err));
        }

        nvs_close(handle);
        return;
    }

    uint32_t ip = 0;
    uint32_t gw = 0;
    uint32_t mask = 0;
    uint32_t dns1 = 0;
    uint32_t dns2 = 0;

    if (nvs_get_u32(handle, "ip", &ip) != ESP_OK ||
        nvs_get_u32(handle, "gw", &gw) != ESP_OK ||
        nvs_get_u32(handle, "mask", &mask) != ESP_OK) {

        ESP_LOGE(TAG,
                 "Opgeslagen vaste IP-configuratie is incompleet");

        nvs_close(handle);
        return;
    }

    nvs_get_u32(handle, "dns1", &dns1);
    nvs_get_u32(handle, "dns2", &dns2);

    nvs_close(handle);

    /*
     * Als de opgeslagen vaste netwerkconfiguratie al actief is,
     * niets meer aan de WiFi-interface veranderen.
     *
     * Matter/CHIP kan tijdens boot het netwerk al volledig hebben
     * opgebouwd. Een onnodige esp_wifi_disconnect() verbreekt dan
     * bestaande MQTT- en Matter-sessies.
     */
    esp_netif_ip_info_t current_ip_info = {};

    if (esp_netif_get_ip_info(sta_netif, &current_ip_info) == ESP_OK &&
        current_ip_info.ip.addr == ip &&
        current_ip_info.gw.addr == gw &&
        current_ip_info.netmask.addr == mask) {

        ESP_LOGI(TAG,
                 "Opgeslagen vaste IP-configuratie is al actief; netwerk blijft verbonden");

        if (dns1 != 0) {
            esp_netif_dns_info_t dns = {};
            dns.ip.type = ESP_IPADDR_TYPE_V4;
            dns.ip.u_addr.ip4.addr = dns1;
            esp_netif_set_dns_info(
                sta_netif,
                ESP_NETIF_DNS_MAIN,
                &dns);
        }

        if (dns2 != 0) {
            esp_netif_dns_info_t dns = {};
            dns.ip.type = ESP_IPADDR_TYPE_V4;
            dns.ip.u_addr.ip4.addr = dns2;
            esp_netif_set_dns_info(
                sta_netif,
                ESP_NETIF_DNS_BACKUP,
                &dns);
        }

        return;
    }

    /*
     * Alleen wanneer de actieve configuratie werkelijk afwijkt,
     * WiFi kort opnieuw configureren.
     */
    ESP_LOGI(TAG,
             "Actieve netwerkconfig wijkt af; opgeslagen vast IP toepassen");

    esp_wifi_disconnect();

    vTaskDelay(pdMS_TO_TICKS(150));

    esp_err_t dhcp_err =
        esp_netif_dhcpc_stop(sta_netif);

    if (dhcp_err != ESP_OK) {
        ESP_LOGW(TAG,
                 "DHCP stoppen gaf: %s",
                 esp_err_to_name(dhcp_err));
    }

    esp_netif_ip_info_t ip_info = {};
    ip_info.ip.addr = ip;
    ip_info.gw.addr = gw;
    ip_info.netmask.addr = mask;

    err = esp_netif_set_ip_info(
        sta_netif,
        &ip_info);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Opgeslagen vast IP toepassen mislukt: %s",
                 esp_err_to_name(err));
        return;
    }

    if (dns1 != 0) {
        esp_netif_dns_info_t dns = {};
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        dns.ip.u_addr.ip4.addr = dns1;

        esp_netif_set_dns_info(
            sta_netif,
            ESP_NETIF_DNS_MAIN,
            &dns);
    }

    if (dns2 != 0) {
        esp_netif_dns_info_t dns = {};
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        dns.ip.u_addr.ip4.addr = dns2;

        esp_netif_set_dns_info(
            sta_netif,
            ESP_NETIF_DNS_BACKUP,
            &dns);
    }

    ESP_LOGI(TAG,
             "Opgeslagen netwerkconfig hersteld: vast IP");

    const esp_err_t reconnect_err = esp_wifi_connect();

    if (reconnect_err != ESP_OK) {
        ESP_LOGW(TAG,
                 "WiFi opnieuw verbinden na vaste IP-configuratie gaf: %s",
                 esp_err_to_name(reconnect_err));
    } else {
        ESP_LOGI(TAG,
                 "WiFi opnieuw verbinden na vaste IP-configuratie gestart");
    }

}


static esp_err_t init_wifi_without_matter()
{
    ESP_LOGI(TAG, "Matter UIT: gewone ESP-IDF WiFi-stack starten");

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta) {
        sta = esp_netif_create_default_wifi_sta();
        if (!sta) {
            return ESP_FAIL;
        }
    }

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_init_cfg);
    if (err != ESP_OK && err != ESP_ERR_WIFI_INIT_STATE) {
        return err;
    }

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t saved_cfg = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &saved_cfg) == ESP_OK &&
        saved_cfg.sta.ssid[0] != 0) {
        ESP_LOGI(TAG, "Opgeslagen WiFi-netwerk gevonden; verbinden zonder Matter");
        err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "WiFi verbinden starten gaf: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "Geen opgeslagen WiFi-SSID; recovery AP zal beschikbaar komen");
    }

    err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "mDNS initialiseren zonder Matter gaf: %s", esp_err_to_name(err));
    } else {
        // In Matter-UIT-modus moet ESP-IDF mDNS zelf bestwayspa.local
        // publiceren. Bij Matter-AAN verzorgt CHIP/Matter zijn eigen mDNS.
        esp_err_t mdns_err = mdns_hostname_set("bestwayspa");
        if (mdns_err == ESP_OK) {
            ESP_LOGI(TAG, "mDNS hostname ingesteld zonder Matter: bestwayspa.local");
        } else {
            ESP_LOGW(TAG,
                     "mDNS hostname instellen zonder Matter gaf: %s",
                     esp_err_to_name(mdns_err));
        }

        mdns_err = mdns_instance_name_set("Bestway Lay-Z-Spa");
        if (mdns_err != ESP_OK) {
            ESP_LOGW(TAG,
                     "mDNS instance name instellen gaf: %s",
                     esp_err_to_name(mdns_err));
        }

        mdns_err = mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
        if (mdns_err != ESP_OK && mdns_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG,
                     "mDNS HTTP service toevoegen gaf: %s",
                     esp_err_to_name(mdns_err));
        }
    }

    return ESP_OK;
}

static void delayed_network_restore_task(void *arg)
{
    // Matter/NimBLE eerst volledig laten opstarten.
    vTaskDelay(pdMS_TO_TICKS(5000));

    ESP_LOGI(TAG,
             "Opgeslagen netwerkconfig vertraagd herstellen");

    apply_saved_network_config();

    vTaskDelete(nullptr);
}


class BestwayDeviceInfoProvider :
    public chip::DeviceLayer::Internal::GenericDeviceInstanceInfoProvider<
        chip::DeviceLayer::Internal::ESP32Config>
{
    using Base =
        chip::DeviceLayer::Internal::GenericDeviceInstanceInfoProvider<
            chip::DeviceLayer::Internal::ESP32Config>;

public:
    BestwayDeviceInfoProvider() :
        Base(chip::DeviceLayer::ConfigurationManagerImpl::GetDefaultInstance())
    {
    }

    CHIP_ERROR GetVendorName(char *buf, size_t bufSize) override
    {
        return copy_string(buf, bufSize, "PtW spa controls");
    }

    CHIP_ERROR GetProductName(char *buf, size_t bufSize) override
    {
        return copy_string(buf, bufSize, "Bestway Spa Bridge");
    }

    CHIP_ERROR GetHardwareVersionString(char *buf, size_t bufSize) override
    {
        return copy_string(buf, bufSize, "BWC_PCB_V2M");
    }

private:
    static CHIP_ERROR copy_string(char *buf,
                                  size_t bufSize,
                                  const char *value)
    {
        const size_t len = std::strlen(value) + 1;

        if (bufSize < len) {
            return CHIP_ERROR_BUFFER_TOO_SMALL;
        }

        std::memcpy(buf, value, len);
        return CHIP_NO_ERROR;
    }
};

static BestwayDeviceInfoProvider g_bestway_device_info_provider;

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    const esp_err_t credentials_err = bestway_matter_credentials_init();
    if (credentials_err != ESP_OK) {
        ESP_LOGE(TAG, "Matter credentials niet beschikbaar: %s; overige functies blijven actief",
                 esp_err_to_name(credentials_err));
    }

    /*
     * bridge_info wordt ook gebruikt wanneer Matter uit staat
     * (o.a. MQTT-configuratie en historie).
     * Daarom deze NVS-partitie altijd expliciet initialiseren.
     *
     * BELANGRIJK:
     * Deze partitie wordt hier nooit automatisch gewist.
     * Bij een fout blijven bestaande MQTT- en historiedata behouden.
     */
    const esp_err_t bridge_nvs_err =
        nvs_flash_init_partition("bridge_info");

    if (bridge_nvs_err == ESP_OK) {

        ESP_LOGI(TAG,
                 "bridge_info NVS-partitie actief");

    } else {

        ESP_LOGE(TAG,
                 "bridge_info NVS initialiseren mislukt: %s",
                 esp_err_to_name(bridge_nvs_err));
    }

    esp_log_level_set("wifi", ESP_LOG_ERROR);

    // Onboard adresseerbare RGB-led van de ESP32-C6-DevKitC-1 (GPIO8).
    // LED-fouten mogen de spa-controller niet tegenhouden.
    const esp_err_t status_led_err = status_led_init();
    if (status_led_err != ESP_OK) {
        ESP_LOGW(TAG, "Status-led niet beschikbaar: %s", esp_err_to_name(status_led_err));
    }

    spa.begin();
    bestway_bus.begin();

    const bool matter_enabled = web_matter_enabled() && credentials_err == ESP_OK;
    ESP_LOGI(TAG, "Matter webinstelling bij boot: %s",
             matter_enabled ? "AAN" : "UIT");

    web_matter_register_callbacks(matter_is_commissioned_for_web,
                                  matter_factory_reset_for_web,
                                  matter_runtime_active_for_web,
                                  matter_wifi_sync_for_web,
                                  bestway_matter_status_json,
                                  bestway_matter_open_window);

    if (matter_enabled) {
        node::config_t node_cfg;
        std::snprintf(node_cfg.root_node.basic_information.node_label,
                      sizeof(node_cfg.root_node.basic_information.node_label),
                      "%s", "Spa Bridge");

        g_node = node::create(&node_cfg, attribute_update_cb, identification_cb);
        if (!g_node) {
            ESP_LOGE(TAG, "Matter node maken mislukt");
            return;
        }


        /*
         * Optionele Matter Basic Information attributen die Google Home
         * tijdens commissioning uitleest.
         */
        {
            endpoint_t *root_ep = endpoint::get(g_node, 0);

            if (root_ep) {
                cluster_t *basic_info =
                    cluster::get(
                        root_ep,
                        chip::app::Clusters::BasicInformation::Id
                    );

                if (basic_info) {
                    /*
                     * ProductLabel
                     */
                    const char *product_label = "Bestway Spa Bridge";

                    attribute_t *product_label_attr =
                        attribute::get(
                            basic_info,
                            chip::app::Clusters::BasicInformation::
                                Attributes::ProductLabel::Id
                        );

                    if (!product_label_attr) {
                        product_label_attr =
                            cluster::basic_information::attribute::
                                create_product_label(
                                    basic_info,
                                    const_cast<char *>(product_label),
                                    strlen(product_label)
                                );
                    }

                    if (product_label_attr) {
                        ESP_LOGI(TAG,
                                 "Matter ProductLabel beschikbaar: %s",
                                 product_label);
                    } else {
                        ESP_LOGW(TAG,
                                 "Matter ProductLabel aanmaken mislukt");
                    }

                    /*
                     * SerialNumber automatisch uit de ESP32 MAC.
                     * Voorbeeld: BWC-V2M-206EF11724C8
                     */
                    uint8_t mac[6] = {};
                    esp_err_t mac_err =
                        esp_read_mac(mac, ESP_MAC_WIFI_STA);

                    if (mac_err == ESP_OK) {
                        char serial_number[32] = {};

                        std::snprintf(
                            serial_number,
                            sizeof(serial_number),
                            "BWC-V2M-%02X%02X%02X%02X%02X%02X",
                            mac[0], mac[1], mac[2],
                            mac[3], mac[4], mac[5]
                        );

                        attribute_t *serial_attr =
                            attribute::get(
                                basic_info,
                                chip::app::Clusters::BasicInformation::
                                    Attributes::SerialNumber::Id
                            );

                        if (!serial_attr) {
                            serial_attr =
                                cluster::basic_information::attribute::
                                    create_serial_number(
                                        basic_info,
                                        serial_number,
                                        strlen(serial_number)
                                    );
                        }

                        if (serial_attr) {
                            ESP_LOGI(TAG,
                                     "Matter SerialNumber beschikbaar: %s",
                                     serial_number);
                        } else {
                            ESP_LOGW(TAG,
                                     "Matter SerialNumber aanmaken mislukt");
                        }
                    } else {
                        ESP_LOGW(TAG,
                                 "MAC lezen voor Matter SerialNumber mislukt: %s",
                                 esp_err_to_name(mac_err));
                    }
                } else {
                    ESP_LOGW(TAG,
                             "Matter Basic Information cluster niet gevonden");
                }
            } else {
                ESP_LOGW(TAG,
                         "Matter root endpoint 0 niet gevonden");
            }
        }


        {
            endpoint_t *root_ep = endpoint::get(g_node, 0);

            if (root_ep) {
                cluster_t *basic_info =
                    cluster::get(
                        root_ep,
                        chip::app::Clusters::BasicInformation::Id
                    );

                if (basic_info) {
                    const char *product_label = "Bestway Spa Bridge";

                    attribute_t *product_label_attr =
                        attribute::get(
                            basic_info,
                            chip::app::Clusters::BasicInformation::
                                Attributes::ProductLabel::Id
                        );

                    if (!product_label_attr) {
                        product_label_attr =
                            cluster::basic_information::attribute::
                                create_product_label(
                                    basic_info,
                                    const_cast<char *>(product_label),
                                    strlen(product_label)
                                );
                    }

                    if (product_label_attr) {
                        ESP_LOGI(TAG,
                                 "Matter ProductLabel beschikbaar: %s",
                                 product_label);
                    } else {
                        ESP_LOGW(TAG,
                                 "Matter ProductLabel aanmaken mislukt");
                    }
                } else {
                    ESP_LOGW(TAG,
                             "Matter Basic Information cluster niet gevonden");
                }
            } else {
                ESP_LOGW(TAG,
                         "Matter root endpoint 0 niet gevonden");
            }
        }


        aggregator::config_t agg_cfg;
        endpoint_t *aggregator_ep =
            aggregator::create(g_node, &agg_cfg, ENDPOINT_FLAG_NONE, nullptr);

        if (!aggregator_ep) {
            ESP_LOGE(TAG, "Aggregator endpoint maken mislukt");
            return;
        }

        g_aggregator_ep = endpoint::get_id(aggregator_ep);
        ESP_LOGI(TAG, "Aggregator endpoint: %u", g_aggregator_ep);

        ESP_ERROR_CHECK(esp_matter_bridge::initialize(g_node, add_bridge_device_type));

        /*
         * Matter eerst starten.
         *
         * Dynamic bridge-endpoints moeten worden aangemaakt/hersteld nadat
         * ESP-Matter gestart is. Daardoor wordt min_unused_endpoint_id uit
         * NVS geladen en worden nieuwe endpoint-ID's ook correct opgeslagen.
         */
        esp_matter::set_custom_device_instance_info_provider(
            &g_bestway_device_info_provider);

        ESP_LOGI(TAG,
                 "Custom Matter device-info provider geregistreerd VOOR Matter-start");

        bestway_matter_register_provider();
        ESP_ERROR_CHECK(esp_matter::start(device_event_cb));
g_matter_started = true;

        ESP_LOGI(TAG,
                 "Matter server gestart -> bridge-endpoints herstellen/aanmaken");

        if (!restore_or_create_bridge_devices()) {
            ESP_LOGE(TAG, "Bridge devices initialiseren mislukt");
            return;
        }

        ESP_LOGI(TAG, "Bestway Matter v5 phase 2.1 klaar");
        ESP_LOGI(TAG, "Matter: Spa Power, Filter, Bubbels, Jets, Verwarming + Spa Thermostaat");
        ESP_LOGI(TAG, "Thermostaat: actuele watertemperatuur + doeltemperatuur 20-40 C");
        ESP_LOGI(TAG, "Bestway bus mode: %s", bestway_bus.mode_name());
        ESP_LOGI(TAG, "SIMULATION: Google Home -> SpaState -> gesimuleerde pomp -> Matter terugkoppeling");

        apply_matter_model_capabilities();
    } else {
        ESP_LOGW(TAG, "Matter is UIT: CHIP/Matter server wordt NIET gestart");
        ESP_ERROR_CHECK(init_wifi_without_matter());
    }

    configure_network_names();

    /*
     * Configuratie alvast laden.
     * De MQTT-verbinding zelf start pas bij IP_EVENT_STA_GOT_IP.
     */
    if (!mqtt_manager.begin()) {
        ESP_LOGE(TAG, "MQTT configuratie initialisatie mislukt");
    }

    ESP_ERROR_CHECK(init_wifi_recovery());

    BaseType_t restore_task_ok = xTaskCreate(
        delayed_network_restore_task,
        "net_restore",
        4096,
        nullptr,
        4,
        nullptr);

    if (restore_task_ok != pdPASS) {
        ESP_LOGE(TAG,
                 "Netwerk herstel-task kon niet worden gestart");
    }

    ESP_ERROR_CHECK(web_server_start());
    ESP_LOGI(TAG, "Webinterface: open http://<ESP-IP>/ in je browser");

    if (!scheduler_manager.begin()) {
        ESP_LOGE(TAG, "Planner initialisatie mislukt");
    }

    uint32_t last_bus_version = bestway_bus.state_version();
    if (g_matter_started) {
        sync_matter_from_spa();
    }

    for (;;) {
        bestway_bus.loop();
        scheduler_manager.loop();
        mqtt_manager.loop();

        wifi_ap_record_t led_wifi_ap = {};
        const bool led_wifi_connected = esp_wifi_sta_get_ap_info(&led_wifi_ap) == ESP_OK;

        /*
         * IPv4 watchdog.
         *
         * Bij sommige koude starts kan WiFi al geassocieerd zijn,
         * terwijl DHCP nog geen IPv4-adres heeft opgeleverd.
         *
         * Na 20 seconden zonder IPv4 wordt WiFi eenmalig opnieuw
         * verbonden. Deze bewuste disconnect mag NIET de recovery-AP
         * triggeren; daarvoor gebruiken we g_ipv4_recovery_in_progress.
         *
         * De bestaande recovery-AP blijft voor echte WiFi-uitval
         * volledig intact.
         */
        static int64_t no_ipv4_since_us = 0;
        static int64_t last_ipv4_recovery_us = 0;

        esp_netif_t *watchdog_sta =
            esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

        esp_netif_ip_info_t watchdog_ip = {};

        const bool have_ipv4 =
            watchdog_sta &&
            esp_netif_get_ip_info(
                watchdog_sta,
                &watchdog_ip) == ESP_OK &&
            watchdog_ip.ip.addr != 0;

        const int64_t now_us = esp_timer_get_time();

        if (led_wifi_connected && !have_ipv4) {
            if (no_ipv4_since_us == 0) {
                no_ipv4_since_us = now_us;
            }

            const bool waited_long_enough =
                (now_us - no_ipv4_since_us) >= 20000000LL;

            const bool retry_allowed =
                last_ipv4_recovery_us == 0 ||
                (now_us - last_ipv4_recovery_us) >= 30000000LL;

            if (waited_long_enough && retry_allowed) {
                ESP_LOGW(
                    TAG,
                    "WiFi verbonden maar geen IPv4 na 20 s -> WiFi/DHCP recovery");

                last_ipv4_recovery_us = now_us;
                no_ipv4_since_us = now_us;

                g_ipv4_recovery_in_progress = true;

                const esp_err_t disconnect_err =
                    esp_wifi_disconnect();

                if (disconnect_err != ESP_OK &&
                    disconnect_err != ESP_ERR_WIFI_NOT_CONNECT) {

                    ESP_LOGW(
                        TAG,
                        "IPv4 recovery disconnect gaf: %s",
                        esp_err_to_name(disconnect_err));
                }

                vTaskDelay(pdMS_TO_TICKS(150));

                const esp_err_t reconnect_err =
                    esp_wifi_connect();

                if (reconnect_err != ESP_OK) {
                    ESP_LOGW(
                        TAG,
                        "IPv4 recovery reconnect gaf: %s",
                        esp_err_to_name(reconnect_err));

                    g_ipv4_recovery_in_progress = false;
                } else {
                    ESP_LOGI(
                        TAG,
                        "IPv4 recovery reconnect gestart");
                }
            }
        } else {
            no_ipv4_since_us = 0;

            if (have_ipv4) {
                g_ipv4_recovery_in_progress = false;
            }
        }

        status_led_update(led_wifi_connected,
                          mqtt_manager.connected(),
                          spa.connected,
                          spa.data_valid,
                          spa.error_code,
                          g_matter_started,
                          g_recovery_ap_active);

        const uint32_t version = bestway_bus.state_version();
        if (version != last_bus_version) {
            last_bus_version = version;
            if (g_matter_started) {
                sync_matter_from_spa();
            }

            // Live UI meteen bijwerken zodra de echte pompstate verandert.
            web_server_push_live_state();
        }

        // Rustige WebSocket-heartbeat voor WiFi/MQTT/uptime-status.
        // Geen HTTP-polling nodig voor de live bedieningsstatus.
        static int64_t last_websocket_heartbeat_us = 0;
        if (last_websocket_heartbeat_us == 0 ||
            (now_us - last_websocket_heartbeat_us) >= 5000000LL) {
            last_websocket_heartbeat_us = now_us;
            web_server_push_live_state();
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
