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

gaggimate::sd::FormatResult wipeAndRemount() {
    sdmmc_card_t *card = cardHandle();
    if (card == nullptr) {
        ESP_LOGW(LOG_TAG, "wipeAndRemount: no card handle");
        return gaggimate::sd::FormatResult::RemountFailed;
    }

    // Invalidate the filesystem by zeroing the first 16 sectors. Kills the
    // MBR, the FAT boot sector, and the first few FAT entries — enough to
    // make fatfs mount fail so that format_if_mount_failed kicks in.
    constexpr size_t kWipeSectors = 16;
    static uint8_t zeros[512] = {0};
    for (size_t i = 0; i < kWipeSectors; ++i) {
        esp_err_t err = sdmmc_write_sectors(card, zeros, i, 1);
        if (err != ESP_OK) {
            ESP_LOGE(LOG_TAG, "sdmmc_write_sectors(sector=%u) failed: %s", static_cast<unsigned>(i), esp_err_to_name(err));
            return gaggimate::sd::FormatResult::WipeFailed;
        }
    }

    SD_MMC.end();
    if (!SD_MMC.begin(kMountPoint, kMode1Bit, /*format_if_mount_failed=*/true)) {
        ESP_LOGE(LOG_TAG, "SD_MMC.begin() after wipe failed");
        return gaggimate::sd::FormatResult::RemountFailed;
    }

    ESP_LOGI(LOG_TAG, "wipeAndRemount: cardSize=%llu bytes", SD_MMC.cardSize());
    return gaggimate::sd::FormatResult::MountedSuccess;
}

gaggimate::sd::FormatResult mountWithFormat() {
    // SD_MMC.end() is safe to call even when we were never mounted — the
    // internal vfs state is idempotent on unmount. It matters because boot-
    // time begin(false) may have partially init'd the host before failing.
    SD_MMC.end();
    if (!SD_MMC.begin(kMountPoint, kMode1Bit, /*format_if_mount_failed=*/true)) {
        ESP_LOGE(LOG_TAG, "mountWithFormat: SD_MMC.begin() failed — card likely absent or unreadable");
        return gaggimate::sd::FormatResult::RemountFailed;
    }
    ESP_LOGI(LOG_TAG, "mountWithFormat: cardSize=%llu bytes", SD_MMC.cardSize());
    return gaggimate::sd::FormatResult::Success;
}

} // namespace

namespace gaggimate::sd {

const char *toString(FormatResult r) {
    switch (r) {
    case FormatResult::Success:
        return "success";
    case FormatResult::MountedSuccess:
        return "success";
    case FormatResult::WipeFailed:
        return "sector wipe failed";
    case FormatResult::RemountFailed:
        return "remount failed (card not detected)";
    }
    return "unknown";
}

FormatResult formatSDCard(bool currentlyMounted) {
    return currentlyMounted ? wipeAndRemount() : mountWithFormat();
}

} // namespace gaggimate::sd
