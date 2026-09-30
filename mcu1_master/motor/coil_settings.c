#include "coil_settings.h"
#include "current_limit.h"
#include <stddef.h>
#include <string.h>
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "hardware/regs/addressmap.h"

#define SETTINGS_MAGIC    0x5A434C4Du    // "ZCLM"
#define SETTINGS_VERSION  1u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    uint16_t limit_ma[DRV_COUNT];
    uint16_t reserved;
    uint32_t check;          // FNV-1a over everything above
} coil_settings_t;

_Static_assert(sizeof(coil_settings_t) <= FLASH_PAGE_SIZE, "settings exceed one page");

static uint32_t fnv1a(const uint8_t *p, uint32_t n) {
    uint32_t h = 2166136261u;
    while (n--) { h ^= *p++; h *= 16777619u; }
    return h;
}

static const coil_settings_t *stored(void) {
    return (const coil_settings_t *)(XIP_BASE + COIL_SETTINGS_OFFSET);
}

static bool valid(const coil_settings_t *s) {
    return s->magic == SETTINGS_MAGIC &&
           s->version == SETTINGS_VERSION &&
           s->count == DRV_COUNT &&
           s->check == fnv1a((const uint8_t *)s, offsetof(coil_settings_t, check));
}

bool coil_settings_load(void) {
    const coil_settings_t *s = stored();
    if (!valid(s)) return false;
    for (uint8_t i = 0; i < DRV_COUNT; i++) {
        uint16_t ma = s->limit_ma[i];
        if (ma > 2420u) ma = 2420u;         // sense full scale
        current_limit_set_ma((drv_id_t)i, ma);
    }
    return true;
}

bool coil_settings_save(void) {
    coil_settings_t s;
    memset(&s, 0, sizeof(s));
    s.magic   = SETTINGS_MAGIC;
    s.version = SETTINGS_VERSION;
    s.count   = DRV_COUNT;
    for (uint8_t i = 0; i < DRV_COUNT; i++) {
        s.limit_ma[i] = current_limit_get_ma((drv_id_t)i);
    }
    s.check = fnv1a((const uint8_t *)&s, offsetof(coil_settings_t, check));

    // Nothing to do if flash already holds these values (saves wear).
    if (memcmp(stored(), &s, sizeof(s)) == 0) return true;

    static uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0xFF, sizeof(page));
    memcpy(page, &s, sizeof(s));

    // Single-core pattern from the SDK flash example: no code may run from
    // flash while it is being erased/programmed, so interrupts are off.
    // Sector erase is 400 ms worst case (W25Q16JV), under the 500 ms
    // watchdog, so feed it first.
    watchdog_update();
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(COIL_SETTINGS_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(COIL_SETTINGS_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
    watchdog_update();

    return memcmp(stored(), &s, sizeof(s)) == 0;
}
