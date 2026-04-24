#pragma once

#include <esp_err.h>

namespace gaggimate::sd {

enum class FormatResult {
    Success,
    NoCardMounted,
    WipeFailed,
    RemountFailed,
};

const char *toString(FormatResult r);

// Destructive. Wipes the first N sectors to invalidate the filesystem, then
// re-mounts SD_MMC with format_if_mount_failed=true so the IDF stack lays down
// a fresh FAT partition. Caller is responsible for gating the call (card
// detected, no active brew, user confirmation).
FormatResult formatSDCard();

} // namespace gaggimate::sd
