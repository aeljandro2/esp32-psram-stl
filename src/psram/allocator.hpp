// esp32-psram-stl: put the C++ standard library in the ESP32's PSRAM.
// SPDX-License-Identifier: MIT
//
// psram::allocator<T, Policy>: a standard allocator backed by the PSRAM heap.

#pragma once

#include <limits>
#include <type_traits>
#include <utility>

#include "config.hpp"

namespace psram {

// ---------------------------------------------------------------------------
// Policies: what happens when PSRAM cannot serve a request.
// ---------------------------------------------------------------------------
namespace policy {

/// PSRAM or nothing. A failed request is an out-of-memory error.
struct only {
    static constexpr bool fallback_to_internal = false;
};

/// PSRAM first; internal RAM when PSRAM is full or absent.
struct prefer {
    static constexpr bool fallback_to_internal = true;
};

}  // namespace policy

// ---------------------------------------------------------------------------
// Out-of-memory hook
// ---------------------------------------------------------------------------
/// Called with the requested size before an out-of-memory error is raised.
/// Use it to log or to record a crash reason. It must not return normally
/// if it wants to replace the default behavior (throw or abort).
using oom_handler = void (*)(std::size_t bytes);

namespace detail {

inline oom_handler& oom_handler_slot() noexcept {
    static oom_handler handler = nullptr;
    return handler;
}

}  // namespace detail

/// Installs a handler for out-of-memory errors and returns the previous one.
inline oom_handler set_oom_handler(oom_handler handler) noexcept {
    oom_handler previous = detail::oom_handler_slot();
    detail::oom_handler_slot() = handler;
    return previous;
}

namespace detail {

constexpr std::uint32_t kPsramCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
constexpr std::uint32_t kInternalCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;

inline void* heap_alloc(std::size_t bytes, std::size_t alignment, std::uint32_t caps) noexcept {
    if (bytes == 0) {
        bytes = 1;  // Every allocation must return a unique, freeable pointer.
    }
    if (alignment > PSRAM_STL_HEAP_ALIGNMENT) {
        return heap_caps_aligned_alloc(alignment, bytes, caps);
    }
    return heap_caps_malloc(bytes, caps);
}

/// Tries PSRAM, then internal RAM if the policy allows it. Never throws.
template <class Policy>
void* try_allocate(std::size_t bytes, std::size_t alignment) noexcept {
    void* p = heap_alloc(bytes, alignment, kPsramCaps);
    if (p == nullptr && Policy::fallback_to_internal) {
        p = heap_alloc(bytes, alignment, kInternalCaps);
    }
    return p;
}

[[noreturn]] inline void out_of_memory(std::size_t bytes) {
    if (oom_handler handler = oom_handler_slot()) {
        handler(bytes);
    }
#if PSRAM_STL_HAS_EXCEPTIONS
    throw std::bad_alloc();
#else
    PSRAM_STL_LOGE("out of memory: %u bytes requested", static_cast<unsigned>(bytes));
    std::abort();
#endif
}

/// Allocates or reports out of memory (throw, or log and abort).
template <class Policy>
void* allocate(std::size_t bytes, std::size_t alignment) {
    void* p = try_allocate<Policy>(bytes, alignment);
    if (p == nullptr) {
        out_of_memory(bytes);
    }
    return p;
}

inline void deallocate(void* p) noexcept {
    // heap_caps_free() releases memory from any heap, aligned or not,
    // so blocks that fell back to internal RAM are freed correctly too.
    heap_caps_free(p);
}

}  // namespace detail

// ---------------------------------------------------------------------------
// The allocator
// ---------------------------------------------------------------------------
/// A stateless standard allocator whose memory comes from the PSRAM heap.
///
/// Works with every allocator-aware standard container, including the
/// node-based ones (map, set, list, unordered_map): they rebind it to their
/// node type, and every instance compares equal, so swap, move and splice
/// behave exactly as with std::allocator.
template <class T, class Policy = policy::only>
class allocator {
   public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using policy_type = Policy;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::false_type;
    using is_always_equal = std::true_type;

    template <class U>
    struct rebind {
        using other = allocator<U, Policy>;
    };

    constexpr allocator() noexcept = default;
    constexpr allocator(const allocator&) noexcept = default;
    template <class U>
    constexpr allocator(const allocator<U, Policy>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > max_size()) {
#if PSRAM_STL_HAS_EXCEPTIONS
            throw std::bad_array_new_length();
#else
            detail::out_of_memory(std::numeric_limits<std::size_t>::max());
#endif
        }
        return static_cast<T*>(detail::allocate<Policy>(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, std::size_t) noexcept { detail::deallocate(p); }

    [[nodiscard]] constexpr std::size_t max_size() const noexcept {
        return std::numeric_limits<std::size_t>::max() / sizeof(T);
    }
};

template <class T, class U, class Policy>
constexpr bool operator==(const allocator<T, Policy>&, const allocator<U, Policy>&) noexcept {
    return true;
}

template <class T, class U, class Policy>
constexpr bool operator!=(const allocator<T, Policy>&, const allocator<U, Policy>&) noexcept {
    return false;
}

}  // namespace psram
