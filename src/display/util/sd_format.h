#pragma once

#include <esp_err.h>

namespace gaggimate::sd {

enum class FormatResult {
    Success,        // freshly mounted + formatted (card was not mounted before)
    MountedSuccess, // wiped and reformatted an already-mounted card
    WipeFailed,     // sector-level write failed
    RemountFailed,  // SD_MMC.begin() failed — card missing, bad contacts, wrong pins, etc.
};

const char *toString(FormatResult r);

// Destructive. Two code paths:
//   currentlyMounted=true  → wipe first 16 sectors, end(), begin(format=true)
//                            to force a fresh FAT. Returns MountedSuccess.
//   currentlyMounted=false → begin(format=true) directly — mounts and formats
//                            a card whose boot-time FS the driver couldn't
//                            read (e.g. exFAT, NTFS, uninitialized). Returns
//                            Success.
// Caller guards for user confirmation + active-brew.
FormatResult formatSDCard(bool currentlyMounted);

} // namespace gaggimate::sd
