# Changelog

## 2.0.0

A rewrite, and a new name (`esp32-psram-stdvector` is now `esp32-psram-stl`; GitHub redirects the old URL).

- Every standard container: vector, deque, list, forward_list, map, multimap, set, multiset, the four unordered containers, and basic_string.
- `psram::allocator<T, Policy>` with two policies: PSRAM only, and PSRAM first with internal RAM fallback (`psram::fallback::`).
- Objects in PSRAM: `make_unique`, `make_shared`, and the `psram::resident` base class.
- Diagnostics: `available`, `info`, `is_psram`, `check_integrity`, `report`, `begin`.
- Out-of-memory handler; `std::bad_alloc` on failure instead of `nullptr`.
- Correct alignment for over-aligned types.
- Arduino library (`PsramStl`), PlatformIO library, and ESP-IDF component from one repository.
- Host tests with sanitizers, device tests and Arduino examples running in QEMU, and CI.

## 1.0.0

- A `PSallocator` for `std::vector`, as a single Arduino sketch.
