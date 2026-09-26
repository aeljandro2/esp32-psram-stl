// esp32-psram-stl: put the C++ standard library in the ESP32's PSRAM.
// SPDX-License-Identifier: MIT
//
// Build configuration, platform headers, and small internal helpers.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

#define PSRAM_STL_VERSION_MAJOR 2
#define PSRAM_STL_VERSION_MINOR 0
#define PSRAM_STL_VERSION_PATCH 0
#define PSRAM_STL_VERSION "2.0.0"

#if __cplusplus < 201703L
#error "esp32-psram-stl needs C++17 or newer (Arduino-ESP32 3.x and ESP-IDF 5.x use it by default)."
#endif

// ---------------------------------------------------------------------------
// Platform
// ---------------------------------------------------------------------------
// PSRAM_STL_HOST is defined by the host test suite, which supplies its own
// esp_heap_caps.h and esp_memory_utils.h that simulate the ESP32 heaps.
#if defined(PSRAM_STL_HOST)
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#elif defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#include <esp_log.h>
#if __has_include(<esp_memory_utils.h>)
#include <esp_memory_utils.h>  // ESP-IDF 5.x
#else
#include <soc/soc_memory_layout.h>  // ESP-IDF 4.x
#endif
#else
#error "esp32-psram-stl targets ESP-IDF or Arduino-ESP32."
#endif

// ---------------------------------------------------------------------------
// Exceptions
// ---------------------------------------------------------------------------
// Arduino-ESP32 builds with exceptions on. ESP-IDF turns them off unless
// CONFIG_COMPILER_CXX_EXCEPTIONS=y. Without exceptions, a failed allocation
// logs the size and calls abort(), which is what the standard library does
// on ESP-IDF anyway.
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
#define PSRAM_STL_HAS_EXCEPTIONS 1
#else
#define PSRAM_STL_HAS_EXCEPTIONS 0
#endif

// The ESP32 heaps guarantee 4-byte alignment. Anything stricter goes through
// heap_caps_aligned_alloc(). Override if your target guarantees more.
#ifndef PSRAM_STL_HEAP_ALIGNMENT
#define PSRAM_STL_HEAP_ALIGNMENT 4
#endif

#ifndef PSRAM_STL_LOG_TAG
#define PSRAM_STL_LOG_TAG "psram-stl"
#endif

#if defined(PSRAM_STL_HOST)
#define PSRAM_STL_LOGE(fmt, ...) std::fprintf(stderr, "[" PSRAM_STL_LOG_TAG "] " fmt "\n", ##__VA_ARGS__)
#else
#define PSRAM_STL_LOGE(fmt, ...) ESP_LOGE(PSRAM_STL_LOG_TAG, fmt, ##__VA_ARGS__)
#endif
