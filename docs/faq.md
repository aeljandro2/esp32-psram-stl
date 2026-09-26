# FAQ

**Does `psram::vector` work with my existing code?**
Yes. It is a `std::vector` with a different allocator. Algorithms, range-for, and `.data()` all work. Functions that take `const std::vector<T>&` need an overload or a template, because the allocator is part of the type; prefer spans or iterator pairs in new code.

**Is it slower than a normal vector?**
The allocation is one `heap_caps_malloc` call, like `malloc`. Access speed is the speed of PSRAM, which is slower than internal RAM and reached through a cache. Large, sequential data does well; many tiny, randomly accessed nodes do worst. Measure on your board.

**Why does my short string not show up in PSRAM?**
Strings up to 15 characters live inside the string object (small-string optimization) and never allocate. That is a feature.

**What happens without PSRAM?**
`psram::available()` returns false. `psram::` containers fail on the first allocation; `psram::fallback::` containers quietly use internal RAM.

**Can I use it inside an interrupt handler?**
Do not allocate inside an ISR, with this library or any other. Also keep data that ISRs read while the flash cache is off in internal RAM.

**Why not just set `CONFIG_SPIRAM_USE_MALLOC` and let `malloc` decide?**
That moves only allocations above a size threshold, for the whole program. This library lets you choose per container and per object, including small nodes, and leaves everything else in fast internal RAM.

**Does it work with ArduinoJson, or other libraries that take an allocator?**
Anything that accepts a standard allocator accepts `psram::allocator<T>`. Libraries with their own allocator interface need a small adapter.

**Why can't I convert `psram::unique_ptr<Derived>` to `psram::unique_ptr<Base>`?**
Freeing through a base pointer is only safe when the base sits at the start of the object, which C++ does not guarantee. Derive the base from `psram::resident` and use `std::unique_ptr<Base>`; the virtual destructor then frees the right address.
