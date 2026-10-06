#pragma once

#include <cstdint>
#include <string>

struct MqttPublicationConfig {
    bool enabled = false;
    std::string topic;
};

struct MqttConfig {
    bool enabled = false;

    std::string host;
    uint16_t port = 1883;

    std::string username;
    std::string password;

    std::string client_id = "LayZSpaController";
    std::string base_topic = "layzspa";

    bool home_assistant_discovery = true;

    MqttPublicationConfig temperature   {false, "temperature"};
    MqttPublicationConfig target        {false, "target/state"};
    MqttPublicationConfig power         {false, "power/state"};
    MqttPublicationConfig heater        {false, "heater/state"};
    MqttPublicationConfig heating_active{false, "heater/active"};
    MqttPublicationConfig godmode       {false, "state/godmode"};
    MqttPublicationConfig full_power    {false, "state/fullpower"};
    MqttPublicationConfig filter        {false, "filter/state"};
    MqttPublicationConfig bubbles       {false, "bubbles/state"};
    MqttPublicationConfig jets          {false, "jets/state"};
    MqttPublicationConfig locked        {false, "lock/state"};
    MqttPublicationConfig connected     {false, "spa/connected"};
    MqttPublicationConfig ready         {false, "spa/ready"};
    MqttPublicationConfig rssi          {false, "system/rssi"};
    MqttPublicationConfig heap          {false, "system/free_heap"};
    MqttPublicationConfig uptime        {false, "system/uptime"};
    MqttPublicationConfig firmware      {false, "system/firmware"};
    MqttPublicationConfig ip            {false, "system/ip"};
    MqttPublicationConfig json          {false, "state"};
    MqttPublicationConfig maintenance   {false, "maintenance"};
    MqttPublicationConfig maintenance_filter_replace {false, "maintenance/filter_replace_days"};
    MqttPublicationConfig maintenance_filter_clean   {false, "maintenance/filter_clean_days"};
    MqttPublicationConfig maintenance_chlorine       {false, "maintenance/chlorine_days"};
};

class MqttManager {
public:
    bool begin();

    bool load();
    bool save();

    bool start();
    void stop();
    void loop();
    bool connected() const;

    uint32_t reconnect_count() const;
    int last_error() const;

    const MqttConfig &config() const;
    MqttConfig &config();

private:
    MqttConfig config_;
};

extern MqttManager mqtt_manager;
