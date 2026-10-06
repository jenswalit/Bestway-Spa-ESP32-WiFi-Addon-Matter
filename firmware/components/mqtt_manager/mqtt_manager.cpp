#include "mqtt_manager.h"

#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cctype>

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_app_desc.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "spa_state.h"
#include "bestway_bus.h"
#include "scheduler_manager.h"
#include "cJSON.h"
#include "nvs.h"
#include <ctime>

namespace {

static const char *TAG = "mqtt_manager";
static constexpr const char *NVS_NAMESPACE = "spa_mqtt";

static esp_mqtt_client_handle_t mqtt_client = nullptr;
static volatile bool mqtt_connected = false;
static volatile bool mqtt_publish_requested = false;

// MQTT targetfeedback: pas publiceren zodra de echte pomp-target is bevestigd.
static volatile bool mqtt_target_feedback_wait = false;
static volatile int mqtt_target_feedback_desired = 0;
static int64_t mqtt_target_feedback_started_us = 0;

static int64_t mqtt_last_publish_us = 0;

static std::string mqtt_broker_uri;
static std::string mqtt_availability_topic;

enum class PendingMqttCommand : uint8_t {
    None = 0,
    Godmode,
    FullPower,
    Power,
    Heater,
    Filter,
    Bubbles,
    Jets,
    Lock,
    Target
};

static volatile PendingMqttCommand pending_mqtt_command =
    PendingMqttCommand::None;

static volatile bool pending_mqtt_bool = false;
static volatile int pending_mqtt_target = 0;


/*
 * Planner-sync Home Assistant <-> webinterface.
 * De planner in SchedulerManager blijft de enige bron van waarheid.
 * MQTT transporteert alleen wijzigingen en een retained snapshot.
 */
enum class PendingPlannerOp : uint8_t {
    None = 0,
    Add,
    Update,
    Delete,
    DeleteDay,
    SetEnabled,
    Clear
};

struct PendingPlannerCommand {
    PendingPlannerOp op = PendingPlannerOp::None;
    uint16_t id = 0;
    bool enabled = true;
    uint8_t days_mask = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    ScheduleAction action = ScheduleAction::HeaterOn;
    int32_t value = 0;
};

static PendingPlannerCommand pending_planner_command;
static volatile bool pending_planner_command_ready = false;
static volatile bool planner_publish_requested = true;
static uint32_t planner_last_published_revision = 0;

static std::string mqtt_join_topic(
    const std::string &base,
    const std::string &suffix)
{
    std::string topic = base;

    while (!topic.empty() && topic.back() == '/') {
        topic.pop_back();
    }

    size_t start = 0;

    while (start < suffix.size() &&
           suffix[start] == '/') {
        ++start;
    }

    if (!topic.empty()) {
        topic += "/";
    }

    topic += suffix.substr(start);

    return topic;
}

static std::string lower_copy(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
        });

    return value;
}

static bool parse_mqtt_bool(
    const std::string &payload,
    bool &value)
{
    const std::string p =
        lower_copy(payload);

    if (p == "on" ||
        p == "1" ||
        p == "true") {
        value = true;
        return true;
    }

    if (p == "off" ||
        p == "0" ||
        p == "false") {
        value = false;
        return true;
    }

    return false;
}


static bool planner_json_int(
    const cJSON *root,
    const char *key,
    int &value,
    bool required)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);

    if (!item) {
        return !required;
    }

    if (!cJSON_IsNumber(item)) {
        return false;
    }

    value = item->valueint;
    return true;
}

static bool planner_json_bool(
    const cJSON *root,
    const char *key,
    bool &value,
    bool required)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);

    if (!item) {
        return !required;
    }

    if (!cJSON_IsBool(item)) {
        return false;
    }

    value = cJSON_IsTrue(item);
    return true;
}

static bool parse_planner_command(
    const std::string &payload,
    PendingPlannerCommand &out)
{
    cJSON *root = cJSON_Parse(payload.c_str());

    if (!root) {
        return false;
    }

    bool ok = false;
    const cJSON *op_item =
        cJSON_GetObjectItemCaseSensitive(root, "op");

    if (!cJSON_IsString(op_item) ||
        !op_item->valuestring) {
        cJSON_Delete(root);
        return false;
    }

    const std::string op = lower_copy(op_item->valuestring);
    PendingPlannerCommand cmd;

    if (op == "clear") {
        cmd.op = PendingPlannerOp::Clear;
        ok = true;
    } else if (op == "delete" || op == "remove") {
        int id = 0;
        ok = planner_json_int(root, "id", id, true) &&
             id > 0 && id <= 65535;
        if (ok) {
            cmd.op = PendingPlannerOp::Delete;
            cmd.id = static_cast<uint16_t>(id);
        }
    } else if (op == "delete_day" || op == "removeday") {
        int id = 0;
        int day_mask = 0;
        ok = planner_json_int(root, "id", id, true) &&
             planner_json_int(root, "dayMask", day_mask, true) &&
             id > 0 && id <= 65535 &&
             day_mask > 0 && day_mask <= 127;
        if (ok) {
            cmd.op = PendingPlannerOp::DeleteDay;
            cmd.id = static_cast<uint16_t>(id);
            cmd.days_mask = static_cast<uint8_t>(day_mask);
        }
    } else if (op == "enable" || op == "set_enabled") {
        int id = 0;
        bool enabled = true;
        ok = planner_json_int(root, "id", id, true) &&
             planner_json_bool(root, "enabled", enabled, true) &&
             id > 0 && id <= 65535;
        if (ok) {
            cmd.op = PendingPlannerOp::SetEnabled;
            cmd.id = static_cast<uint16_t>(id);
            cmd.enabled = enabled;
        }
    } else if (op == "add" || op == "update") {
        int id = 0;
        int days = 0;
        int hour = 0;
        int minute = 0;
        int action = 0;
        int value = 0;
        bool enabled = true;

        ok = planner_json_int(root, "daysMask", days, true) &&
             planner_json_int(root, "hour", hour, true) &&
             planner_json_int(root, "minute", minute, true) &&
             planner_json_int(root, "action", action, true) &&
             planner_json_int(root, "value", value, false) &&
             planner_json_bool(root, "enabled", enabled, false) &&
             days > 0 && days <= 127 &&
             hour >= 0 && hour <= 23 &&
             minute >= 0 && minute <= 59 &&
             action >= 0 && action <= 8;

        if (ok && action == 8) {
            ok = value >= 20 && value <= 40;
        }

        if (ok && op == "update") {
            ok = planner_json_int(root, "id", id, true) &&
                 id > 0 && id <= 65535;
        }

        if (ok) {
            cmd.op = op == "add"
                ? PendingPlannerOp::Add
                : PendingPlannerOp::Update;
            cmd.id = static_cast<uint16_t>(id);
            cmd.enabled = enabled;
            cmd.days_mask = static_cast<uint8_t>(days);
            cmd.hour = static_cast<uint8_t>(hour);
            cmd.minute = static_cast<uint8_t>(minute);
            cmd.action = static_cast<ScheduleAction>(action);
            cmd.value = value;
        }
    }

    cJSON_Delete(root);

    if (ok) {
        out = cmd;
    }

    return ok;
}

static std::string planner_state_json()
{
    std::string json;
    json.reserve(2200);

    json += "{\"revision\":";
    json += std::to_string(scheduler_manager.revision());
    json += ",\"count\":";
    json += std::to_string(scheduler_manager.count());
    json += ",\"max\":";
    json += std::to_string(SchedulerManager::MAX_SCHEDULES);
    json += ",\"items\":[";

    for (uint8_t i = 0; i < scheduler_manager.count(); ++i) {
        const ScheduleItem *item = scheduler_manager.get(i);
        if (!item) continue;

        if (json.back() != '[') {
            json += ',';
        }

        json += "{\"id\":";
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
        json += std::to_string(static_cast<unsigned>(item->action));
        json += ",\"value\":";
        json += std::to_string(item->value);
        json += '}';
    }

    json += "]}";
    return json;
}

static void publish_planner_result(
    bool ok,
    const char *message)
{
    if (!mqtt_client || !mqtt_connected) {
        return;
    }

    const MqttConfig &cfg = mqtt_manager.config();
    const std::string topic =
        mqtt_join_topic(cfg.base_topic, "planner/result");

    std::string payload = "{\"ok\":";
    payload += ok ? "true" : "false";
    payload += ",\"message\":\"";
    payload += message ? message : "";
    payload += "\"}";

    esp_mqtt_client_publish(
        mqtt_client,
        topic.c_str(),
        payload.c_str(),
        0,
        1,
        0);
}

static void publish_planner_state()
{
    if (!mqtt_client || !mqtt_connected) {
        return;
    }

    const MqttConfig &cfg = mqtt_manager.config();
    const std::string topic =
        mqtt_join_topic(cfg.base_topic, "planner/state");
    const std::string payload = planner_state_json();

    esp_mqtt_client_publish(
        mqtt_client,
        topic.c_str(),
        payload.c_str(),
        0,
        1,
        1);

    planner_last_published_revision =
        scheduler_manager.revision();
    planner_publish_requested = false;
}

static volatile uint32_t mqtt_reconnect_count = 0;
static volatile uint32_t mqtt_consecutive_failures = 0;
static volatile bool mqtt_recovery_requested = false;
static int mqtt_last_error = 0;
static bool mqtt_has_connected_once = false;
static int64_t mqtt_last_hard_recovery_us = 0;

static constexpr uint32_t MQTT_HARD_RECOVERY_FAILURES = 3;
static constexpr int64_t MQTT_HARD_RECOVERY_COOLDOWN_US = 10000000LL;

/*
 * Na een MQTT-schakelcommando wacht MQTT kort op de echte bevestiging van
 * de Bestway-pomp. Zolang die bevestiging nog niet in SpaState staat, wordt
 * geen oude status terug naar Home Assistant gepubliceerd.
 */
enum class MqttFeedbackWait : uint8_t {
    None = 0,
    Power,
    Heater,
    Filter,
    Bubbles,
    Jets,
    Lock
};

static MqttFeedbackWait mqtt_feedback_wait = MqttFeedbackWait::None;
static bool mqtt_feedback_desired = false;
static int64_t mqtt_feedback_started_us = 0;
static constexpr int64_t MQTT_FEEDBACK_TIMEOUT_US = 2000000LL;

static bool mqtt_feedback_current_value(MqttFeedbackWait item)
{
    switch (item) {
        case MqttFeedbackWait::Power:   return spa.power;
        case MqttFeedbackWait::Heater:  return spa.heater;
        case MqttFeedbackWait::Filter:  return spa.filter;
        case MqttFeedbackWait::Bubbles: return spa.bubbles;
        case MqttFeedbackWait::Jets:    return spa.jets;
        case MqttFeedbackWait::Lock:    return spa.locked;
        default:                        return false;
    }
}

static const char *mqtt_feedback_name(MqttFeedbackWait item)
{
    switch (item) {
        case MqttFeedbackWait::Power:   return "power";
        case MqttFeedbackWait::Heater:  return "heater";
        case MqttFeedbackWait::Filter:  return "filter";
        case MqttFeedbackWait::Bubbles: return "bubbles";
        case MqttFeedbackWait::Jets:    return "jets";
        case MqttFeedbackWait::Lock:    return "lock";
        default:                        return "none";
    }
}

static void mqtt_wait_for_feedback(MqttFeedbackWait item, bool desired)
{
    mqtt_feedback_wait = item;
    mqtt_feedback_desired = desired;
    mqtt_feedback_started_us = esp_timer_get_time();
}

static void publish_home_assistant_discovery();

static void mqtt_event_handler(
    void *,
    esp_event_base_t,
    int32_t event_id,
    void *event_data)
{
    auto *event =
        static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            mqtt_publish_requested = true;
            planner_publish_requested = true;
            spa.mqtt_online = true;

            if (mqtt_has_connected_once) {
                ESP_LOGI(
                    TAG,
                    "MQTT opnieuw verbonden na %u mislukte poging(en)",
                    static_cast<unsigned>(mqtt_consecutive_failures));
            } else {
                mqtt_has_connected_once = true;
            }

            mqtt_consecutive_failures = 0;
            mqtt_recovery_requested = false;
            mqtt_last_error = 0;

            ESP_LOGI(
                TAG,
                "MQTT verbonden");

            if (!mqtt_availability_topic.empty()) {
                esp_mqtt_client_publish(
                    event->client,
                    mqtt_availability_topic.c_str(),
                    "online",
                    0,
                    1,
                    1);
            }

            {
                const MqttConfig &cfg =
                    mqtt_manager.config();

                const std::string godmode_topic =
                    mqtt_join_topic(
                        cfg.base_topic,
                        "command/godmode");

                const std::string fullpower_topic =
                    mqtt_join_topic(
                        cfg.base_topic,
                        "command/fullpower");

                const std::string power_topic =
                    mqtt_join_topic(
                        cfg.base_topic,
                        "command/power");

                const std::string heater_topic =
                    mqtt_join_topic(
                        cfg.base_topic,
                        "command/heater");

                const std::string filter_topic =
                    mqtt_join_topic(
                        cfg.base_topic,
                        "command/filter");

                const std::string bubbles_topic =
                    mqtt_join_topic(
                        cfg.base_topic,
                        "command/bubbles");

                const std::string jets_topic =
                    mqtt_join_topic(
                        cfg.base_topic,
                        "command/jets");

                const std::string lock_topic =
                    mqtt_join_topic(
                        cfg.base_topic,
                        "command/lock");

                const std::string target_topic =
                    mqtt_join_topic(
                        cfg.base_topic,
                        "command/target");

                const std::string planner_command_topic =
                    mqtt_join_topic(
                        cfg.base_topic,
                        "planner/command");

                esp_mqtt_client_subscribe(
                    event->client,
                    godmode_topic.c_str(),
                    1);

                esp_mqtt_client_subscribe(
                    event->client,
                    fullpower_topic.c_str(),
                    1);

                esp_mqtt_client_subscribe(
                    event->client,
                    power_topic.c_str(),
                    1);

                esp_mqtt_client_subscribe(
                    event->client,
                    heater_topic.c_str(),
                    1);

                esp_mqtt_client_subscribe(
                    event->client,
                    filter_topic.c_str(),
                    1);

                esp_mqtt_client_subscribe(
                    event->client,
                    bubbles_topic.c_str(),
                    1);

                if (bestway_bus.has_jets()) {
                    esp_mqtt_client_subscribe(
                        event->client,
                        jets_topic.c_str(),
                        1);
                }

                esp_mqtt_client_subscribe(
                    event->client,
                    lock_topic.c_str(),
                    1);

                esp_mqtt_client_subscribe(
                    event->client,
                    target_topic.c_str(),
                    1);

                esp_mqtt_client_subscribe(
                    event->client,
                    planner_command_topic.c_str(),
                    1);

                ESP_LOGI(
                    TAG,
                    "MQTT command topics geabonneerd");
            }

            publish_home_assistant_discovery();
            break;

        case MQTT_EVENT_DATA:
        {
            const std::string topic(
                event->topic,
                event->topic_len);

            const std::string payload(
                event->data,
                event->data_len);

            const MqttConfig &cfg =
                mqtt_manager.config();

            const std::string godmode_topic =
                mqtt_join_topic(
                    cfg.base_topic,
                    "command/godmode");

            const std::string fullpower_topic =
                mqtt_join_topic(
                    cfg.base_topic,
                    "command/fullpower");

            const std::string power_topic =
                mqtt_join_topic(
                    cfg.base_topic,
                    "command/power");

            const std::string heater_topic =
                mqtt_join_topic(
                    cfg.base_topic,
                    "command/heater");

            const std::string filter_topic =
                mqtt_join_topic(
                    cfg.base_topic,
                    "command/filter");

            const std::string bubbles_topic =
                mqtt_join_topic(
                    cfg.base_topic,
                    "command/bubbles");

            const std::string jets_topic =
                mqtt_join_topic(
                    cfg.base_topic,
                    "command/jets");

            const std::string lock_topic =
                mqtt_join_topic(
                    cfg.base_topic,
                    "command/lock");

            const std::string target_topic =
                mqtt_join_topic(
                    cfg.base_topic,
                    "command/target");

            const std::string planner_command_topic =
                mqtt_join_topic(
                    cfg.base_topic,
                    "planner/command");

            if (topic == planner_command_topic) {
                PendingPlannerCommand command;

                if (payload.size() <= 512 &&
                    parse_planner_command(payload, command)) {
                    pending_planner_command = command;
                    pending_planner_command_ready = true;
                } else {
                    ESP_LOGW(
                        TAG,
                        "Ongeldige MQTT planneropdracht");
                    publish_planner_result(
                        false,
                        "invalid_command");
                }

                break;
            }

            bool bool_value = false;

            if (topic == godmode_topic) {
                if (parse_mqtt_bool(
                        payload,
                        bool_value)) {

                    pending_mqtt_bool =
                        bool_value;

                    pending_mqtt_command =
                        PendingMqttCommand::Godmode;
                } else {
                    ESP_LOGW(
                        TAG,
                        "Ongeldige MQTT Godmode waarde: %s",
                        payload.c_str());
                }

            } else if (topic == fullpower_topic) {
                if (parse_mqtt_bool(
                        payload,
                        bool_value)) {

                    pending_mqtt_bool =
                        bool_value;

                    pending_mqtt_command =
                        PendingMqttCommand::FullPower;

                } else {
                    ESP_LOGW(
                        TAG,
                        "Ongeldige MQTT Full Power waarde: %s",
                        payload.c_str());
                }

            } else if (topic == power_topic) {
                if (lower_copy(payload) == "press") {
                    pending_mqtt_bool = false;
                    pending_mqtt_command =
                        PendingMqttCommand::Power;

                } else if (parse_mqtt_bool(
                               payload,
                               bool_value)) {

                    pending_mqtt_bool =
                        bool_value;

                    pending_mqtt_command =
                        PendingMqttCommand::Power;

                } else {
                    ESP_LOGW(
                        TAG,
                        "Ongeldige MQTT Power waarde: %s",
                        payload.c_str());
                }

            } else if (topic == heater_topic) {
                if (parse_mqtt_bool(
                        payload,
                        bool_value)) {

                    pending_mqtt_bool =
                        bool_value;

                    pending_mqtt_command =
                        PendingMqttCommand::Heater;
                }
            } else if (topic == filter_topic) {
                if (parse_mqtt_bool(
                        payload,
                        bool_value)) {

                    pending_mqtt_bool =
                        bool_value;

                    pending_mqtt_command =
                        PendingMqttCommand::Filter;
                }
            } else if (topic == bubbles_topic) {
                if (parse_mqtt_bool(
                        payload,
                        bool_value)) {

                    pending_mqtt_bool =
                        bool_value;

                    pending_mqtt_command =
                        PendingMqttCommand::Bubbles;
                }
            } else if (topic == jets_topic) {
                if (parse_mqtt_bool(
                        payload,
                        bool_value)) {

                    pending_mqtt_bool =
                        bool_value;

                    pending_mqtt_command =
                        PendingMqttCommand::Jets;
                }
            } else if (topic == lock_topic) {
                if (parse_mqtt_bool(
                        payload,
                        bool_value)) {

                    pending_mqtt_bool =
                        bool_value;

                    pending_mqtt_command =
                        PendingMqttCommand::Lock;
                } else {
                    ESP_LOGW(
                        TAG,
                        "Ongeldige MQTT vergrendeling waarde: %s",
                        payload.c_str());
                }
            } else if (topic == target_topic) {
                char *end = nullptr;

                const long target =
                    std::strtol(
                        payload.c_str(),
                        &end,
                        10);

                if (end &&
                    *end == '\0' &&
                    target >= 20 &&
                    target <= 40) {

                    pending_mqtt_target =
                        static_cast<int>(target);

                    pending_mqtt_command =
                        PendingMqttCommand::Target;
                } else {
                    ESP_LOGW(
                        TAG,
                        "Ongeldige MQTT doeltemperatuur: %s",
                        payload.c_str());
                }
            }

            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            spa.mqtt_online = false;

            ESP_LOGW(
                TAG,
                "MQTT verbinding verbroken");
            break;

        case MQTT_EVENT_ERROR:
            if (event->error_handle) {
                mqtt_last_error =
                    static_cast<int>(
                        event->error_handle->error_type);
            } else {
                mqtt_last_error = -1;
            }

            /*
             * Tel echte mislukte verbindingspogingen. De oude teller werd
             * pas verhoogd nadat een reconnect succesvol was en bleef dus
             * op 0 staan zolang de broker onbereikbaar bleef.
             */
            ++mqtt_reconnect_count;
            ++mqtt_consecutive_failures;

            ESP_LOGW(
                TAG,
                "MQTT fout ontvangen, type=%d, poging=%u, opeenvolgende fouten=%u",
                mqtt_last_error,
                static_cast<unsigned>(mqtt_reconnect_count),
                static_cast<unsigned>(mqtt_consecutive_failures));

            /*
             * ESP-MQTT probeert zelf opnieuw te verbinden. Als meerdere
             * transportpogingen achter elkaar mislukken, laten we de gewone
             * loop de client volledig opnieuw opbouwen. Dit gebeurt bewust
             * niet vanuit de MQTT event callback.
             */
            if (!mqtt_connected &&
                mqtt_consecutive_failures >= MQTT_HARD_RECOVERY_FAILURES) {

                mqtt_recovery_requested = true;

                ESP_LOGW(
                    TAG,
                    "MQTT hard herstel aangevraagd na %u opeenvolgende fouten",
                    static_cast<unsigned>(mqtt_consecutive_failures));
            }
            break;

        default:
            break;
    }
}

bool nvs_get_string(nvs_handle_t handle,
                    const char *key,
                    std::string &value)
{
    size_t length = 0;

    esp_err_t err =
        nvs_get_str(handle, key, nullptr, &length);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return true;
    }

    if (err != ESP_OK) {
        return false;
    }

    if (length == 0) {
        value.clear();
        return true;
    }

    std::string buffer(length, '\0');

    err = nvs_get_str(
        handle,
        key,
        buffer.data(),
        &length);

    if (err != ESP_OK) {
        return false;
    }

    if (!buffer.empty() &&
        buffer.back() == '\0') {
        buffer.pop_back();
    }

    value = buffer;
    return true;
}

bool nvs_set_string(nvs_handle_t handle,
                    const char *key,
                    const std::string &value)
{
    return nvs_set_str(
        handle,
        key,
        value.c_str()) == ESP_OK;
}

bool load_publication(nvs_handle_t handle,
                      const char *enabled_key,
                      const char *topic_key,
                      MqttPublicationConfig &item)
{
    uint8_t enabled = item.enabled ? 1 : 0;

    esp_err_t err =
        nvs_get_u8(handle, enabled_key, &enabled);

    if (err != ESP_OK &&
        err != ESP_ERR_NVS_NOT_FOUND) {
        return false;
    }

    item.enabled = enabled != 0;

    return nvs_get_string(
        handle,
        topic_key,
        item.topic);
}

bool save_publication(nvs_handle_t handle,
                      const char *enabled_key,
                      const char *topic_key,
                      const MqttPublicationConfig &item)
{
    if (nvs_set_u8(
            handle,
            enabled_key,
            item.enabled ? 1 : 0) != ESP_OK) {
        return false;
    }

    return nvs_set_string(
        handle,
        topic_key,
        item.topic);
}

std::string full_topic(const MqttConfig &cfg,
                       const std::string &topic)
{
    if (topic.empty()) {
        return {};
    }

    std::string full = cfg.base_topic;

    while (!full.empty() &&
           full.back() == '/') {
        full.pop_back();
    }

    size_t start = 0;

    while (start < topic.size() &&
           topic[start] == '/') {
        ++start;
    }

    if (!full.empty()) {
        full += "/";
    }

    full += topic.substr(start);

    return full;
}

void publish_value(const MqttConfig &cfg,
                   const MqttPublicationConfig &publication,
                   const std::string &value)
{
    if (!mqtt_client ||
        !mqtt_connected ||
        !publication.enabled ||
        publication.topic.empty()) {
        return;
    }

    const std::string topic =
        full_topic(cfg, publication.topic);

    if (topic.empty()) {
        return;
    }

    esp_mqtt_client_publish(
        mqtt_client,
        topic.c_str(),
        value.c_str(),
        0,
        0,
        1);
}

const char *on_off(bool value)
{
    return value ? "ON" : "OFF";
}

void get_network_state(std::string &ip,
                       int &rssi)
{
    ip = "--";
    rssi = 0;

    wifi_ap_record_t ap = {};

    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        rssi = ap.rssi;
    }

    esp_netif_t *netif =
        esp_netif_get_handle_from_ifkey(
            "WIFI_STA_DEF");

    esp_netif_ip_info_t info = {};

    if (netif &&
        esp_netif_get_ip_info(netif, &info) == ESP_OK &&
        info.ip.addr != 0) {

        char buffer[24] = {};

        std::snprintf(
            buffer,
            sizeof(buffer),
            IPSTR,
            IP2STR(&info.ip));

        ip = buffer;
    }
}

std::string discovery_json_escape(const std::string &value)
{
    std::string out;
    out.reserve(value.size() + 8);

    for (char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }

    return out;
}

std::string discovery_object_id(const std::string &name)
{
    std::string out;

    for (char c : name) {
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9')) {
            out += static_cast<char>(
                (c >= 'A' && c <= 'Z')
                    ? (c - 'A' + 'a')
                    : c);
        } else {
            out += '_';
        }
    }

    return out;
}

void publish_discovery_entity(
    const MqttConfig &cfg,
    const char *component,
    const char *object_name,
    const char *friendly_name,
    const MqttPublicationConfig &publication,
    const char *device_class = nullptr,
    const char *unit = nullptr,
    const char *state_class = nullptr,
    bool binary = false)
{
    if (!mqtt_client ||
        !mqtt_connected ||
        !cfg.home_assistant_discovery ||
        !publication.enabled ||
        publication.topic.empty()) {
        return;
    }

    const std::string object_id =
        discovery_object_id(object_name);

    const std::string state_topic =
        full_topic(cfg, publication.topic);

    std::string discovery_topic =
        "homeassistant/";
    discovery_topic += component;
    discovery_topic += "/bestway_spa/";
    discovery_topic += object_id;
    discovery_topic += "/config";

    std::string payload = "{";

    payload += "\"name\":\"";
    payload += discovery_json_escape(friendly_name);
    payload += "\"";

    payload += ",\"unique_id\":\"bestway_spa_";
    payload += object_id;
    payload += "\"";

    payload += ",\"state_topic\":\"";
    payload += discovery_json_escape(state_topic);
    payload += "\"";

    payload += ",\"availability_topic\":\"";
    payload += discovery_json_escape(
        mqtt_availability_topic);
    payload += "\"";

    payload += ",\"payload_available\":\"online\"";
    payload += ",\"payload_not_available\":\"offline\"";

    if (binary) {
        payload += ",\"payload_on\":\"ON\"";
        payload += ",\"payload_off\":\"OFF\"";
    }

    if (device_class && *device_class) {
        payload += ",\"device_class\":\"";
        payload += discovery_json_escape(device_class);
        payload += "\"";
    }

    if (unit && *unit) {
        payload += ",\"unit_of_measurement\":\"";
        payload += discovery_json_escape(unit);
        payload += "\"";
    }

    if (state_class && *state_class) {
        payload += ",\"state_class\":\"";
        payload += discovery_json_escape(state_class);
        payload += "\"";
    }

    payload += ",\"device\":{";
    payload += "\"identifiers\":[\"bestway_spa_controller\"]";
    payload += ",\"name\":\"Bestway Lay-Z-Spa\"";
    payload += ",\"manufacturer\":\"Bestway\"";
    payload += ",\"model\":\"ESP32-C6 Spa Control\"";
    payload += "}";

    payload += "}";

    esp_mqtt_client_publish(
        mqtt_client,
        discovery_topic.c_str(),
        payload.c_str(),
        0,
        1,
        1);
}

static void publish_home_assistant_number(
    const MqttConfig &cfg,
    const char *object_id,
    const char *name,
    const MqttPublicationConfig &pub,
    const char *command_suffix,
    int min_value,
    int max_value,
    int step_value,
    const char *unit = nullptr,
    const char *device_class = nullptr)
{
    if (!mqtt_client ||
        !mqtt_connected ||
        !cfg.home_assistant_discovery ||
        !pub.enabled) {
        return;
    }

    const std::string state_topic =
        mqtt_join_topic(
            cfg.base_topic,
            pub.topic);

    const std::string command_topic =
        mqtt_join_topic(
            cfg.base_topic,
            command_suffix);

    std::string discovery_topic =
        "homeassistant/number/bestway_spa/";

    discovery_topic += object_id;
    discovery_topic += "/config";

    std::string payload = "{";

    payload += "\"name\":\"";
    payload += discovery_json_escape(name);
    payload += "\"";

    payload += ",\"unique_id\":\"bestway_spa_";
    payload += discovery_json_escape(object_id);
    payload += "\"";

    payload += ",\"state_topic\":\"";
    payload += discovery_json_escape(state_topic);
    payload += "\"";

    payload += ",\"command_topic\":\"";
    payload += discovery_json_escape(command_topic);
    payload += "\"";

    payload += ",\"min\":";
    payload += std::to_string(min_value);

    payload += ",\"max\":";
    payload += std::to_string(max_value);

    payload += ",\"step\":";
    payload += std::to_string(step_value);

    payload += ",\"mode\":\"slider\"";

    payload += ",\"availability_topic\":\"";
    payload += discovery_json_escape(
        mqtt_availability_topic);
    payload += "\"";

    payload += ",\"payload_available\":\"online\"";
    payload += ",\"payload_not_available\":\"offline\"";

    if (unit && *unit) {
        payload += ",\"unit_of_measurement\":\"";
        payload += discovery_json_escape(unit);
        payload += "\"";
    }

    if (device_class && *device_class) {
        payload += ",\"device_class\":\"";
        payload += discovery_json_escape(device_class);
        payload += "\"";
    }

    payload += ",\"device\":{";
    payload += "\"identifiers\":[\"bestway_spa_controller\"]";
    payload += ",\"name\":\"Bestway Lay-Z-Spa\"";
    payload += ",\"manufacturer\":\"Bestway\"";
    payload += ",\"model\":\"ESP32-C6 Spa Control\"";
    payload += "}";

    payload += "}";

    esp_mqtt_client_publish(
        mqtt_client,
        discovery_topic.c_str(),
        payload.c_str(),
        0,
        1,
        1);
}


static void publish_home_assistant_button(
    const MqttConfig &cfg,
    const char *object_id,
    const char *name,
    const char *command_suffix,
    const char *icon)
{
    if (!mqtt_client ||
        !cfg.home_assistant_discovery) {
        return;
    }

    const std::string discovery_topic =
        "homeassistant/button/bestway_spa/" +
        std::string(object_id) +
        "/config";

    const std::string command_topic =
        mqtt_join_topic(
            cfg.base_topic,
            command_suffix);

    std::string payload = "{";

    payload += "\"name\":\"";
    payload += name;
    payload += "\",";

    payload += "\"unique_id\":\"bestway_spa_";
    payload += object_id;
    payload += "\",";

    payload += "\"command_topic\":\"";
    payload += command_topic;
    payload += "\",";

    payload += "\"payload_press\":\"PRESS\",";

    payload += "\"availability_topic\":\"";
    payload += mqtt_availability_topic;
    payload += "\",";

    payload += "\"icon\":\"";
    payload += icon;
    payload += "\",";

    payload += "\"device\":{";
    payload += "\"identifiers\":[\"bestway_spa\"],";
    payload += "\"name\":\"Bestway Spa\",";
    payload += "\"manufacturer\":\"Bestway\",";
    payload += "\"model\":\"ESP32-C6 Spa Control\"";
    payload += "}";

    payload += "}";

    esp_mqtt_client_publish(
        mqtt_client,
        discovery_topic.c_str(),
        payload.c_str(),
        0,
        1,
        1);
}


static void publish_home_assistant_switch(
    const MqttConfig &cfg,
    const char *object_id,
    const char *name,
    const MqttPublicationConfig &pub,
    const char *command_suffix,
    const char *icon = nullptr)
{
    if (!mqtt_client ||
        !mqtt_connected ||
        !cfg.home_assistant_discovery ||
        !pub.enabled) {
        return;
    }

    const std::string state_topic =
        mqtt_join_topic(
            cfg.base_topic,
            pub.topic);

    const std::string command_topic =
        mqtt_join_topic(
            cfg.base_topic,
            command_suffix);

    std::string discovery_topic =
        "homeassistant/switch/bestway_spa/";

    discovery_topic += object_id;
    discovery_topic += "/config";

    std::string payload = "{";

    payload += "\"name\":\"";
    payload += discovery_json_escape(name);
    payload += "\"";

    payload += ",\"unique_id\":\"bestway_spa_";
    payload += discovery_json_escape(object_id);
    payload += "\"";

    payload += ",\"state_topic\":\"";
    payload += discovery_json_escape(state_topic);
    payload += "\"";

    payload += ",\"command_topic\":\"";
    payload += discovery_json_escape(command_topic);
    payload += "\"";

    payload += ",\"payload_on\":\"ON\"";
    payload += ",\"payload_off\":\"OFF\"";

    payload += ",\"availability_topic\":\"";
    payload += discovery_json_escape(
        mqtt_availability_topic);
    payload += "\"";

    payload += ",\"payload_available\":\"online\"";
    payload += ",\"payload_not_available\":\"offline\"";

    if (icon && *icon) {
        payload += ",\"icon\":\"";
        payload += discovery_json_escape(icon);
        payload += "\"";
    }

    payload += ",\"device\":{";
    payload += "\"identifiers\":[\"bestway_spa_controller\"]";
    payload += ",\"name\":\"Bestway Lay-Z-Spa\"";
    payload += ",\"manufacturer\":\"Bestway\"";
    payload += ",\"model\":\"ESP32-C6 Spa Control\"";
    payload += "}";

    payload += "}";

    esp_mqtt_client_publish(
        mqtt_client,
        discovery_topic.c_str(),
        payload.c_str(),
        0,
        1,
        1);
}


static void publish_home_assistant_godmode_switch(
    const MqttConfig &cfg)
{
    if (!mqtt_client ||
        !mqtt_connected ||
        !cfg.home_assistant_discovery) {
        return;
    }

    const std::string discovery_topic =
        "homeassistant/switch/bestway_spa/godmode/config";

    /*
     * Godmode bestaat alleen voor 4-wire.
     *
     * Wanneer het model geen 4-wire is, of de Godmode-statepublicatie
     * uitgeschakeld is, verwijderen we een eventueel oude retained
     * discovery-config zodat Home Assistant geen onbruikbare switch houdt.
     */
    if (bestway_bus.model_info().family !=
            BestwayProtocolFamily::Uart4Wire ||
        !cfg.godmode.enabled ||
        cfg.godmode.topic.empty()) {

        esp_mqtt_client_publish(
            mqtt_client,
            discovery_topic.c_str(),
            "",
            0,
            1,
            1);

        return;
    }

    const std::string state_topic =
        full_topic(
            cfg,
            cfg.godmode.topic);

    const std::string command_topic =
        mqtt_join_topic(
            cfg.base_topic,
            "command/godmode");

    std::string payload = "{";

    payload += "\"name\":\"Godmode\"";
    payload += ",\"unique_id\":\"bestway_spa_godmode\"";

    payload += ",\"state_topic\":\"";
    payload += discovery_json_escape(state_topic);
    payload += "\"";

    payload += ",\"command_topic\":\"";
    payload += discovery_json_escape(command_topic);
    payload += "\"";

    payload += ",\"payload_on\":\"ON\"";
    payload += ",\"payload_off\":\"OFF\"";

    payload += ",\"availability_topic\":\"";
    payload += discovery_json_escape(
        mqtt_availability_topic);
    payload += "\"";

    payload += ",\"payload_available\":\"online\"";
    payload += ",\"payload_not_available\":\"offline\"";

    payload += ",\"icon\":\"mdi:shield-crown\"";

    payload += ",\"device\":{";
    payload += "\"identifiers\":[\"bestway_spa_controller\"]";
    payload += ",\"name\":\"Bestway Lay-Z-Spa\"";
    payload += ",\"manufacturer\":\"Bestway\"";
    payload += ",\"model\":\"ESP32-C6 Spa Control\"";
    payload += "}";

    payload += "}";

    esp_mqtt_client_publish(
        mqtt_client,
        discovery_topic.c_str(),
        payload.c_str(),
        0,
        1,
        1);
}


static void publish_home_assistant_fullpower_switch(
    const MqttConfig &cfg)
{
    if (!mqtt_client ||
        !mqtt_connected ||
        !cfg.home_assistant_discovery) {
        return;
    }

    const std::string discovery_topic =
        "homeassistant/switch/bestway_spa/fullpower/config";

    /*
     * Full Power bestaat alleen voor 4-wire.
     *
     * Wanneer het model geen 4-wire is, of de Full Power
     * state-publicatie is uitgeschakeld, verwijderen we
     * eventueel oude retained Home Assistant discovery.
     */
    if (bestway_bus.model_info().family !=
            BestwayProtocolFamily::Uart4Wire ||
        !cfg.full_power.enabled ||
        cfg.full_power.topic.empty()) {

        esp_mqtt_client_publish(
            mqtt_client,
            discovery_topic.c_str(),
            "",
            0,
            1,
            1);

        return;
    }

    const std::string state_topic =
        full_topic(
            cfg,
            cfg.full_power.topic);

    const std::string command_topic =
        mqtt_join_topic(
            cfg.base_topic,
            "command/fullpower");

    std::string payload = "{";

    payload += "\"name\":\"Full Power\"";
    payload += ",\"unique_id\":\"bestway_spa_fullpower\"";

    payload += ",\"state_topic\":\"";
    payload += discovery_json_escape(state_topic);
    payload += "\"";

    payload += ",\"command_topic\":\"";
    payload += discovery_json_escape(command_topic);
    payload += "\"";

    payload += ",\"payload_on\":\"ON\"";
    payload += ",\"payload_off\":\"OFF\"";

    payload += ",\"availability_topic\":\"";
    payload += discovery_json_escape(
        mqtt_availability_topic);
    payload += "\"";

    payload += ",\"payload_available\":\"online\"";
    payload += ",\"payload_not_available\":\"offline\"";

    payload += ",\"icon\":\"mdi:lightning-bolt\"";

    payload += ",\"device\":{";
    payload += "\"identifiers\":[\"bestway_spa_controller\"]";
    payload += ",\"name\":\"Bestway Lay-Z-Spa\"";
    payload += ",\"manufacturer\":\"Bestway\"";
    payload += ",\"model\":\"ESP32-C6 Spa Control\"";
    payload += "}";

    payload += "}";

    esp_mqtt_client_publish(
        mqtt_client,
        discovery_topic.c_str(),
        payload.c_str(),
        0,
        1,
        1);
}


static void publish_home_assistant_planner_sensor(
    const MqttConfig &cfg)
{
    if (!mqtt_client ||
        !mqtt_connected ||
        !cfg.home_assistant_discovery) {
        return;
    }

    const std::string state_topic =
        mqtt_join_topic(cfg.base_topic, "planner/state");

    const std::string discovery_topic =
        "homeassistant/sensor/bestway_spa/planner/config";

    std::string payload = "{";
    payload += "\"name\":\"Planner\"";
    payload += ",\"unique_id\":\"bestway_spa_planner\"";
    payload += ",\"state_topic\":\"";
    payload += discovery_json_escape(state_topic);
    payload += "\"";
    payload += ",\"value_template\":\"{{ value_json.count }}\"";
    payload += ",\"json_attributes_topic\":\"";
    payload += discovery_json_escape(state_topic);
    payload += "\"";
    payload += ",\"availability_topic\":\"";
    payload += discovery_json_escape(mqtt_availability_topic);
    payload += "\"";
    payload += ",\"payload_available\":\"online\"";
    payload += ",\"payload_not_available\":\"offline\"";
    payload += ",\"icon\":\"mdi:calendar-clock\"";
    payload += ",\"device\":{";
    payload += "\"identifiers\":[\"bestway_spa_controller\"]";
    payload += ",\"name\":\"Bestway Lay-Z-Spa\"";
    payload += ",\"manufacturer\":\"Bestway\"";
    payload += ",\"model\":\"ESP32-C6 Spa Control\"";
    payload += "}}";

    esp_mqtt_client_publish(
        mqtt_client,
        discovery_topic.c_str(),
        payload.c_str(),
        0,
        1,
        1);
}

static void publish_home_assistant_discovery()
{
    const MqttConfig &cfg =
        mqtt_manager.config();

    if (!cfg.home_assistant_discovery ||
        !mqtt_client ||
        !mqtt_connected) {
        return;
    }

    ESP_LOGI(
        TAG,
        "Home Assistant MQTT Discovery publiceren");

    /*
     * Oude retained binary_sensor discovery-configs verwijderen.
     * Deze functies zijn nu echte bestuurbare HA switches.
     */
    esp_mqtt_client_publish(
        mqtt_client,
        "homeassistant/sensor/bestway_spa/target_temperature/config",
        "",
        0,
        1,
        1);

    static const char *legacy_switch_topics[] = {
        "homeassistant/binary_sensor/bestway_spa/heater/config",
        "homeassistant/binary_sensor/bestway_spa/filter/config",
        "homeassistant/binary_sensor/bestway_spa/bubbles/config",
        "homeassistant/binary_sensor/bestway_spa/jets/config",
        "homeassistant/binary_sensor/bestway_spa/locked/config"
    };

    for (const char *topic : legacy_switch_topics) {
        esp_mqtt_client_publish(
            mqtt_client,
            topic,
            "",
            0,
            1,
            1);
    }

    publish_home_assistant_godmode_switch(cfg);
    publish_home_assistant_fullpower_switch(cfg);
    publish_home_assistant_planner_sensor(cfg);

    publish_discovery_entity(
        cfg,
        "sensor",
        "temperature",
        "Watertemperatuur",
        cfg.temperature,
        "temperature",
        "°C",
        "measurement");

    publish_home_assistant_number(
        cfg,
        "target_temperature",
        "Doeltemperatuur",
        cfg.target,
        "command/target",
        20,
        40,
        1,
        "°C",
        "temperature");

    if (bestway_bus.model_info().family ==
        BestwayProtocolFamily::SixWireType1) {

        esp_mqtt_client_publish(
            mqtt_client,
            "homeassistant/binary_sensor/bestway_spa/power/config",
            "",
            0,
            1,
            1);

        esp_mqtt_client_publish(
            mqtt_client,
            "homeassistant/button/bestway_spa/power/config",
            "",
            0,
            1,
            1);

        publish_home_assistant_switch(
            cfg,
            "power",
            "Spa aan",
            cfg.power,
            "command/power",
            "mdi:power");

    } else if (bestway_bus.model_info().family ==
               BestwayProtocolFamily::SixWireType2) {

        esp_mqtt_client_publish(
            mqtt_client,
            "homeassistant/binary_sensor/bestway_spa/power/config",
            "",
            0,
            1,
            1);

        esp_mqtt_client_publish(
            mqtt_client,
            "homeassistant/switch/bestway_spa/power/config",
            "",
            0,
            1,
            1);

        publish_home_assistant_button(
            cfg,
            "power",
            "Spa Power",
            "command/power",
            "mdi:power");

    } else {
        esp_mqtt_client_publish(
            mqtt_client,
            "homeassistant/switch/bestway_spa/power/config",
            "",
            0,
            1,
            1);

        esp_mqtt_client_publish(
            mqtt_client,
            "homeassistant/button/bestway_spa/power/config",
            "",
            0,
            1,
            1);

        publish_discovery_entity(
            cfg,
            "binary_sensor",
            "power",
            "Spa aan",
            cfg.power,
            "power",
            nullptr,
            nullptr,
            true);
    }

    publish_home_assistant_switch(
        cfg,
        "heater",
        "Heater",
        cfg.heater,
        "command/heater",
        "mdi:radiator");

    publish_discovery_entity(
        cfg,
        "binary_sensor",
        "heating_active",
        "Daadwerkelijk verwarmen",
        cfg.heating_active,
        "heat",
        nullptr,
        nullptr,
        true);

    publish_home_assistant_switch(
        cfg,
        "filter",
        "Filter",
        cfg.filter,
        "command/filter",
        "mdi:air-filter");

    publish_home_assistant_switch(
        cfg,
        "bubbles",
        "Bubbels",
        cfg.bubbles,
        "command/bubbles",
        "mdi:weather-windy");

    if (bestway_bus.has_jets()) {
        publish_home_assistant_switch(
            cfg,
            "jets",
            "Jets",
            cfg.jets,
            "command/jets",
            "mdi:waves");
    } else {
        /*
         * Als eerder een HydroJet-model actief was kan Home Assistant
         * nog een retained Jets discovery-config hebben.
         * Lege retained payload verwijdert die entity.
         */
        esp_mqtt_client_publish(
            mqtt_client,
            "homeassistant/switch/bestway_spa/jets/config",
            "",
            0,
            1,
            1);
    }

    if (bestway_bus.model_info().family ==
            BestwayProtocolFamily::SixWireType1 ||
        bestway_bus.model_info().family ==
            BestwayProtocolFamily::SixWireType2) {

        publish_home_assistant_switch(
            cfg,
            "locked",
            "Vergrendeling",
            cfg.locked,
            "command/lock",
            "mdi:lock");

    } else {
        esp_mqtt_client_publish(
            mqtt_client,
            "homeassistant/switch/bestway_spa/locked/config",
            "",
            0,
            1,
            1);

        publish_discovery_entity(
            cfg,
            "binary_sensor",
            "locked",
            "Vergrendeling",
            cfg.locked,
            "lock",
            nullptr,
            nullptr,
            true);
    }

    publish_discovery_entity(
        cfg,
        "binary_sensor",
        "connected",
        "Spa verbonden",
        cfg.connected,
        "connectivity",
        nullptr,
        nullptr,
        true);

    publish_discovery_entity(
        cfg,
        "binary_sensor",
        "ready",
        "Spa gebruiksklaar",
        cfg.ready,
        nullptr,
        nullptr,
        nullptr,
        true);

    publish_discovery_entity(
        cfg,
        "sensor",
        "wifi_rssi",
        "WiFi signaal",
        cfg.rssi,
        "signal_strength",
        "dBm",
        "measurement");

    publish_discovery_entity(
        cfg,
        "sensor",
        "free_heap",
        "Vrij geheugen",
        cfg.heap,
        nullptr,
        "B",
        "measurement");

    publish_discovery_entity(
        cfg,
        "sensor",
        "uptime",
        "Uptime",
        cfg.uptime);

    publish_discovery_entity(
        cfg,
        "sensor",
        "firmware",
        "Firmware",
        cfg.firmware);

    publish_discovery_entity(
        cfg,
        "sensor",
        "ip_address",
        "IP-adres",
        cfg.ip);

    /*
     * Onderhoudsstatus:
     * OK / WARNING / ALARM
     *
     * Gebruikt de bestaande configureerbare Maintenance-publicatie.
     */
    publish_discovery_entity(
        cfg,
        "sensor",
        "maintenance_status",
        "Onderhoudsstatus",
        cfg.maintenance);

    publish_discovery_entity(
        cfg,
        "sensor",
        "maintenance_filter_replace",
        "Onderhoud filter vervangen",
        cfg.maintenance_filter_replace,
        nullptr,
        "d");

    publish_discovery_entity(
        cfg,
        "sensor",
        "maintenance_filter_clean",
        "Onderhoud filter reinigen",
        cfg.maintenance_filter_clean,
        nullptr,
        "d");

    publish_discovery_entity(
        cfg,
        "sensor",
        "maintenance_chlorine",
        "Onderhoud chloor",
        cfg.maintenance_chlorine,
        nullptr,
        "d");
}



/*
 * Lees de onderhoudsstatus uit dezelfde NVS-data als de
 * Onderhoud-pagina. Zo blijft MQTT onafhankelijk van web_server.
 *
 * Resultaat:
 *   OK
 *   WARNING
 *   ALARM
 */
static std::string mqtt_maintenance_remaining_days(
    int item_index)
{
    struct ItemConfig {
        const char *enabled_key;
        const char *days_key;
        const char *last_key;
        uint16_t default_days;
    };

    static const ItemConfig configs[3] = {
        {"fr_en", "fr_days", "fr_last", 30},
        {"fc_en", "fc_days", "fc_last", 7},
        {"cl_en", "cl_days", "cl_last", 3},
    };

    if (item_index < 0 || item_index >= 3) {
        return "unknown";
    }

    bool enabled = true;
    uint16_t interval_days =
        configs[item_index].default_days;

    int64_t last_done = 0;

    nvs_handle_t nvs = 0;

    if (nvs_open(
            "spa_maint",
            NVS_READONLY,
            &nvs) == ESP_OK) {

        uint8_t enabled_value = 1;
        uint16_t days_value = 0;
        int64_t last_value = 0;

        if (nvs_get_u8(
                nvs,
                configs[item_index].enabled_key,
                &enabled_value) == ESP_OK) {

            enabled = enabled_value != 0;
        }

        if (nvs_get_u16(
                nvs,
                configs[item_index].days_key,
                &days_value) == ESP_OK &&
            days_value >= 1 &&
            days_value <= 3650) {

            interval_days = days_value;
        }

        if (nvs_get_i64(
                nvs,
                configs[item_index].last_key,
                &last_value) == ESP_OK) {

            last_done = last_value;
        }

        nvs_close(nvs);
    }

    /*
     * Uitgeschakeld, nog nooit uitgevoerd of klok nog niet geldig:
     * Home Assistant toont de sensor als unknown.
     */
    const time_t now = time(nullptr);

    if (!enabled ||
        last_done < 1700000000 ||
        now < 1700000000) {

        return "unknown";
    }

    time_t last =
        static_cast<time_t>(last_done);

    struct tm last_tm = {};
    struct tm now_tm = {};

    localtime_r(&last, &last_tm);
    localtime_r(&now, &now_tm);

    /*
     * Middag gebruiken voorkomt afwijkingen door zomer-/wintertijd.
     */
    last_tm.tm_hour = 12;
    last_tm.tm_min = 0;
    last_tm.tm_sec = 0;

    now_tm.tm_hour = 12;
    now_tm.tm_min = 0;
    now_tm.tm_sec = 0;

    const time_t last_day =
        mktime(&last_tm);

    const time_t today =
        mktime(&now_tm);

    const long elapsed_days =
        static_cast<long>(
            difftime(today, last_day) / 86400.0);

    const long remaining =
        static_cast<long>(interval_days) -
        elapsed_days;

    return std::to_string(remaining);
}


static std::string mqtt_maintenance_status()
{
    struct Item {
        bool enabled;
        uint16_t interval_days;
        int64_t last_done;
    };

    Item items[3] = {
        {true, 30, 0}, // filter vervangen
        {true, 7,  0}, // filter reinigen
        {true, 3,  0}, // chloor
    };

    nvs_handle_t nvs = 0;

    if (nvs_open("spa_maint", NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t enabled = 0;
        uint16_t days = 0;
        int64_t last = 0;

        if (nvs_get_u8(nvs, "fr_en", &enabled) == ESP_OK)
            items[0].enabled = enabled != 0;

        if (nvs_get_u16(nvs, "fr_days", &days) == ESP_OK &&
            days >= 1 && days <= 3650)
            items[0].interval_days = days;

        if (nvs_get_i64(nvs, "fr_last", &last) == ESP_OK)
            items[0].last_done = last;

        if (nvs_get_u8(nvs, "fc_en", &enabled) == ESP_OK)
            items[1].enabled = enabled != 0;

        if (nvs_get_u16(nvs, "fc_days", &days) == ESP_OK &&
            days >= 1 && days <= 3650)
            items[1].interval_days = days;

        if (nvs_get_i64(nvs, "fc_last", &last) == ESP_OK)
            items[1].last_done = last;

        if (nvs_get_u8(nvs, "cl_en", &enabled) == ESP_OK)
            items[2].enabled = enabled != 0;

        if (nvs_get_u16(nvs, "cl_days", &days) == ESP_OK &&
            days >= 1 && days <= 3650)
            items[2].interval_days = days;

        if (nvs_get_i64(nvs, "cl_last", &last) == ESP_OK)
            items[2].last_done = last;

        nvs_close(nvs);
    }

    const time_t now = time(nullptr);

    /*
     * Zolang de klok niet geldig is, geen vals onderhoudsalarm.
     */
    if (now < 1700000000) {
        return "OK";
    }

    bool warning = false;

    for (const Item &item : items) {
        if (!item.enabled ||
            item.last_done < 1700000000) {
            continue;
        }

        time_t last =
            static_cast<time_t>(item.last_done);

        struct tm last_tm = {};
        struct tm now_tm = {};

        localtime_r(&last, &last_tm);
        localtime_r(&now, &now_tm);

        /*
         * Middag voorkomt problemen rond zomertijdwissels.
         */
        last_tm.tm_hour = 12;
        last_tm.tm_min = 0;
        last_tm.tm_sec = 0;

        now_tm.tm_hour = 12;
        now_tm.tm_min = 0;
        now_tm.tm_sec = 0;

        const time_t last_day =
            mktime(&last_tm);

        const time_t today =
            mktime(&now_tm);

        const long elapsed_days =
            static_cast<long>(
                difftime(today, last_day) / 86400.0);

        const long remaining =
            static_cast<long>(item.interval_days) -
            elapsed_days;

        if (remaining <= 0) {
            return "ALARM";
        }

        if (remaining <= 2) {
            warning = true;
        }
    }

    return warning ? "WARNING" : "OK";
}


std::string mqtt_state_json()
{
    std::string ip;
    int rssi = 0;

    get_network_state(ip, rssi);

    const bool ready =
        spa.connected &&
        spa.data_valid;

    const esp_app_desc_t *app =
        esp_app_get_description();

    std::string json = "{";

    json += "\"temperature\":";
    json += std::to_string(spa.temperature_c);

    json += ",\"target\":";
    json += std::to_string(spa.target_temperature_c);

    json += ",\"power\":";
    json += spa.power ? "true" : "false";

    json += ",\"heater\":";
    json += spa.heater ? "true" : "false";

    json += ",\"heaterActive\":";
    json += spa.heater_active ? "true" : "false";

    json += ",\"filter\":";
    json += spa.filter ? "true" : "false";

    json += ",\"bubbles\":";
    json += spa.bubbles ? "true" : "false";

    json += ",\"jets\":";
    json += spa.jets ? "true" : "false";

    json += ",\"locked\":";
    json += spa.locked ? "true" : "false";

    json += ",\"connected\":";
    json += spa.connected ? "true" : "false";

    json += ",\"ready\":";
    json += ready ? "true" : "false";

    json += ",\"dataValid\":";
    json += spa.data_valid ? "true" : "false";

    json += ",\"errorCode\":";
    json += std::to_string(spa.error_code);

    json += ",\"rssi\":";
    json += std::to_string(rssi);

    json += ",\"freeHeap\":";
    json += std::to_string(
        static_cast<unsigned>(
            esp_get_free_heap_size()));

    json += ",\"uptime\":\"";
    json += spa.uptime();
    json += "\"";

    json += ",\"ip\":\"";
    json += ip;
    json += "\"";

    json += ",\"firmware\":\"";

    if (app) {
        json += app->version;
    }

    json += "\"";

    json += ",\"busMode\":\"";
    json += bestway_bus.mode_name();
    json += "\"";

    json += ",\"godMode\":";
    json += bestway_bus.godmode()
        ? "true"
        : "false";

    json += ",\"protocolFamily\":\"";

    if (bestway_bus.model_info().family ==
        BestwayProtocolFamily::Uart4Wire) {
        json += "4wire";
    } else if (
        bestway_bus.model_info().family ==
        BestwayProtocolFamily::SixWireType2) {
        json += "type2";
    } else {
        json += "type1";
    }

    json += "\"}";
    return json;
}



} // namespace


MqttManager mqtt_manager;


bool MqttManager::begin()
{
    if (!load()) {
        ESP_LOGW(
            TAG,
            "MQTT configuratie niet gevonden of ongeldig; standaardwaarden worden gebruikt");

        config_ = MqttConfig{};

        if (!save()) {
            ESP_LOGE(
                TAG,
                "Opslaan standaard MQTT configuratie mislukt");

            return false;
        }
    }

    ESP_LOGI(
        TAG,
        "MQTT configuratie geladen: enabled=%s host=%s port=%u baseTopic=%s",
        config_.enabled ? "true" : "false",
        config_.host.c_str(),
        static_cast<unsigned>(config_.port),
        config_.base_topic.c_str());

    ESP_LOGI(
        TAG,
        "MQTT configuratie gereed; wachten op IPv4");

    return true;
}


bool MqttManager::start()
{
    stop();

    if (!config_.enabled) {
        ESP_LOGI(
            TAG,
            "MQTT uitgeschakeld");

        return true;
    }

    if (config_.host.empty()) {
        ESP_LOGW(
            TAG,
            "MQTT ingeschakeld maar broker ontbreekt");

        return true;
    }

    mqtt_broker_uri =
        "mqtt://" +
        config_.host +
        ":" +
        std::to_string(config_.port);

    mqtt_availability_topic =
        config_.base_topic;

    if (!mqtt_availability_topic.empty() &&
        mqtt_availability_topic.back() != '/') {
        mqtt_availability_topic += "/";
    }

    mqtt_availability_topic += "availability";

    esp_mqtt_client_config_t mqtt_cfg = {};

    mqtt_cfg.broker.address.uri =
        mqtt_broker_uri.c_str();

    mqtt_cfg.credentials.client_id =
        config_.client_id.c_str();

    /*
     * Na een warme reset kan IPv4 al beschikbaar zijn terwijl het netwerk
     * richting de broker nog net niet stabiel genoeg is. Laat ESP-MQTT
     * daarom sneller opnieuw proberen; hard recovery blijft het vangnet.
     */
    mqtt_cfg.network.reconnect_timeout_ms = 2000;

    if (!config_.username.empty()) {
        mqtt_cfg.credentials.username =
            config_.username.c_str();
    }

    if (!config_.password.empty()) {
        mqtt_cfg.credentials.authentication.password =
            config_.password.c_str();
    }

    mqtt_cfg.session.last_will.topic =
        mqtt_availability_topic.c_str();

    mqtt_cfg.session.last_will.msg =
        "offline";

    mqtt_cfg.session.last_will.msg_len = 0;
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = 1;

    mqtt_client =
        esp_mqtt_client_init(&mqtt_cfg);

    if (!mqtt_client) {
        ESP_LOGE(
            TAG,
            "esp_mqtt_client_init mislukt");

        return false;
    }

    esp_err_t err =
        esp_mqtt_client_register_event(
            mqtt_client,
            MQTT_EVENT_ANY,
            mqtt_event_handler,
            nullptr);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "MQTT event handler registreren mislukt: %s",
            esp_err_to_name(err));

        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = nullptr;

        return false;
    }

    err =
        esp_mqtt_client_start(mqtt_client);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "MQTT client starten mislukt: %s",
            esp_err_to_name(err));

        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = nullptr;

        return false;
    }

    mqtt_consecutive_failures = 0;
    mqtt_recovery_requested = false;

    ESP_LOGI(
        TAG,
        "MQTT client gestart: %s",
        mqtt_broker_uri.c_str());

    return true;
}


void MqttManager::stop()
{
    const bool was_connected = mqtt_connected;

    mqtt_connected = false;
    mqtt_publish_requested = false;
    mqtt_last_publish_us = 0;
    mqtt_feedback_wait = MqttFeedbackWait::None;
    mqtt_feedback_started_us = 0;
    mqtt_target_feedback_wait = false;
    mqtt_target_feedback_desired = 0;
    mqtt_target_feedback_started_us = 0;
    spa.mqtt_online = false;

    if (!mqtt_client) {
        return;
    }

    /*
     * Alleen een nette offline-status publiceren als de client werkelijk
     * verbonden was. Tijdens hard herstel na een socket-timeout willen we
     * geen publish meer op een reeds kapotte transportverbinding starten.
     */
    if (was_connected &&
        !mqtt_availability_topic.empty()) {

        esp_mqtt_client_publish(
            mqtt_client,
            mqtt_availability_topic.c_str(),
            "offline",
            0,
            1,
            1);
    }

    esp_mqtt_client_stop(mqtt_client);
    esp_mqtt_client_destroy(mqtt_client);

    mqtt_client = nullptr;
}


void MqttManager::loop()
{
    /*
     * Hard MQTT-herstel buiten de MQTT callback uitvoeren.
     * Hiermee herstellen we ook van een transport/socket die na een broker-
     * herstart niet meer schoon terugkomt. WiFi, Matter en de spa-bus blijven
     * hierbij volledig ongemoeid.
     */
    if (mqtt_recovery_requested && config_.enabled) {
        const int64_t now_us = esp_timer_get_time();

        if (mqtt_last_hard_recovery_us == 0 ||
            (now_us - mqtt_last_hard_recovery_us) >=
                MQTT_HARD_RECOVERY_COOLDOWN_US) {

            mqtt_recovery_requested = false;
            mqtt_consecutive_failures = 0;
            mqtt_last_hard_recovery_us = now_us;

            ESP_LOGW(
                TAG,
                "MQTT hard herstel: client stoppen, vernietigen en opnieuw starten");

            if (!start()) {
                ESP_LOGE(
                    TAG,
                    "MQTT hard herstel mislukt; nieuwe poging volgt");

                mqtt_recovery_requested = true;
            }

            return;
        }
    }

    const PendingMqttCommand command =
        pending_mqtt_command;

    if (command != PendingMqttCommand::None) {
        pending_mqtt_command =
            PendingMqttCommand::None;

        switch (command) {
            case PendingMqttCommand::Godmode:
                if (bestway_bus.model_info().family !=
                    BestwayProtocolFamily::Uart4Wire) {

                    ESP_LOGW(
                        TAG,
                        "MQTT Godmode genegeerd: geen 4-wire model");
                    break;
                }

                ESP_LOGI(
                    TAG,
                    "MQTT opdracht: Godmode %s",
                    pending_mqtt_bool
                        ? "ON"
                        : "OFF");

                bestway_bus.set_godmode(
                    pending_mqtt_bool);
                break;

            case PendingMqttCommand::FullPower:
                if (bestway_bus.model_info().family !=
                    BestwayProtocolFamily::Uart4Wire) {

                    ESP_LOGW(
                        TAG,
                        "MQTT Full Power genegeerd: geen 4-wire model");
                    break;
                }

                if (!bestway_bus.godmode()) {
                    ESP_LOGW(
                        TAG,
                        "MQTT Full Power geweigerd: Godmode vereist");
                    break;
                }

                ESP_LOGI(
                    TAG,
                    "MQTT opdracht: Full Power %s",
                    pending_mqtt_bool
                        ? "ON"
                        : "OFF");

                bestway_bus.set_godmode_full_power(
                    pending_mqtt_bool);
                break;

            case PendingMqttCommand::Power:
                if (bestway_bus.model_info().family !=
                        BestwayProtocolFamily::SixWireType1 &&
                    bestway_bus.model_info().family !=
                        BestwayProtocolFamily::SixWireType2) {

                    ESP_LOGW(
                        TAG,
                        "MQTT Power genegeerd: geen 6-wire model");
                    break;
                }

                ESP_LOGI(
                    TAG,
                    "MQTT opdracht: power %s",
                    pending_mqtt_bool
                        ? "ON"
                        : "OFF/toggle");

                mqtt_wait_for_feedback(
                    MqttFeedbackWait::Power,
                    pending_mqtt_bool);

                bestway_bus.set_power(
                    pending_mqtt_bool);
                break;

            case PendingMqttCommand::Heater:
                if (bestway_bus.model_info().family ==
                        BestwayProtocolFamily::Uart4Wire &&
                    !bestway_bus.godmode()) {

                    ESP_LOGW(
                        TAG,
                        "MQTT heater geweigerd: 4-wire vereist Godmode");
                    break;
                }

                ESP_LOGI(
                    TAG,
                    "MQTT opdracht: heater %s",
                    pending_mqtt_bool
                        ? "ON"
                        : "OFF");

                mqtt_wait_for_feedback(
                    MqttFeedbackWait::Heater,
                    pending_mqtt_bool);

                bestway_bus.set_heater(
                    pending_mqtt_bool);
                break;

            case PendingMqttCommand::Filter:
                ESP_LOGI(
                    TAG,
                    "MQTT opdracht: filter %s",
                    pending_mqtt_bool
                        ? "ON"
                        : "OFF");

                mqtt_wait_for_feedback(
                    MqttFeedbackWait::Filter,
                    pending_mqtt_bool);

                bestway_bus.set_filter(
                    pending_mqtt_bool);
                break;

            case PendingMqttCommand::Bubbles:
                ESP_LOGI(
                    TAG,
                    "MQTT opdracht: bubbles %s",
                    pending_mqtt_bool
                        ? "ON"
                        : "OFF");

                mqtt_wait_for_feedback(
                    MqttFeedbackWait::Bubbles,
                    pending_mqtt_bool);

                bestway_bus.set_bubbles(
                    pending_mqtt_bool);
                break;

            case PendingMqttCommand::Jets:
                ESP_LOGI(
                    TAG,
                    "MQTT opdracht: jets %s",
                    pending_mqtt_bool
                        ? "ON"
                        : "OFF");

                mqtt_wait_for_feedback(
                    MqttFeedbackWait::Jets,
                    pending_mqtt_bool);

                bestway_bus.set_jets(
                    pending_mqtt_bool);
                break;

            case PendingMqttCommand::Lock:
                if (bestway_bus.model_info().family !=
                        BestwayProtocolFamily::SixWireType1 &&
                    bestway_bus.model_info().family !=
                        BestwayProtocolFamily::SixWireType2) {

                    ESP_LOGW(
                        TAG,
                        "MQTT vergrendeling genegeerd: geen 6-wire model");
                    break;
                }

                ESP_LOGI(
                    TAG,
                    "MQTT opdracht: vergrendeling %s",
                    pending_mqtt_bool
                        ? "AAN"
                        : "UIT");

                mqtt_wait_for_feedback(
                    MqttFeedbackWait::Lock,
                    pending_mqtt_bool);

                bestway_bus.set_lock(
                    pending_mqtt_bool);
                break;

            case PendingMqttCommand::Target:
                if (bestway_bus.model_info().family ==
                        BestwayProtocolFamily::Uart4Wire &&
                    !bestway_bus.godmode()) {

                    ESP_LOGW(
                        TAG,
                        "MQTT target geweigerd: 4-wire vereist Godmode");
                    break;
                }

                ESP_LOGI(
                    TAG,
                    "MQTT opdracht: target %d C",
                    pending_mqtt_target);

                mqtt_target_feedback_wait = true;
                mqtt_target_feedback_desired =
                    pending_mqtt_target;
                mqtt_target_feedback_started_us =
                    esp_timer_get_time();

                bestway_bus.set_target_temperature(
                    pending_mqtt_target);
                break;

            default:
                break;
        }

        /*
         * Niet direct na een MQTT-commando de huidige spa-state publiceren.
         * De pomp kan de opdracht nog niet verwerkt hebben; direct publiceren
         * zou dan kort de oude waarde terug naar Home Assistant sturen
         * (bijv. ON -> OFF -> ON). De normale periodieke publicatie en verse
         * spa-status blijven de werkelijke toestand terugmelden.
         */
    }

    if (pending_planner_command_ready) {
        pending_planner_command_ready = false;
        const PendingPlannerCommand cmd = pending_planner_command;
        pending_planner_command = PendingPlannerCommand{};

        bool ok = false;
        const char *result = "failed";

        ScheduleItem item;
        item.enabled = cmd.enabled;
        item.days_mask = cmd.days_mask;
        item.hour = cmd.hour;
        item.minute = cmd.minute;
        item.action = cmd.action;
        item.value = cmd.value;

        switch (cmd.op) {
            case PendingPlannerOp::Add:
                ok = scheduler_manager.add(item);
                result = ok ? "added" : "add_failed";
                break;

            case PendingPlannerOp::Update:
                ok = scheduler_manager.update(cmd.id, item);
                result = ok ? "updated" : "update_failed";
                break;

            case PendingPlannerOp::Delete:
                ok = scheduler_manager.remove(cmd.id);
                result = ok ? "deleted" : "not_found";
                break;

            case PendingPlannerOp::DeleteDay:
            {
                const ScheduleItem *existing = nullptr;
                for (uint8_t i = 0; i < scheduler_manager.count(); ++i) {
                    const ScheduleItem *candidate = scheduler_manager.get(i);
                    if (candidate && candidate->id == cmd.id) {
                        existing = candidate;
                        break;
                    }
                }

                if (existing) {
                    ScheduleItem updated = *existing;
                    updated.days_mask = static_cast<uint8_t>(
                        updated.days_mask & ~cmd.days_mask);

                    ok = updated.days_mask == 0
                        ? scheduler_manager.remove(cmd.id)
                        : scheduler_manager.update(cmd.id, updated);
                    result = ok ? "day_deleted" : "delete_day_failed";
                } else {
                    result = "not_found";
                }
                break;
            }

            case PendingPlannerOp::SetEnabled:
                ok = scheduler_manager.set_enabled(cmd.id, cmd.enabled);
                result = ok ? "enabled_changed" : "not_found";
                break;

            case PendingPlannerOp::Clear:
                ok = scheduler_manager.reset();
                result = ok ? "cleared" : "clear_failed";
                break;

            default:
                result = "invalid_operation";
                break;
        }

        ESP_LOGI(
            TAG,
            "MQTT planneropdracht: %s",
            result);

        planner_publish_requested = true;
        publish_planner_result(ok, result);
    }

    if (!config_.enabled ||
        !mqtt_client ||
        !mqtt_connected) {
        return;
    }

    const int64_t now_us =
        esp_timer_get_time();

    if (planner_publish_requested ||
        planner_last_published_revision != scheduler_manager.revision()) {
        publish_planner_state();
    }

    /*
     * Wacht na een HA-commando op de echte pompbevestiging.
     * Als SpaState de gewenste waarde ziet, publiceren we die meteen.
     * Tot die tijd blokkeren we MQTT state-publicatie zodat een oude OFF/ON
     * waarde de HA-switch niet visueel terug laat springen.
     * Na 2 seconden geven we de blokkade op en publiceren we de werkelijke
     * toestand als veiligheidsvangnet.
     */
    /*
     * Doeltemperatuur apart bevestigen.
     * Pas zodra SpaState de werkelijk door de pomp bevestigde target bevat,
     * publiceren we MQTT direct terug naar Home Assistant.
     */
    if (mqtt_target_feedback_wait) {
        const bool confirmed =
            spa.target_temperature_valid &&
            spa.target_temperature_c ==
                mqtt_target_feedback_desired;

        const bool timed_out =
            mqtt_target_feedback_started_us != 0 &&
            (now_us - mqtt_target_feedback_started_us) >=
                MQTT_FEEDBACK_TIMEOUT_US;

        if (confirmed) {
            ESP_LOGI(
                TAG,
                "MQTT pompbevestiging ontvangen: target=%d C",
                static_cast<int>(mqtt_target_feedback_desired));

            mqtt_target_feedback_wait = false;
            mqtt_target_feedback_started_us = 0;
            mqtt_publish_requested = true;

        } else if (timed_out) {
            ESP_LOGW(
                TAG,
                "MQTT target bevestiging timeout: gewenst=%d C werkelijk=%d C",
                static_cast<int>(mqtt_target_feedback_desired),
                spa.target_temperature_c);

            mqtt_target_feedback_wait = false;
            mqtt_target_feedback_started_us = 0;
            mqtt_publish_requested = true;
        }
    }

    if (mqtt_feedback_wait != MqttFeedbackWait::None) {
        const bool confirmed =
            mqtt_feedback_current_value(mqtt_feedback_wait) ==
            mqtt_feedback_desired;

        const bool timed_out =
            mqtt_feedback_started_us != 0 &&
            (now_us - mqtt_feedback_started_us) >=
                MQTT_FEEDBACK_TIMEOUT_US;

        if (confirmed) {
            ESP_LOGI(
                TAG,
                "MQTT pompbevestiging ontvangen: %s=%s",
                mqtt_feedback_name(mqtt_feedback_wait),
                mqtt_feedback_desired ? "ON" : "OFF");

            mqtt_feedback_wait = MqttFeedbackWait::None;
            mqtt_feedback_started_us = 0;
            mqtt_publish_requested = true;
        } else if (timed_out) {
            ESP_LOGW(
                TAG,
                "MQTT pompbevestiging timeout: %s gewenst=%s, werkelijk=%s",
                mqtt_feedback_name(mqtt_feedback_wait),
                mqtt_feedback_desired ? "ON" : "OFF",
                mqtt_feedback_current_value(mqtt_feedback_wait) ? "ON" : "OFF");

            mqtt_feedback_wait = MqttFeedbackWait::None;
            mqtt_feedback_started_us = 0;
            mqtt_publish_requested = true;
        } else {
            return;
        }
    }

    const bool interval_elapsed =
        mqtt_last_publish_us == 0 ||
        (now_us - mqtt_last_publish_us) >=
            5000000LL;

    if (!mqtt_publish_requested &&
        !interval_elapsed) {
        return;
    }

    mqtt_publish_requested = false;
    mqtt_last_publish_us = now_us;

    std::string ip;
    int rssi = 0;

    get_network_state(ip, rssi);

    const bool ready =
        spa.connected &&
        spa.data_valid;

    const esp_app_desc_t *app =
        esp_app_get_description();

    /*
     * Godmode is uitsluitend een 4-wire status.
     * De state-publicatie gebruikt dezelfde configureerbare
     * publicatiearchitectuur als de overige MQTT-statussen.
     */
    if (bestway_bus.model_info().family ==
        BestwayProtocolFamily::Uart4Wire) {

        publish_value(
            config_,
            config_.godmode,
            on_off(bestway_bus.godmode()));

        publish_value(
            config_,
            config_.full_power,
            on_off(bestway_bus.godmode_full_power()));
    }

    publish_value(
        config_,
        config_.temperature,
        std::to_string(spa.temperature_c));

    publish_value(
        config_,
        config_.target,
        std::to_string(spa.target_temperature_c));

    publish_value(
        config_,
        config_.power,
        on_off(spa.power));

    publish_value(
        config_,
        config_.heater,
        on_off(spa.heater));

    publish_value(
        config_,
        config_.heating_active,
        on_off(spa.heater_active));

    publish_value(
        config_,
        config_.filter,
        on_off(spa.filter));

    publish_value(
        config_,
        config_.bubbles,
        on_off(spa.bubbles));

    if (bestway_bus.has_jets()) {
        publish_value(
            config_,
            config_.jets,
            on_off(spa.jets));
    }

    publish_value(
        config_,
        config_.locked,
        on_off(spa.locked));

    publish_value(
        config_,
        config_.connected,
        on_off(spa.connected));

    publish_value(
        config_,
        config_.ready,
        on_off(ready));

    publish_value(
        config_,
        config_.rssi,
        std::to_string(rssi));

    publish_value(
        config_,
        config_.heap,
        std::to_string(
            static_cast<unsigned>(
                esp_get_free_heap_size())));

    publish_value(
        config_,
        config_.uptime,
        spa.uptime());

    publish_value(
        config_,
        config_.firmware,
        app ? app->version : "");

    publish_value(
        config_,
        config_.ip,
        ip);

    publish_value(
        config_,
        config_.maintenance,
        mqtt_maintenance_status());

    publish_value(
        config_,
        config_.maintenance_filter_replace,
        mqtt_maintenance_remaining_days(0));

    publish_value(
        config_,
        config_.maintenance_filter_clean,
        mqtt_maintenance_remaining_days(1));

    publish_value(
        config_,
        config_.maintenance_chlorine,
        mqtt_maintenance_remaining_days(2));

    publish_value(
        config_,
        config_.json,
        mqtt_state_json());
}


bool MqttManager::connected() const
{
    return mqtt_connected;
}


uint32_t MqttManager::reconnect_count() const
{
    return mqtt_reconnect_count;
}


int MqttManager::last_error() const
{
    return mqtt_last_error;
}


bool MqttManager::load()
{
    nvs_handle_t handle;

    esp_err_t err =
        nvs_open_from_partition(
            "bridge_info",
            NVS_NAMESPACE,
            NVS_READONLY,
            &handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return false;
    }

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "nvs_open load fout: %s",
            esp_err_to_name(err));

        return false;
    }

    MqttConfig loaded;

    uint8_t enabled =
        loaded.enabled ? 1 : 0;

    uint8_t discovery =
        loaded.home_assistant_discovery ? 1 : 0;

    uint16_t port =
        loaded.port;

    bool ok = true;

    err = nvs_get_u8(
        handle,
        "enabled",
        &enabled);

    if (err != ESP_OK &&
        err != ESP_ERR_NVS_NOT_FOUND) {
        ok = false;
    }

    err = nvs_get_u16(
        handle,
        "port",
        &port);

    if (err != ESP_OK &&
        err != ESP_ERR_NVS_NOT_FOUND) {
        ok = false;
    }

    err = nvs_get_u8(
        handle,
        "discovery",
        &discovery);

    if (err != ESP_OK &&
        err != ESP_ERR_NVS_NOT_FOUND) {
        ok = false;
    }

    ok &= nvs_get_string(handle, "host", loaded.host);
    ok &= nvs_get_string(handle, "user", loaded.username);
    ok &= nvs_get_string(handle, "pass", loaded.password);
    ok &= nvs_get_string(handle, "client_id", loaded.client_id);
    ok &= nvs_get_string(handle, "base_topic", loaded.base_topic);

    ok &= load_publication(handle, "p_temp",   "t_temp",   loaded.temperature);
    ok &= load_publication(handle, "p_target", "t_target", loaded.target);
    ok &= load_publication(handle, "p_power",  "t_power",  loaded.power);
    ok &= load_publication(handle, "p_heater", "t_heater", loaded.heater);
    ok &= load_publication(handle, "p_hactive","t_hactive",loaded.heating_active);
    ok &= load_publication(handle, "p_god",    "t_god",    loaded.godmode);
    ok &= load_publication(handle, "p_full",   "t_full",   loaded.full_power);
    ok &= load_publication(handle, "p_filter", "t_filter", loaded.filter);
    ok &= load_publication(handle, "p_bubbles","t_bubbles",loaded.bubbles);
    ok &= load_publication(handle, "p_jets",   "t_jets",   loaded.jets);
    ok &= load_publication(handle, "p_locked", "t_locked", loaded.locked);
    ok &= load_publication(handle, "p_conn",   "t_conn",   loaded.connected);
    ok &= load_publication(handle, "p_ready",  "t_ready",  loaded.ready);
    ok &= load_publication(handle, "p_rssi",   "t_rssi",   loaded.rssi);
    ok &= load_publication(handle, "p_heap",   "t_heap",   loaded.heap);
    ok &= load_publication(handle, "p_uptime", "t_uptime", loaded.uptime);
    ok &= load_publication(handle, "p_fw",     "t_fw",     loaded.firmware);
    ok &= load_publication(handle, "p_ip",     "t_ip",     loaded.ip);
    ok &= load_publication(handle, "p_json",   "t_json",   loaded.json);
    ok &= load_publication(handle, "p_maint",  "t_maint",  loaded.maintenance);
    ok &= load_publication(handle, "p_mfr",    "t_mfr",    loaded.maintenance_filter_replace);
    ok &= load_publication(handle, "p_mfc",    "t_mfc",    loaded.maintenance_filter_clean);
    ok &= load_publication(handle, "p_mcl",    "t_mcl",    loaded.maintenance_chlorine);

    nvs_close(handle);

    if (!ok) {
        return false;
    }

    loaded.enabled =
        enabled != 0;

    loaded.home_assistant_discovery =
        discovery != 0;

    if (port == 0) {
        port = 1883;
    }

    loaded.port = port;

    if (loaded.client_id.empty()) {
        loaded.client_id =
            "LayZSpaController";
    }

    if (loaded.base_topic.empty()) {
        loaded.base_topic =
            "layzspa";
    }

    config_ = std::move(loaded);

    return true;
}


bool MqttManager::save()
{
    nvs_handle_t handle;

    esp_err_t err =
        nvs_open_from_partition(
            "bridge_info",
            NVS_NAMESPACE,
            NVS_READWRITE,
            &handle);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "nvs_open save fout: %s",
            esp_err_to_name(err));

        return false;
    }

    bool ok = true;

    ok &= nvs_set_u8(
              handle,
              "enabled",
              config_.enabled ? 1 : 0) == ESP_OK;

    ok &= nvs_set_u16(
              handle,
              "port",
              config_.port) == ESP_OK;

    ok &= nvs_set_u8(
              handle,
              "discovery",
              config_.home_assistant_discovery ? 1 : 0) == ESP_OK;

    ok &= nvs_set_string(handle, "host",       config_.host);
    ok &= nvs_set_string(handle, "user",       config_.username);
    ok &= nvs_set_string(handle, "pass",       config_.password);
    ok &= nvs_set_string(handle, "client_id",  config_.client_id);
    ok &= nvs_set_string(handle, "base_topic", config_.base_topic);

    ok &= save_publication(handle, "p_temp",   "t_temp",   config_.temperature);
    ok &= save_publication(handle, "p_target", "t_target", config_.target);
    ok &= save_publication(handle, "p_power",  "t_power",  config_.power);
    ok &= save_publication(handle, "p_heater", "t_heater", config_.heater);
    ok &= save_publication(handle, "p_hactive","t_hactive",config_.heating_active);
    ok &= save_publication(handle, "p_god",    "t_god",    config_.godmode);
    ok &= save_publication(handle, "p_full",   "t_full",   config_.full_power);
    ok &= save_publication(handle, "p_filter", "t_filter", config_.filter);
    ok &= save_publication(handle, "p_bubbles","t_bubbles",config_.bubbles);
    ok &= save_publication(handle, "p_jets",   "t_jets",   config_.jets);
    ok &= save_publication(handle, "p_locked", "t_locked", config_.locked);
    ok &= save_publication(handle, "p_conn",   "t_conn",   config_.connected);
    ok &= save_publication(handle, "p_ready",  "t_ready",  config_.ready);
    ok &= save_publication(handle, "p_rssi",   "t_rssi",   config_.rssi);
    ok &= save_publication(handle, "p_heap",   "t_heap",   config_.heap);
    ok &= save_publication(handle, "p_uptime", "t_uptime", config_.uptime);
    ok &= save_publication(handle, "p_fw",     "t_fw",     config_.firmware);
    ok &= save_publication(handle, "p_ip",     "t_ip",     config_.ip);
    ok &= save_publication(handle, "p_json",   "t_json",   config_.json);
    ok &= save_publication(handle, "p_maint",  "t_maint",  config_.maintenance);
    ok &= save_publication(handle, "p_mfr",    "t_mfr",    config_.maintenance_filter_replace);
    ok &= save_publication(handle, "p_mfc",    "t_mfc",    config_.maintenance_filter_clean);
    ok &= save_publication(handle, "p_mcl",    "t_mcl",    config_.maintenance_chlorine);

    if (ok) {
        ok =
            nvs_commit(handle) == ESP_OK;
    }

    nvs_close(handle);

    return ok;
}


const MqttConfig &MqttManager::config() const
{
    return config_;
}


MqttConfig &MqttManager::config()
{
    return config_;
}
