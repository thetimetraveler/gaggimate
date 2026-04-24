#include "sd_format.h"

#include <SD_MMC.h>
#include <esp_log.h>
#include <sdmmc_cmd.h>

namespace {

// Mirrors the actual begin() used by our panel drivers. Keep in sync if the
// driver args change.
constexpr const char *kMountPoint = "/sdcard";
constexpr bool kMode1Bit = true;

// Subclass-access hack to reach SDMMCFS's protected _card handle. SDMMCFS has
// no public accessor, but _card is protected — so a same-shaped subclass can
// read it. We never instantiate this; we reinterpret the global SD_MMC.
struct CardAccess : public fs::SDMMCFS {
    sdmmc_card_t *handle() const { return _card; }
};

sdmmc_card_t *cardHandle() {
    return static_cast<const CardAccess *>(&SD_MMC)->handle();
}

constexpr const char *LOG_TAG = "sd_format";

} // namespace

namespace gaggimate::sd {

const char *toString(FormatResult r) {
    switch (r) {
    case FormatResult::Success:
        return "success";
    case FormatResult::NoCardMounted:
        return "no card mounted";
    case FormatResult::WipeFailed:
        return "sector wipe failed";
    case FormatResult::RemountFailed:
        return "remount failed";
    }
    return "unknown";
}

FormatResult formatSDCard() {
    sdmmc_card_t *card = cardHandle();
    if (card == nullptr) {
        ESP_LOGW(LOG_TAG, "No SD card handle — begin() never succeeded");
        return FormatResult::NoCardMounted;
    }

    // Invalidate the filesystem by zeroing the first 16 sectors. This kills
    // the MBR, the FAT boot sector, and the first few FAT entries — enough to
    // make the IDF's fatfs mount fail so that format_if_mount_failed kicks in.
    // 16 × 512 = 8 KiB, negligible wear.
    constexpr size_t kWipeSectors = 16;
    static uint8_t zeros[512] = {0};
    for (size_t i = 0; i < kWipeSectors; ++i) {
        esp_err_t err = sdmmc_write_sectors(card, zeros, i, 1);
        if (err != ESP_OK) {
            ESP_LOGE(LOG_TAG, "sdmmc_write_sectors(sector=%u) failed: %s", static_cast<unsigned>(i), esp_err_to_name(err));
            return FormatResult::WipeFailed;
        }
    }

    // Tear down the Arduino wrapper so its vfs registration releases, then
    // re-mount with auto-format. SD_MMC.end() deinits the fatfs layer; the
    // subsequent begin() runs the full mount path including format.
    SD_MMC.end();
    if (!SD_MMC.begin(kMountPoint, kMode1Bit, /*format_if_mount_failed=*/true)) {
        ESP_LOGE(LOG_TAG, "SD_MMC.begin() after wipe failed");
        return FormatResult::RemountFailed;
    }

    ESP_LOGI(LOG_TAG, "SD card formatted: cardSize=%llu bytes", SD_MMC.cardSize());
    return FormatResult::Success;
}

} // namespace gaggimate::sd
