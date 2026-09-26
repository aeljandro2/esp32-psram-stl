# Contributing

Issues and pull requests are welcome.

1. **Run the host tests** before opening a pull request: `cmake -S tests/host -B build/host && cmake --build build/host && ./build/host/host_tests`.
2. **Add a test** for every behavior you change. If it touches real memory (alignment, both cores, heap integrity), add it to `tests/device` too.
3. **Keep it header-only and dependency-free.** C++17, ESP-IDF and Arduino-ESP32 only.
4. **Document public names** in `docs/api.md`, and user-facing changes in `CHANGELOG.md`.

Benchmarks from real hardware are especially welcome: board, PSRAM type, clock, and the code you ran.
