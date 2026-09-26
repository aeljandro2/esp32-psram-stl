// Simulated ESP32 heaps for the host tests. See shim/esp_heap_caps.h.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <map>

#include "esp_heap_caps.h"

namespace fakeheap {
namespace {

constexpr std::size_t kGuard = 32;            // canary bytes before and after each block
constexpr unsigned char kCanaryFront = 0xA5;
constexpr unsigned char kCanaryBack = 0x5A;
constexpr std::size_t kDefaultInternal = 320 * 1024;
constexpr std::size_t kDefaultPsram = 4 * 1024 * 1024;

struct block {
    void* raw;          // what malloc returned
    std::size_t size;   // bytes the caller asked for
    region where;
};

struct heap_state {
    std::size_t capacity = 0;
    std::size_t used = 0;
    std::size_t minimum_free = 0;
    counters c;
};

// Ordered by address so lookups can find the block that contains a pointer.
std::map<const unsigned char*, block>& blocks() {
    static std::map<const unsigned char*, block> b;
    return b;
}

heap_state& state(region r) {
    static heap_state internal{kDefaultInternal, 0, kDefaultInternal, {}};
    static heap_state psram{kDefaultPsram, 0, kDefaultPsram, {}};
    return r == region::psram ? psram : internal;
}

bool pick_region(std::uint32_t caps, region* out) {
    if (caps & MALLOC_CAP_SPIRAM) {
        *out = region::psram;
        return true;
    }
    if (caps & (MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT)) {
        *out = region::internal;
        return true;
    }
    return false;
}

void* allocate(std::size_t alignment, std::size_t size, std::uint32_t caps, bool aligned_call) {
    region r;
    if (!pick_region(caps, &r)) {
        return nullptr;
    }
    heap_state& h = state(r);
    if (size == 0 || h.used + size > h.capacity) {
        ++h.c.failed;
        return nullptr;
    }
    alignment = std::max<std::size_t>(alignment, 4);
    unsigned char* raw = static_cast<unsigned char*>(std::malloc(size + 2 * kGuard + alignment));
    if (raw == nullptr) {
        ++h.c.failed;
        return nullptr;
    }
    auto start = reinterpret_cast<std::uintptr_t>(raw + kGuard);
    start = (start + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
    auto* user = reinterpret_cast<unsigned char*>(start);
    std::memset(user - kGuard, kCanaryFront, kGuard);
    std::memset(user + size, kCanaryBack, kGuard);
    std::memset(user, 0xCD, size);  // garbage, like a real heap

    blocks()[user] = block{raw, size, r};
    h.used += size;
    h.minimum_free = std::min(h.minimum_free, h.capacity - h.used);
    ++h.c.allocations;
    if (aligned_call) {
        ++h.c.aligned_allocations;
    }
    ++h.c.live_blocks;
    h.c.live_bytes += size;
    return user;
}

bool guards_intact(const unsigned char* user, const block& b) {
    for (std::size_t i = 0; i < kGuard; ++i) {
        if (user[-1 - static_cast<std::ptrdiff_t>(i)] != kCanaryFront || user[b.size + i] != kCanaryBack) {
            return false;
        }
    }
    return true;
}

}  // namespace

void set_capacity(region r, std::size_t bytes) {
    heap_state& h = state(r);
    h.capacity = bytes;
    h.minimum_free = bytes > h.used ? bytes - h.used : 0;
}

void reset_capacities() {
    set_capacity(region::internal, kDefaultInternal);
    set_capacity(region::psram, kDefaultPsram);
}

const counters& stats(region r) { return state(r).c; }

void reset_stats() {
    for (region r : {region::internal, region::psram}) {
        counters& c = state(r).c;
        const std::size_t live_blocks = c.live_blocks;
        const std::size_t live_bytes = c.live_bytes;
        c = counters{};
        c.live_blocks = live_blocks;
        c.live_bytes = live_bytes;
    }
}

bool owner_of(const void* p, region* out) {
    const auto* q = static_cast<const unsigned char*>(p);
    auto& b = blocks();
    auto it = b.upper_bound(q);
    if (it == b.begin()) {
        return false;
    }
    --it;
    if (q >= it->first && q < it->first + it->second.size) {
        *out = it->second.where;
        return true;
    }
    return false;
}

}  // namespace fakeheap

using namespace fakeheap;

void* heap_caps_malloc(std::size_t size, std::uint32_t caps) { return allocate(4, size, caps, false); }

void* heap_caps_aligned_alloc(std::size_t alignment, std::size_t size, std::uint32_t caps) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return nullptr;  // ESP-IDF requires a power of two
    }
    return allocate(alignment, size, caps, true);
}

void heap_caps_free(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    auto& b = blocks();
    auto it = b.find(static_cast<unsigned char*>(ptr));
    if (it == b.end()) {
        std::fprintf(stderr, "fakeheap: free of an unknown pointer %p (double free or not from the heap)\n", ptr);
        std::abort();
    }
    if (!guards_intact(it->first, it->second)) {
        std::fprintf(stderr, "fakeheap: heap corruption around %p (%zu bytes)\n", ptr, it->second.size);
        std::abort();
    }
    heap_state& h = state(it->second.where);
    h.used -= it->second.size;
    ++h.c.frees;
    --h.c.live_blocks;
    h.c.live_bytes -= it->second.size;
    std::free(it->second.raw);
    b.erase(it);
}

std::size_t heap_caps_get_total_size(std::uint32_t caps) {
    region r;
    return pick_region(caps, &r) ? state(r).capacity : 0;
}

std::size_t heap_caps_get_free_size(std::uint32_t caps) {
    region r;
    return pick_region(caps, &r) ? state(r).capacity - state(r).used : 0;
}

std::size_t heap_caps_get_largest_free_block(std::uint32_t caps) { return heap_caps_get_free_size(caps); }

std::size_t heap_caps_get_minimum_free_size(std::uint32_t caps) {
    region r;
    return pick_region(caps, &r) ? state(r).minimum_free : 0;
}

bool heap_caps_check_integrity(std::uint32_t caps, bool print_errors) {
    region wanted;
    if (!pick_region(caps, &wanted)) {
        return true;
    }
    bool ok = true;
    for (const auto& [user, b] : blocks()) {
        if (b.where == wanted && !guards_intact(user, b)) {
            ok = false;
            if (print_errors) {
                std::fprintf(stderr, "fakeheap: corrupted guard around %p\n", static_cast<const void*>(user));
            }
        }
    }
    return ok;
}
