# Guide

How esp32-psram-stl works, and how to get the most out of PSRAM.

## How it works

Every standard container takes an allocator as its last template parameter. By default that is `std::allocator<T>`, which calls `operator new`, which on an ESP32 calls `malloc`, which serves small requests from internal RAM.

`psram::allocator<T>` replaces that chain with one call:

```cpp
heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
```

`heap_caps_malloc` is ESP-IDF's capability-based allocator. Asking for `MALLOC_CAP_SPIRAM` guarantees the block comes from external RAM, whatever `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` says about plain `malloc`. It works whenever PSRAM is part of the heap, in Arduino and in plain ESP-IDF, and does not depend on Arduino's `ps_malloc()` wrapper.

`psram::vector<T>` and friends are aliases:

```cpp
template <class T>
using vector = std::vector<T, psram::allocator<T>>;
```

So a `psram::vector<int>` **is** a `std::vector`. Every algorithm, range-for, and API you know works unchanged.

## Why node-based containers need more than a vector does

A `std::vector<T>` allocates arrays of `T`. A `std::map<K, V>` never allocates a `std::pair<const K, V>`: it allocates tree nodes, an internal type that holds the pair plus three pointers and a color bit. So the map **rebinds** your allocator to its node type, and sometimes compares allocators (on swap, move assignment, and splice) to decide whether memory can be handed over.

`psram::allocator` supports both:

- It is a template over `T`, so `std::allocator_traits` can rebind it to any node type.
- It is stateless and declares `is_always_equal = true_type`, and `operator==` returns `true`. Memory allocated by one instance can always be freed by another.

Version 1.x of this library lacked the equality operators and returned `nullptr` on failure, which is why only `std::vector` was reliable.

## Policies and out-of-memory

| Policy | Names | Behavior |
|---|---|---|
| `psram::policy::only` | `psram::vector`, `psram::map`, ... | PSRAM or nothing |
| `psram::policy::prefer` | `psram::fallback::vector`, ... | PSRAM, then `MALLOC_CAP_INTERNAL` |

When a request cannot be served:

1. Your handler runs, if you installed one with `psram::set_oom_handler`.
2. With exceptions on (Arduino-ESP32, or ESP-IDF with `CONFIG_COMPILER_CXX_EXCEPTIONS=y`), `std::bad_alloc` is thrown. The container is left unchanged, as the standard guarantees.
3. With exceptions off (ESP-IDF default), the size is logged and `abort()` is called, which is what the standard library does on ESP-IDF anyway.

Objects that derive from `psram::resident` also support `new (std::nothrow) T`, which returns `nullptr` instead.

## Strings

`psram::string` is `std::basic_string<char, std::char_traits<char>, psram::allocator<char>>`. With the GCC standard library used on ESP32, strings up to 15 characters are stored inside the string object itself (the small-string optimization). They never touch the heap, PSRAM or otherwise. Longer strings allocate from PSRAM.

`psram::string` and `std::string` are different types. Convert with `.c_str()` or iterators:

```cpp
std::string from_api = ...;
psram::string stored(from_api.begin(), from_api.end());
```

## Objects

Three ways, depending on who owns the object:

```cpp
auto a = psram::make_unique<Sensor>(pin);   // one owner
auto b = psram::make_shared<Model>();       // shared owners; object and counts in one block

class Frame : public psram::resident { ... };
Frame* c = new Frame();                     // every `new Frame` goes to PSRAM
```

`psram::resident` is an empty base class with class-level `operator new` and `operator delete` (plain, array, aligned, and nothrow forms, plus placement new). It adds no bytes to your type. Because it is a base, it composes with hierarchies: `delete` through a base pointer with a virtual destructor calls the right `operator delete` with the right address.

## Alignment

The ESP32 heaps guarantee 4-byte alignment. When `alignof(T)` is larger (for example `alignas(64)` structures or `double` on some targets), the allocator switches to `heap_caps_aligned_alloc`. Aligned and unaligned blocks are both freed with `heap_caps_free`.

If your target guarantees more, raise the threshold before including the library:

```cpp
#define PSRAM_STL_HEAP_ALIGNMENT 8
#include <PsramStl.h>
```

## Fragmentation

PSRAM is a heap like any other. Growth patterns shape it:

- A `vector` that grows by `push_back` roughly doubles its buffer at each step, freeing the old one. Many small vectors doing this at the same time leave many small gaps.
- An `unordered_map` grows its bucket array the same way on rehash.
- A `map`, `set`, or `list` allocates one uniform node per element, which fragments less.

Two habits keep the largest free block large:

1. `reserve()` whenever you know the size, ideally right after construction.
2. Watch `psram::info().largest_block` during development, not just `free`.

The [Diagnostics example](../examples/Diagnostics/Diagnostics.ino) prints both numbers for the same data built with and without `reserve()`. On a freshly booted board the two stay close, because the heap merges freed neighbors; the difference shows up over hours of mixed, long-lived allocations.

## Globals and static storage

A global `psram::vector<int> g;` puts its elements in PSRAM, but the container header (a few pointers) sits in internal `.bss`. On ESP-IDF you can move the header too:

```cpp
EXT_RAM_BSS_ATTR psram::vector<int> g;   // needs CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y
```

On ESP-IDF with the default `CONFIG_SPIRAM_BOOT_INIT=y`, PSRAM joins the heap before global constructors run, so globals can allocate from it in their constructors. In Arduino sketches, allocate in `setup()` or later: depending on the core's configuration, PSRAM may join the heap only after global constructors have run.

## What must stay in internal RAM

- **Code and data used while the flash cache is disabled** (during flash writes, some interrupt handlers marked `IRAM_ATTR`). PSRAM is accessed through the same cache, so it is not readable then.
- **DMA buffers** for peripherals that cannot reach external RAM. Use `heap_caps_malloc(size, MALLOC_CAP_DMA)`.
- **FreeRTOS task stacks** unless you enable external stacks explicitly.

## Multi-core and threads

The ESP-IDF heap takes a lock on every allocation, so two cores can allocate and free PSRAM at the same time. The device tests do exactly that. The containers themselves have the same rules as `std::`: concurrent reads are fine, a write needs exclusive access.

## Performance

PSRAM is slower than internal SRAM and faster than flash. The CPU reaches it through a cache, so sequential access to large buffers performs well and random access to many small nodes performs worst. This library adds no overhead beyond the `heap_caps_malloc` call itself.

Numbers depend on the chip, the PSRAM mode (quad or octal), clock speeds, and cache settings. Benchmarks on real hardware will be added; simulation cannot measure them.
