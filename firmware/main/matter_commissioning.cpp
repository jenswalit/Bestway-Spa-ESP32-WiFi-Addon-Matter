#include "matter_commissioning.h"
#include <esp_matter_providers.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/CHIPDeviceLayer.h>
#include <app/server/Server.h>
#include <app/server/CommissioningWindowManager.h>
#include <crypto/CHIPCryptoPAL.h>
#include <setup_payload/SetupPayload.h>
#include <setup_payload/ManualSetupPayloadGenerator.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <bootloader_random.h>
#include <esp_random.h>
#include <esp_mac.h>
#include <esp_timer.h>
#include <nvs.h>
#include <cJSON.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <atomic>
#include <esp_log.h>

namespace {
constexpr uint32_t kVersion = 1;
constexpr uint32_t kIterations = 1000;
constexpr uint32_t kWindowSeconds = 300;
constexpr const char *kNamespace = "bwc_pair";
struct Credentials {
    uint32_t version;
    uint32_t pin;
    uint16_t discriminator;
    uint16_t reserved;
    uint8_t salt[16];
};
static_assert(sizeof(Credentials) == 28, "NVS record format changed");
Credentials base{};
uint8_t chip_id[6]{};
char module_id[32]{};
bool ready = false;
uint8_t verifier_bytes[chip::Crypto::kSpake2p_VerifierSerialized_Length]{};
std::string temporary_manual, temporary_qr;
int64_t temporary_deadline_us = 0;
std::atomic<bool> window_opening{false};
std::atomic<esp_err_t> window_open_error{ESP_OK};

bool valid(const Credentials &c) {
    return c.version == kVersion && c.reserved == 0 &&
        c.discriminator <= chip::kMaxDiscriminatorValue &&
        chip::PayloadContents::IsValidSetupPIN(c.pin) && c.pin != 20202021;
}

esp_err_t generate(Credentials &c) {
    // Public eFuse identity provides uniqueness; secret hardware entropy provides
    // unpredictability. A hash of the public identity alone is never used.
    uint8_t input[38], digest[32];
    std::memcpy(input + 32, chip_id, sizeof(chip_id));
    c = {};
    do {
        esp_fill_random(input, 32);
        const CHIP_ERROR err = chip::Crypto::Hash_SHA256(input, sizeof(input), digest);
        if (err != CHIP_NO_ERROR) {
            chip::Crypto::ClearSecretData(input);
            chip::Crypto::ClearSecretData(digest);
            return ESP_FAIL;
        }
        uint32_t sample = 0;
        std::memcpy(&sample, digest, sizeof(sample));
        // Rejection sampling avoids modulo bias across the valid PIN range.
        constexpr uint32_t range = 99999998;
        constexpr uint64_t limit = (UINT64_C(1) << 32) / range * range;
        if (sample >= limit) continue;
        c.version = kVersion;
        c.pin = sample % range + 1;
        c.discriminator = (uint16_t(digest[4]) | (uint16_t(digest[5]) << 8)) & 0xFFF;
        std::memcpy(c.salt, digest + 16, sizeof(c.salt));
    } while (!valid(c));
    chip::Crypto::ClearSecretData(input);
    chip::Crypto::ClearSecretData(digest);
    return ESP_OK;
}

esp_err_t save(const Credentials &c) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(handle, "credentials", &c, sizeof(c));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

CHIP_ERROR codes(const Credentials &c, bool on_network, std::string &manual, std::string &qr) {
    chip::SetupPayload payload;
    payload.version = 0;
    payload.vendorID = CONFIG_DEVICE_VENDOR_ID;
    payload.productID = CONFIG_DEVICE_PRODUCT_ID;
    payload.commissioningFlow = chip::CommissioningFlow::kStandard;
    payload.rendezvousInformation.SetValue(chip::RendezvousInformationFlags(
        on_network ? chip::RendezvousInformationFlag::kOnNetwork : chip::RendezvousInformationFlag::kBLE));
    payload.discriminator.SetLongValue(c.discriminator);
    payload.setUpPINCode = c.pin;
    CHIP_ERROR err = chip::ManualSetupPayloadGenerator(payload).payloadDecimalStringRepresentation(manual);
    if (err == CHIP_NO_ERROR)
        err = chip::QRCodeSetupPayloadGenerator(payload).payloadBase38Representation(qr);
    return err;
}

class Provider final : public chip::DeviceLayer::CommissionableDataProvider {
public:
    CHIP_ERROR GetSetupDiscriminator(uint16_t &v) override {
        if (!ready) return CHIP_ERROR_INCORRECT_STATE;
        v = base.discriminator; return CHIP_NO_ERROR;
    }
    CHIP_ERROR SetSetupDiscriminator(uint16_t) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
    CHIP_ERROR GetSpake2pIterationCount(uint32_t &v) override {
        if (!ready) return CHIP_ERROR_INCORRECT_STATE;
        v = kIterations; return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetSpake2pSalt(chip::MutableByteSpan &v) override {
        if (!ready) return CHIP_ERROR_INCORRECT_STATE;
        if (v.size() < sizeof(base.salt)) return CHIP_ERROR_BUFFER_TOO_SMALL;
        std::memcpy(v.data(), base.salt, sizeof(base.salt)); v.reduce_size(sizeof(base.salt));
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetSpake2pVerifier(chip::MutableByteSpan &v, size_t &len) override {
        len = sizeof(verifier_bytes);
        if (!ready) return CHIP_ERROR_INCORRECT_STATE;
        if (v.size() < len) return CHIP_ERROR_BUFFER_TOO_SMALL;
        std::memcpy(v.data(), verifier_bytes, len); v.reduce_size(len);
        return CHIP_NO_ERROR;
    }
    // PASE uses the verifier. Do not hand the PIN to SDK onboarding log helpers.
    // The local web status obtains the payload directly from our own record.
    CHIP_ERROR GetSetupPasscode(uint32_t &) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
    CHIP_ERROR SetSetupPasscode(uint32_t) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
} provider;
}

esp_err_t bestway_matter_credentials_init() {
    esp_err_t err = esp_efuse_mac_get_default(chip_id);
    if (err != ESP_OK) return err;
    std::snprintf(module_id, sizeof(module_id), "BWC-C6-%02X%02X%02X%02X%02X%02X",
        chip_id[0], chip_id[1], chip_id[2], chip_id[3], chip_id[4], chip_id[5]);
    nvs_handle_t handle;
    err = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        size_t size = sizeof(base);
        err = nvs_get_blob(handle, "credentials", &base, &size);
        nvs_close(handle);
        if (err == ESP_OK && (size != sizeof(base) || !valid(base))) return ESP_ERR_INVALID_STATE;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Entropy source enabled only here, before Wi-Fi/BLE/ADC are initialized.
        bootloader_random_enable();
        err = generate(base);
        bootloader_random_disable();
        if (err == ESP_OK) err = save(base);
    }
    if (err != ESP_OK) return err; // Never replace a corrupt/unreadable record automatically.
    chip::Crypto::Spake2pVerifier verifier;
    CHIP_ERROR chip_err = verifier.Generate(kIterations, chip::ByteSpan(base.salt), base.pin);
    chip::MutableByteSpan bytes(verifier_bytes);
    if (chip_err == CHIP_NO_ERROR) chip_err = verifier.Serialize(bytes);
    if (chip_err != CHIP_NO_ERROR) return ESP_FAIL;
    std::string manual, qr;
    if (codes(base, false, manual, qr) != CHIP_NO_ERROR || manual.size() != 11) return ESP_FAIL;
    ready = true;
    return ESP_OK;
}

void bestway_matter_register_provider() {
    esp_matter::set_custom_commissionable_data_provider(&provider);
}

esp_err_t bestway_matter_rotate_credentials() {
    // Explicit reset runs while Matter/BLE/Wi-Fi is active: hardware RNG has entropy.
    // Do not mutate the running provider; commit the replacement for the next boot.
    Credentials replacement;
    esp_err_t err = generate(replacement);
    if (err == ESP_OK) err = save(replacement);
    chip::Crypto::ClearSecretData(reinterpret_cast<uint8_t *>(&replacement), sizeof(replacement));
    return err;
}

std::string bestway_matter_status_json(bool runtime_active) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return "{}";
    cJSON_AddStringToObject(root, "moduleId", module_id);
    cJSON_AddBoolToObject(root, "credentialsReady", ready);
    std::string manual, qr;
    if (ready) codes(base, false, manual, qr);
    cJSON_AddStringToObject(root, "manualPairingCode", manual.c_str());
    cJSON_AddStringToObject(root, "qrPayload", qr.c_str());
    cJSON *fabrics = cJSON_AddArrayToObject(root, "fabrics");
    unsigned count = 0;
    bool window_open = false;
    if (runtime_active) {
        chip::DeviceLayer::PlatformMgr().LockChipStack();
        auto &server = chip::Server::GetInstance();
        window_open = server.GetCommissioningWindowManager().IsCommissioningWindowOpen();
        for (const auto &fabric : server.GetFabricTable()) {
            ++count;
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "index", fabric.GetFabricIndex());
            cJSON_AddNumberToObject(item, "vendorId", static_cast<uint16_t>(fabric.GetVendorId()));
            const auto label = fabric.GetFabricLabel();
            const std::string label_string(label.data(), label.size());
            cJSON_AddStringToObject(item, "label", label_string.c_str());
            char id[24];
            std::snprintf(id, sizeof(id), "%016llX", static_cast<unsigned long long>(fabric.GetFabricId()));
            cJSON_AddStringToObject(item, "fabricId", id);
            cJSON_AddItemToArray(fabrics, item);
        }
        const int64_t remaining = temporary_deadline_us - esp_timer_get_time();
        if (!window_open || remaining <= 0) {
            temporary_manual.clear(); temporary_qr.clear(); temporary_deadline_us = 0;
        }
        cJSON_AddStringToObject(root, "temporaryPairingCode", temporary_manual.c_str());
        cJSON_AddStringToObject(root, "temporaryQrPayload", temporary_qr.c_str());
        cJSON_AddNumberToObject(root, "windowRemainingSeconds", temporary_deadline_us ? (remaining + 999999) / 1000000 : 0);
        chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    }
    cJSON_AddBoolToObject(root, "windowOpening", window_opening.load());
    cJSON_AddNumberToObject(root, "windowOpenError", window_open_error.load());
    cJSON_AddNumberToObject(root, "fabricCount", count);
    cJSON_AddBoolToObject(root, "commissioningWindowOpen", window_open);
    char *json = cJSON_PrintUnformatted(root);
    std::string result = json ? json : "{}";
    cJSON_free(json); cJSON_Delete(root);
    return result;
}

static esp_err_t open_window_on_matter_thread() {
    if (!ready) return ESP_ERR_INVALID_STATE;
    // ScheduleWork runs on the existing CHIP event-loop task, with its stack lock held.
    auto &server = chip::Server::GetInstance();
    auto &manager = server.GetCommissioningWindowManager();
    const bool commissioned = server.GetFabricTable().FabricCount() > 0;
    if (manager.IsCommissioningWindowOpen()) {
        return commissioned ? ESP_ERR_INVALID_STATE : ESP_OK;
    }
    if (!commissioned) {
        CHIP_ERROR err = manager.OpenBasicCommissioningWindow(chip::System::Clock::Seconds32(kWindowSeconds));
        return err == CHIP_NO_ERROR ? ESP_OK : ESP_FAIL;
    }
    // Derivation uses the Matter task stack; the HTTP task never performs P256 work.
    ESP_LOGI("bwc_commissioning", "Tijdelijke credentials genereren");
    Credentials temporary;
    if (generate(temporary) != ESP_OK) {
        chip::Crypto::ClearSecretData(reinterpret_cast<uint8_t *>(&temporary), sizeof(temporary));
        return ESP_FAIL;
    }
    ESP_LOGI("bwc_commissioning", "SPAKE2 verifier berekenen");
    chip::Crypto::Spake2pVerifier verifier;
    CHIP_ERROR err = verifier.Generate(kIterations, chip::ByteSpan(temporary.salt), temporary.pin);
    std::string manual, qr;
    if (err == CHIP_NO_ERROR) err = codes(temporary, true, manual, qr);
    if (err != CHIP_NO_ERROR) {
        chip::Crypto::ClearSecretData(reinterpret_cast<uint8_t *>(&temporary), sizeof(temporary));
        return ESP_FAIL;
    }
    if (manager.IsCommissioningWindowOpen()) {
        chip::Crypto::ClearSecretData(reinterpret_cast<uint8_t *>(&temporary), sizeof(temporary));
        return ESP_ERR_INVALID_STATE;
    }
    // The SDK requires an existing administrative fabric for an enhanced window.
    // Associate the locally requested window with the first existing fabric;
    // never advertise an invalid fabric index (0), and never change its keys.
    chip::FabricIndex owner_index = chip::kUndefinedFabricIndex;
    chip::VendorId owner_vendor = chip::VendorId::Unspecified;
    for (const auto &fabric : server.GetFabricTable()) {
        owner_index = fabric.GetFabricIndex(); owner_vendor = fabric.GetVendorId(); break;
    }
    if (owner_index == chip::kUndefinedFabricIndex) {
        chip::Crypto::ClearSecretData(reinterpret_cast<uint8_t *>(&temporary), sizeof(temporary));
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI("bwc_commissioning", "Enhanced commissioning window openen");
    err = manager.OpenEnhancedCommissioningWindow(chip::System::Clock::Seconds32(kWindowSeconds),
        temporary.discriminator, verifier, kIterations, chip::ByteSpan(temporary.salt),
        owner_index, owner_vendor);
    if (err == CHIP_NO_ERROR) {
        temporary_manual = manual; temporary_qr = qr;
        temporary_deadline_us = esp_timer_get_time() + int64_t(kWindowSeconds) * 1000000;
    }
    chip::Crypto::ClearSecretData(reinterpret_cast<uint8_t *>(&temporary), sizeof(temporary));
    return err == CHIP_NO_ERROR ? ESP_OK : ESP_FAIL;
}

static void open_window_work(intptr_t)
{
    ESP_LOGI("bwc_commissioning", "Koppelactie gestart op Matter-taak");
    const esp_err_t err = open_window_on_matter_thread();
    window_open_error.store(err);
    window_opening.store(false);
    if (err == ESP_OK) ESP_LOGI("bwc_commissioning", "Koppelvenster geopend");
    else ESP_LOGE("bwc_commissioning", "Koppelvenster mislukt: %s", esp_err_to_name(err));
}

esp_err_t bestway_matter_open_window()
{
    if (!ready) return ESP_ERR_INVALID_STATE;
    bool expected = false;
    if (!window_opening.compare_exchange_strong(expected, true)) return ESP_ERR_INVALID_STATE;
    window_open_error.store(ESP_OK);
    // ScheduleWork is explicitly safe from any thread, without taking the CHIP lock.
    const CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().ScheduleWork(open_window_work, 0);
    if (err != CHIP_NO_ERROR) {
        window_open_error.store(ESP_FAIL);
        window_opening.store(false);
        return ESP_FAIL;
    }
    return ESP_OK;
}
