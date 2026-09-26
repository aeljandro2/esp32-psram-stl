// esp32-psram-stl: put the C++ standard library in the ESP32's PSRAM.
// SPDX-License-Identifier: MIT
//
// Is PSRAM there, how much is left, and does a pointer live in it?

#pragma once

#include "config.hpp"

#if defined(ARDUINO) && __has_include(<Print.h>)
#include <Print.h>
#define PSRAM_STL_HAS_ARDUINO_PRINT 1
#else
#define PSRAM_STL_HAS_ARDUINO_PRINT 0
#endif

namespace psram {

/// A snapshot of the PSRAM heap, in bytes.
struct heap_info {
    std::size_t total;          ///< PSRAM added to the heap at boot.
    std::size_t free;           ///< Free right now.
    std::size_t largest_block;  ///< Largest single allocation possible right now.
    std::size_t minimum_free;   ///< Lowest free value since boot (high-water mark).
};

/// True when the PSRAM heap exists (PSRAM fitted, enabled, and initialized).
inline bool available() noexcept { return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0; }

/// Current PSRAM heap numbers.
inline heap_info info() noexcept {
    return heap_info{
        heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
        heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
    };
}

/// True when `p` points into external RAM. Handy in tests and asserts:
///   assert(psram::is_psram(v.data()));
inline bool is_psram(const void* p) noexcept { return p != nullptr && esp_ptr_external_ram(p); }

/// Walks the PSRAM heap and checks its metadata (and, with heap poisoning
/// enabled in ESP-IDF, the guard bytes around every block). Returns false on
/// corruption; with `print_errors`, the heap logs where it found it.
inline bool check_integrity(bool print_errors = true) noexcept {
    return heap_caps_check_integrity(MALLOC_CAP_SPIRAM, print_errors);
}

/// Prints a one-screen PSRAM report with printf (ESP-IDF style).
inline void report() {
    const heap_info h = info();
    std::printf("PSRAM  total %u  free %u  largest block %u  min free %u  (bytes)\n",
                static_cast<unsigned>(h.total), static_cast<unsigned>(h.free),
                static_cast<unsigned>(h.largest_block), static_cast<unsigned>(h.minimum_free));
}

#if PSRAM_STL_HAS_ARDUINO_PRINT
/// Prints the same report to any Arduino stream: psram::report(Serial);
inline void report(Print& out) {
    const heap_info h = info();
    out.printf("PSRAM  total %u  free %u  largest block %u  min free %u  (bytes)\n",
               static_cast<unsigned>(h.total), static_cast<unsigned>(h.free),
               static_cast<unsigned>(h.largest_block), static_cast<unsigned>(h.minimum_free));
}

/// Arduino-style start: checks PSRAM and, if you pass a stream, prints a
/// report or a hint on how to enable it.
///
///   if (!psram::begin(Serial)) { /* no PSRAM: stop or degrade */ }
inline bool begin(Print& out) {
    if (!available()) {
        out.println("PSRAM not found. Enable it in Tools > PSRAM (Arduino IDE) or "
                    "with board_build.arduino.memory_type / -DBOARD_HAS_PSRAM (PlatformIO).");
        return false;
    }
    report(out);
    return true;
}
#endif

/// Checks PSRAM without printing anything.
inline bool begin() noexcept { return available(); }

}  // namespace psram
