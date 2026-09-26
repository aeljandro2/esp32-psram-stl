# API reference

Everything lives in namespace `psram`. One include: `#include <PsramStl.h>`.

## Allocator

```cpp
namespace psram {
namespace policy {
struct only;    // PSRAM or nothing (default)
struct prefer;  // PSRAM, then internal RAM
}

template <class T, class Policy = policy::only>
class allocator;
}
```

A stateless standard allocator. `allocate(n)` returns memory for `n` objects of `T` from PSRAM (or internal RAM as a fallback under `policy::prefer`), aligned to `alignof(T)`. `deallocate(p, n)` frees it. All instances with the same policy compare equal, and `allocator_traits` can rebind it to any type.

| Member | Value |
|---|---|
| `value_type` | `T` |
| `policy_type` | `Policy` |
| `is_always_equal` | `std::true_type` |
| `propagate_on_container_move_assignment` | `std::true_type` |
| `max_size()` | `SIZE_MAX / sizeof(T)` |

Requests larger than `max_size()` throw `std::bad_array_new_length`. Failed requests throw `std::bad_alloc` (see [Out of memory](#out-of-memory)).

## Containers

| Alias | Standard type |
|---|---|
| `psram::vector<T>` | `std::vector<T, allocator<T>>` |
| `psram::deque<T>` | `std::deque<T, allocator<T>>` |
| `psram::list<T>` | `std::list<T, allocator<T>>` |
| `psram::forward_list<T>` | `std::forward_list<T, allocator<T>>` |
| `psram::map<K, V, Compare = std::less<K>>` | `std::map<K, V, Compare, allocator<std::pair<const K, V>>>` |
| `psram::multimap<K, V, Compare>` | `std::multimap<...>` |
| `psram::set<K, Compare = std::less<K>>` | `std::set<K, Compare, allocator<K>>` |
| `psram::multiset<K, Compare>` | `std::multiset<...>` |
| `psram::unordered_map<K, V, Hash = std::hash<K>, Equal = std::equal_to<K>>` | `std::unordered_map<K, V, Hash, Equal, allocator<std::pair<const K, V>>>` |
| `psram::unordered_multimap<K, V, Hash, Equal>` | `std::unordered_multimap<...>` |
| `psram::unordered_set<K, Hash, Equal>` | `std::unordered_set<...>` |
| `psram::unordered_multiset<K, Hash, Equal>` | `std::unordered_multiset<...>` |
| `psram::basic_string<CharT, Traits = std::char_traits<CharT>>` | `std::basic_string<CharT, Traits, allocator<CharT>>` |
| `psram::string` | `psram::basic_string<char>` |

`psram::fallback::` has the same names with `policy::prefer`.

## Objects

```cpp
template <class T> struct deleter;                    // destroys and frees
template <class T> using unique_ptr = std::unique_ptr<T, deleter<T>>;

template <class T, class... Args> unique_ptr<T>     make_unique(Args&&...);
template <class T, class... Args> std::shared_ptr<T> make_shared(Args&&...);

template <class Policy> struct basic_resident;         // base class with class-level new/delete
using resident = basic_resident<policy::only>;

namespace fallback {
template <class T, class... Args> unique_ptr<T>     make_unique(Args&&...);
template <class T, class... Args> std::shared_ptr<T> make_shared(Args&&...);
using resident = basic_resident<policy::prefer>;
}
```

- `make_unique` frees the memory if `T`'s constructor throws.
- `unique_ptr<T>` is the size of a raw pointer. It does not convert to `unique_ptr<Base>`: for hierarchies, use `resident` and `std::unique_ptr<Base>`.
- `make_unique` does not support arrays; use `psram::vector<T>`.
- `resident` provides plain, array, aligned (`std::align_val_t`), nothrow, and placement `operator new`, with matching `operator delete`. It is empty and adds no bytes.

## Out of memory

```cpp
using oom_handler = void (*)(std::size_t bytes);
oom_handler set_oom_handler(oom_handler handler);   // returns the previous handler
```

The handler runs before the error is raised. Then, with exceptions on, `std::bad_alloc` is thrown; with exceptions off, the size is logged and `abort()` is called.

## Diagnostics

```cpp
struct heap_info {
    std::size_t total;          // PSRAM in the heap
    std::size_t free;           // free now
    std::size_t largest_block;  // largest single allocation possible now
    std::size_t minimum_free;   // lowest free since boot
};

bool      available();                          // PSRAM heap exists
heap_info info();
bool      is_psram(const void* p);              // p points into external RAM
bool      check_integrity(bool print_errors = true);
void      report();                             // printf one-line summary
bool      begin();                              // same as available()

// Arduino only:
void      report(Print& out);                   // psram::report(Serial)
bool      begin(Print& out);                    // report, or a hint on how to enable PSRAM
```

## Configuration macros

| Macro | Default | Meaning |
|---|---|---|
| `PSRAM_STL_HEAP_ALIGNMENT` | `4` | Alignment the heap guarantees; stricter types use `heap_caps_aligned_alloc` |
| `PSRAM_STL_LOG_TAG` | `"psram-stl"` | ESP-IDF log tag for out-of-memory messages |
| `PSRAM_STL_VERSION` | `"2.0.0"` | Also `_MAJOR`, `_MINOR`, `_PATCH` |
