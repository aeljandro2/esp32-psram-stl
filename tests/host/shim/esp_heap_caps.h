// Host stand-in for ESP-IDF's esp_heap_caps.h.
//
// Simulates two heaps (internal RAM and PSRAM) with fixed capacities. Every
// block is tracked and wrapped in canary guards, so the tests can prove
// where memory came from, that nothing leaks, and that nothing writes out of
// bounds. Same function names and semantics as ESP-IDF.

#pragma once

#include <cstddef>
#include <cstdint>

#define MALLOC_CAP_8BIT (1u << 2)
#define MALLOC_CAP_SPIRAM (1u << 10)
#define MALLOC_CAP_INTERNAL (1u << 11)
#define MALLOC_CAP_DEFAULT (1u << 12)

void* heap_caps_malloc(std::size_t size, std::uint32_t caps);
void* heap_caps_aligned_alloc(std::size_t alignment, std::size_t size, std::uint32_t caps);
void heap_caps_free(void* ptr);
std::size_t heap_caps_get_total_size(std::uint32_t caps);
std::size_t heap_caps_get_free_size(std::uint32_t caps);
std::size_t heap_caps_get_largest_free_block(std::uint32_t caps);
std::size_t heap_caps_get_minimum_free_size(std::uint32_t caps);
bool heap_caps_check_integrity(std::uint32_t caps, bool print_errors);

namespace fakeheap {

enum class region { internal, psram };

struct counters {
    std::size_t allocations = 0;          // successful allocations
    std::size_t aligned_allocations = 0;  // through heap_caps_aligned_alloc
    std::size_t frees = 0;
    std::size_t failed = 0;               // requests that returned nullptr
    std::size_t live_blocks = 0;
    std::size_t live_bytes = 0;
};

/// Sets how many bytes a region can hand out. 0 disables it (no PSRAM).
void set_capacity(region r, std::size_t bytes);
/// Restores the default capacities (internal 320 KiB, PSRAM 4 MiB).
void reset_capacities();
const counters& stats(region r);
/// Zeroes the counters (not the live blocks).
void reset_stats();
/// Region that owns a live pointer, or false if it is not a live block.
bool owner_of(const void* p, region* out);

}  // namespace fakeheap
