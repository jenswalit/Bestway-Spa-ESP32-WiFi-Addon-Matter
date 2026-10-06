#include "scheduler_manager.h"

#include <ctime>
#include <cstring>

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

#include "bestway_bus.h"

namespace {

static const char *TAG = "scheduler";

static constexpr const char *NVS_NAMESPACE = "spa_sched";
static constexpr const char *NVS_ITEMS_KEY = "items";
static constexpr const char *NVS_COUNT_KEY = "count";
static constexpr const char *NVS_NEXT_ID_KEY = "next_id";

uint8_t weekday_to_mask(uint8_t weekday)
{
    // tm_wday: 0=zondag, 1=maandag, ... 6=zaterdag.
    return static_cast<uint8_t>(1U << weekday);
}

struct StoredScheduleItem {
    uint16_t id;
    uint8_t enabled;
    uint8_t days_mask;
    uint8_t hour;
    uint8_t minute;
    uint8_t action;
    int32_t value;
};

} // namespace

SchedulerManager scheduler_manager;

bool SchedulerManager::begin()
{
    schedule_count_ = 0;
    next_id_ = 1;
    revision_ = 1;

    if (!load()) {
        ESP_LOGW(TAG, "Geen geldige plannerdata; starten met lege planner");
        schedule_count_ = 0;
        next_id_ = 1;
        return save();
    }

    ESP_LOGI(TAG,
             "Planner geladen: %u schema('s)",
             static_cast<unsigned>(schedule_count_));

    return true;
}

bool SchedulerManager::load()
{
    nvs_handle_t nvs = 0;

    esp_err_t err =
        nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return false;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Planner NVS openen mislukt: %s",
                 esp_err_to_name(err));
        return false;
    }

    uint8_t count = 0;

    err = nvs_get_u8(nvs, NVS_COUNT_KEY, &count);

    if (err != ESP_OK || count > MAX_SCHEDULES) {
        nvs_close(nvs);
        return false;
    }

    uint16_t next_id = 1;

    if (nvs_get_u16(nvs, NVS_NEXT_ID_KEY, &next_id) != ESP_OK ||
        next_id == 0) {
        next_id = 1;
    }

    StoredScheduleItem stored[MAX_SCHEDULES] = {};

    if (count > 0) {
        size_t blob_size =
            static_cast<size_t>(count) *
            sizeof(StoredScheduleItem);

        err = nvs_get_blob(
            nvs,
            NVS_ITEMS_KEY,
            stored,
            &blob_size);

        if (err != ESP_OK ||
            blob_size !=
                static_cast<size_t>(count) *
                    sizeof(StoredScheduleItem)) {
            nvs_close(nvs);
            return false;
        }
    }

    nvs_close(nvs);

    schedule_count_ = count;
    next_id_ = next_id;

    for (uint8_t i = 0; i < schedule_count_; ++i) {
        schedules_[i].id = stored[i].id;
        schedules_[i].enabled = stored[i].enabled != 0;
        schedules_[i].days_mask = stored[i].days_mask;
        schedules_[i].hour = stored[i].hour;
        schedules_[i].minute = stored[i].minute;
        schedules_[i].action =
            static_cast<ScheduleAction>(stored[i].action);
        schedules_[i].value = stored[i].value;
        schedules_[i].last_run_minute = -1;
    }

    return true;
}

bool SchedulerManager::save()
{
    nvs_handle_t nvs = 0;

    esp_err_t err =
        nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Planner NVS openen voor schrijven mislukt: %s",
                 esp_err_to_name(err));
        return false;
    }

    StoredScheduleItem stored[MAX_SCHEDULES] = {};

    for (uint8_t i = 0; i < schedule_count_; ++i) {
        stored[i].id = schedules_[i].id;
        stored[i].enabled = schedules_[i].enabled ? 1 : 0;
        stored[i].days_mask = schedules_[i].days_mask;
        stored[i].hour = schedules_[i].hour;
        stored[i].minute = schedules_[i].minute;
        stored[i].action =
            static_cast<uint8_t>(schedules_[i].action);
        stored[i].value = schedules_[i].value;
    }

    err = nvs_set_u8(
        nvs,
        NVS_COUNT_KEY,
        schedule_count_);

    if (err == ESP_OK) {
        err = nvs_set_u16(
            nvs,
            NVS_NEXT_ID_KEY,
            next_id_);
    }

    if (err == ESP_OK) {
        if (schedule_count_ > 0) {
            err = nvs_set_blob(
                nvs,
                NVS_ITEMS_KEY,
                stored,
                static_cast<size_t>(schedule_count_) *
                    sizeof(StoredScheduleItem));
        } else {
            const esp_err_t erase_err =
                nvs_erase_key(nvs, NVS_ITEMS_KEY);

            if (erase_err != ESP_OK &&
                erase_err != ESP_ERR_NVS_NOT_FOUND) {
                err = erase_err;
            }
        }
    }

    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Planner opslaan mislukt: %s",
                 esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG,
             "Planner opgeslagen: %u schema('s)",
             static_cast<unsigned>(schedule_count_));

    return true;
}

bool SchedulerManager::reset()
{
    schedule_count_ = 0;
    next_id_ = 1;
    ++revision_;

    return save();
}

uint8_t SchedulerManager::count() const
{
    return schedule_count_;
}

const ScheduleItem *SchedulerManager::get(
    uint8_t index) const
{
    if (index >= schedule_count_) {
        return nullptr;
    }

    return &schedules_[index];
}

bool SchedulerManager::add(
    const ScheduleItem &item)
{
    if (schedule_count_ >= MAX_SCHEDULES) {
        return false;
    }

    ScheduleItem new_item = item;

    new_item.id = next_id_++;
    new_item.last_run_minute = -1;

    schedules_[schedule_count_++] = new_item;
    ++revision_;

    return save();
}

bool SchedulerManager::update(
    uint16_t id,
    const ScheduleItem &item)
{
    const int index = find_index_by_id(id);

    if (index < 0) {
        return false;
    }

    ScheduleItem updated = item;

    updated.id = id;
    updated.last_run_minute = -1;

    schedules_[index] = updated;
    ++revision_;

    return save();
}

bool SchedulerManager::remove(uint16_t id)
{
    const int index = find_index_by_id(id);

    if (index < 0) {
        return false;
    }

    for (uint8_t i = static_cast<uint8_t>(index);
         i + 1 < schedule_count_;
         ++i) {
        schedules_[i] = schedules_[i + 1];
    }

    --schedule_count_;
    ++revision_;

    return save();
}

bool SchedulerManager::set_enabled(
    uint16_t id,
    bool enabled)
{
    const int index = find_index_by_id(id);

    if (index < 0) {
        return false;
    }

    schedules_[index].enabled = enabled;
    ++revision_;

    return save();
}

ScheduleDayResult SchedulerManager::set_day_enabled(
    uint16_t id, uint8_t day_mask, bool enabled)
{
    const int index = find_index_by_id(id);
    if (index < 0) return ScheduleDayResult::NotFound;
    // Only one existing weekday may be addressed by a day switch.
    if (day_mask == 0 || day_mask > 64 ||
        (day_mask & (day_mask - 1)) != 0 ||
        (schedules_[index].days_mask & day_mask) == 0) {
        return ScheduleDayResult::InvalidDay;
    }
    const ScheduleItem source = schedules_[index];
    if (source.enabled == enabled) return ScheduleDayResult::Ok;

    const uint8_t remaining = static_cast<uint8_t>(source.days_mask & ~day_mask);
    int target = -1;
    // Combine only equivalent records with disjoint days. This also reunites a
    // split week when its disabled day is switched on again.
    for (uint8_t i = 0; i < schedule_count_; ++i) {
        const ScheduleItem &candidate = schedules_[i];
        if (i != index && candidate.enabled == enabled &&
            candidate.hour == source.hour && candidate.minute == source.minute &&
            candidate.action == source.action && candidate.value == source.value &&
            (candidate.days_mask & source.days_mask) == 0) {
            target = i;
            break;
        }
    }
    if (target < 0 && remaining != 0 && schedule_count_ >= MAX_SCHEDULES) {
        return ScheduleDayResult::Full;
    }

    // One NVS save for the complete change; restore RAM on a failed save.
    ScheduleItem previous[MAX_SCHEDULES];
    std::memcpy(previous, schedules_, sizeof(previous));
    const uint8_t previous_count = schedule_count_;
    const uint16_t previous_next_id = next_id_;

    if (target >= 0) {
        schedules_[target].days_mask |= day_mask;
        if (remaining != 0) {
            schedules_[index].days_mask = remaining;
        } else {
            for (uint8_t i = static_cast<uint8_t>(index); i + 1 < schedule_count_; ++i) {
                schedules_[i] = schedules_[i + 1];
            }
            --schedule_count_;
        }
    } else if (remaining == 0) {
        schedules_[index].enabled = enabled;
    } else {
        ScheduleItem day = source;
        // Skip ID zero and live IDs when the 16-bit counter wraps.
        while (next_id_ == 0 || find_index_by_id(next_id_) >= 0) ++next_id_;
        day.id = next_id_++;
        day.days_mask = day_mask;
        day.enabled = enabled;
        schedules_[index].days_mask = remaining;
        schedules_[schedule_count_++] = day;
    }
    if (!save()) {
        std::memcpy(schedules_, previous, sizeof(previous));
        schedule_count_ = previous_count;
        next_id_ = previous_next_id;
        return ScheduleDayResult::SaveFailed;
    }
    ++revision_;
    return ScheduleDayResult::Ok;
}

uint32_t SchedulerManager::revision() const
{
    return revision_;
}

int SchedulerManager::find_index_by_id(
    uint16_t id) const
{
    for (uint8_t i = 0; i < schedule_count_; ++i) {
        if (schedules_[i].id == id) {
            return i;
        }
    }

    return -1;
}

bool SchedulerManager::should_run(
    const ScheduleItem &item,
    uint8_t weekday,
    uint8_t hour,
    uint8_t minute) const
{
    if (!item.enabled ||
        item.days_mask == 0) {
        return false;
    }

    if ((item.days_mask &
         weekday_to_mask(weekday)) == 0) {
        return false;
    }

    return item.hour == hour &&
           item.minute == minute;
}

void SchedulerManager::execute(
    const ScheduleItem &item)
{
    switch (item.action) {
        case ScheduleAction::HeaterOn:
            bestway_bus.set_heater(true);
            break;

        case ScheduleAction::HeaterOff:
            bestway_bus.set_heater(false);
            break;

        case ScheduleAction::FilterOn:
            bestway_bus.set_filter(true);
            break;

        case ScheduleAction::FilterOff:
            bestway_bus.set_filter(false);
            break;

        case ScheduleAction::BubblesOn:
            bestway_bus.set_bubbles(true);
            break;

        case ScheduleAction::BubblesOff:
            bestway_bus.set_bubbles(false);
            break;

        case ScheduleAction::JetsOn:
            bestway_bus.set_jets(true);
            break;

        case ScheduleAction::JetsOff:
            bestway_bus.set_jets(false);
            break;

        case ScheduleAction::SetTargetTemperature: {
            int target = item.value;

            if (target < 20) target = 20;
            if (target > 40) target = 40;

            bestway_bus.set_target_temperature(target);
            break;
        }
    }

    ESP_LOGI(TAG,
             "Planner uitgevoerd, id=%u",
             static_cast<unsigned>(item.id));
}

void SchedulerManager::loop()
{
    static int64_t last_check_ms = 0;

    const int64_t now_ms =
        esp_timer_get_time() / 1000;

    if (now_ms - last_check_ms < 1000) {
        return;
    }

    last_check_ms = now_ms;

    const time_t now = time(nullptr);

    // Geen geldige netwerktijd beschikbaar.
    if (now <= 1577836800) {
        return;
    }

    struct tm local = {};

    if (!localtime_r(&now, &local)) {
        return;
    }

    const uint8_t weekday =
        static_cast<uint8_t>(local.tm_wday);

    const uint8_t hour =
        static_cast<uint8_t>(local.tm_hour);

    const uint8_t minute =
        static_cast<uint8_t>(local.tm_min);

    const int64_t current_minute =
        static_cast<int64_t>(now / 60);

    for (uint8_t i = 0;
         i < schedule_count_;
         ++i) {

        ScheduleItem &item = schedules_[i];

        if (!should_run(
                item,
                weekday,
                hour,
                minute)) {
            continue;
        }

        if (item.last_run_minute ==
            current_minute) {
            continue;
        }

        execute(item);
        item.last_run_minute =
            current_minute;
    }
}
