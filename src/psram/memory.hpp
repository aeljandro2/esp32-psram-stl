// esp32-psram-stl: put the C++ standard library in the ESP32's PSRAM.
// SPDX-License-Identifier: MIT
//
// Objects in PSRAM: smart pointers and a base class for your own types.
//
//   auto sensor = psram::make_unique<Sensor>(pin);     // std::unique_ptr in PSRAM
//   auto model  = psram::make_shared<Model>();         // object and control block in PSRAM
//   class Frame : public psram::resident { ... };      // `new Frame` lands in PSRAM

#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "allocator.hpp"

namespace psram {

// ---------------------------------------------------------------------------
// unique_ptr
// ---------------------------------------------------------------------------
/// Destroys the object and returns its memory to the heap it came from.
template <class T>
struct deleter {
    // No converting constructor on purpose: freeing a base-class pointer is
    // only safe when the base sits at offset zero, which C++ does not
    // guarantee. For class hierarchies, derive from psram::resident and use
    // std::unique_ptr<Base>: the virtual destructor then frees the right address.
    constexpr deleter() noexcept = default;

    void operator()(T* p) const noexcept {
        static_assert(sizeof(T) > 0, "cannot delete an incomplete type");
        if (p != nullptr) {
            p->~T();
            detail::deallocate(const_cast<std::remove_cv_t<T>*>(p));
        }
    }
};

/// A std::unique_ptr whose object lives in PSRAM (or internal RAM, for the
/// fallback family). Same size as a raw pointer.
template <class T>
using unique_ptr = std::unique_ptr<T, deleter<T>>;

namespace detail {

template <class T, class Policy, class... Args>
unique_ptr<T> make_unique_with(Args&&... args) {
    static_assert(!std::is_array_v<T>, "use psram::vector<T> for arrays");
    void* memory = detail::allocate<Policy>(sizeof(T), alignof(T));
#if PSRAM_STL_HAS_EXCEPTIONS
    try {
        return unique_ptr<T>(::new (memory) T(std::forward<Args>(args)...));
    } catch (...) {
        detail::deallocate(memory);
        throw;
    }
#else
    return unique_ptr<T>(::new (memory) T(std::forward<Args>(args)...));
#endif
}

}  // namespace detail

/// Constructs a T in PSRAM and returns a psram::unique_ptr<T> that owns it.
template <class T, class... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return detail::make_unique_with<T, policy::only>(std::forward<Args>(args)...);
}

/// Constructs a T in PSRAM and returns a std::shared_ptr<T>. The object and
/// its reference counts share one PSRAM allocation.
template <class T, class... Args>
std::shared_ptr<T> make_shared(Args&&... args) {
    return std::allocate_shared<T>(allocator<T, policy::only>(), std::forward<Args>(args)...);
}

// ---------------------------------------------------------------------------
// resident: a base class that sends `new YourType` to PSRAM
// ---------------------------------------------------------------------------
/// Inherit from psram::resident and every `new YourType(...)` expression,
/// including arrays and over-aligned types, allocates from PSRAM. The base
/// is empty, so it adds no bytes to your type.
///
/// Objects on the stack or inside other objects are not affected: they live
/// wherever their owner lives.
template <class Policy>
struct basic_resident {
    static void* operator new(std::size_t bytes) {
        return detail::allocate<Policy>(bytes, PSRAM_STL_HEAP_ALIGNMENT);
    }
    static void* operator new[](std::size_t bytes) {
        return detail::allocate<Policy>(bytes, PSRAM_STL_HEAP_ALIGNMENT);
    }
    static void* operator new(std::size_t bytes, std::align_val_t alignment) {
        return detail::allocate<Policy>(bytes, static_cast<std::size_t>(alignment));
    }
    static void* operator new[](std::size_t bytes, std::align_val_t alignment) {
        return detail::allocate<Policy>(bytes, static_cast<std::size_t>(alignment));
    }

    // `new (std::nothrow) YourType` returns nullptr instead of failing hard.
    static void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
        return detail::try_allocate<Policy>(bytes, PSRAM_STL_HEAP_ALIGNMENT);
    }
    static void* operator new[](std::size_t bytes, const std::nothrow_t&) noexcept {
        return detail::try_allocate<Policy>(bytes, PSRAM_STL_HEAP_ALIGNMENT);
    }

    // Placement new stays available: `new (buffer) YourType`.
    static void* operator new(std::size_t, void* where) noexcept { return where; }

    static void operator delete(void* p) noexcept { detail::deallocate(p); }
    static void operator delete[](void* p) noexcept { detail::deallocate(p); }
    static void operator delete(void* p, std::align_val_t) noexcept { detail::deallocate(p); }
    static void operator delete[](void* p, std::align_val_t) noexcept { detail::deallocate(p); }
    static void operator delete(void* p, const std::nothrow_t&) noexcept { detail::deallocate(p); }
    static void operator delete[](void* p, const std::nothrow_t&) noexcept { detail::deallocate(p); }
    static void operator delete(void*, void*) noexcept {}

   protected:
    basic_resident() = default;
    ~basic_resident() = default;
};

using resident = basic_resident<policy::only>;

// ---------------------------------------------------------------------------
// Fallback family: PSRAM first, internal RAM when PSRAM is full
// ---------------------------------------------------------------------------
namespace fallback {

template <class T, class... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return detail::make_unique_with<T, policy::prefer>(std::forward<Args>(args)...);
}

template <class T, class... Args>
std::shared_ptr<T> make_shared(Args&&... args) {
    return std::allocate_shared<T>(allocator<T, policy::prefer>(), std::forward<Args>(args)...);
}

using resident = basic_resident<policy::prefer>;

}  // namespace fallback

}  // namespace psram
