// Host stand-in for ESP-IDF's esp_memory_utils.h.
#pragma once

#include "esp_heap_caps.h"

/// True when the pointer falls inside a live block of the simulated PSRAM.
inline bool esp_ptr_external_ram(const void* p) {
    fakeheap::region r;
    return fakeheap::owner_of(p, &r) && r == fakeheap::region::psram;
}
