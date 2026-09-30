#ifndef COIL_SETTINGS_H
#define COIL_SETTINGS_H

// =============================================================================
// mcu1_master/motor/coil_settings.h — per-coil amp settings kept in flash
//
// Stored in the last 4 KB sector of the physical flash. The board file
// (weact_studio_rp2350b_core) says 16 MB, but this board fits a W25Q16JV
// (U1), which is 2 MB, so the offset is set from the real chip:
//   2 MB - 4 KB = 0x1FF000
// The firmware image is far smaller, so this sector is never code.
// =============================================================================

#include <stdbool.h>
#include <stdint.h>
#include "drv8873.h"

#define COIL_SETTINGS_FLASH_BYTES   (2u * 1024u * 1024u)    // W25Q16JV
#define COIL_SETTINGS_OFFSET        (COIL_SETTINGS_FLASH_BYTES - 4096u)

// Load saved amp settings into the current limiter. Keeps the defaults if
// nothing valid is stored. Returns true if saved values were loaded.
bool coil_settings_load(void);

// Save the limiter's current settings. Skips the write if nothing changed.
// Returns true if flash holds exactly these values afterwards.
bool coil_settings_save(void);

#endif // COIL_SETTINGS_H
