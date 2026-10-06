#pragma once

#include <cstdint>

enum class ScheduleAction : uint8_t {
    HeaterOn = 0,
    HeaterOff = 1,
    FilterOn = 2,
    FilterOff = 3,
    BubblesOn = 4,
    BubblesOff = 5,
    JetsOn = 6,
    JetsOff = 7,
    SetTargetTemperature = 8
};

enum class ScheduleDayResult : uint8_t {
    Ok, NotFound, InvalidDay, Full, SaveFailed
};

struct ScheduleItem {
    uint16_t id = 0;
    bool enabled = true;

    uint8_t days_mask = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;

    ScheduleAction action = ScheduleAction::HeaterOn;
    int32_t value = 0;

    // Alleen runtime; wordt niet gebruikt als planner-ID.
    int64_t last_run_minute = -1;
};

class SchedulerManager {
public:
    static constexpr uint8_t MAX_SCHEDULES = 24;

    bool begin();
    void loop();

    bool load();
    bool save();
    bool reset();

    uint8_t count() const;
    const ScheduleItem *get(uint8_t index) const;

    bool add(const ScheduleItem &item);
    bool update(uint16_t id, const ScheduleItem &item);
    bool remove(uint16_t id);
    bool set_enabled(uint16_t id, bool enabled);

    // Splits/combines existing records; the stored/MQTT format stays unchanged.
    ScheduleDayResult set_day_enabled(uint16_t id, uint8_t day_mask, bool enabled);

    // Verandert bij iedere plannerwijziging; gebruikt voor MQTT-sync.
    uint32_t revision() const;

private:
    ScheduleItem schedules_[MAX_SCHEDULES] = {};
    uint8_t schedule_count_ = 0;
    uint16_t next_id_ = 1;
    uint32_t revision_ = 1;

    int find_index_by_id(uint16_t id) const;

    bool should_run(
        const ScheduleItem &item,
        uint8_t weekday,
        uint8_t hour,
        uint8_t minute) const;

    void execute(const ScheduleItem &item);
};

extern SchedulerManager scheduler_manager;
