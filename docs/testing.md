# Testing

Three layers, all automated in [CI](../.github/workflows/ci.yml).

## 1. Host tests (seconds, no hardware)

The library is compiled for your PC against a simulated ESP32 heap ([`tests/host/fake_heap.cpp`](../tests/host/fake_heap.cpp)). The simulation keeps two regions, internal RAM and PSRAM, with fixed capacities, and wraps every block in 32 guard bytes on each side. It knows who owns every byte, so tests can prove where memory came from.

```bash
cmake -S tests/host -B build/host
cmake --build build/host -j
./build/host/host_tests
```

The build uses AddressSanitizer and UndefinedBehaviorSanitizer, `-Wall -Wextra -Wpedantic -Wconversion -Werror`, on GCC and Clang.

| Test | Proves |
|---|---|
| `allocator_meets_the_standard_contract` | Rebinding, equality, traits, empty types, `unique_ptr` size |
| `size_overflow_is_rejected` | Oversized requests throw instead of wrapping around |
| `vector_elements_live_in_psram` | Elements in PSRAM, no internal RAM used, no leaks |
| `every_standard_container_lives_in_psram` | All eleven node and sequence containers, element by element |
| `strings_use_psram_beyond_the_small_string_buffer` | Short strings do not allocate; long ones go to PSRAM |
| `nested_containers_stay_in_psram` | `map<string, vector<int>>`: every level in PSRAM |
| `swap_move_and_splice_keep_memory_in_psram` | Allocator propagation and equality in practice |
| `map_behaves_exactly_like_std_map` | 50,000 random operations, identical to `std::map` |
| `unordered_map_behaves_exactly_like_std_unordered_map` | 50,000 random operations and rehashes |
| `sequences_behave_exactly_like_std_sequences` | vector, deque, list under 20,000 random inserts and erases |
| `over_aligned_types_get_aligned_memory` | 16- and 64-byte alignment, via the aligned path only when needed |
| `only_policy_raises_bad_alloc_when_psram_is_full` | Throws, calls the handler with the right size, never touches internal RAM |
| `fallback_policy_prefers_psram` | Fallback family still uses PSRAM first |
| `fallback_policy_uses_internal_ram_when_psram_is_full` | And internal RAM when PSRAM is full |
| `no_psram_at_all` | Behavior on a board without PSRAM |
| `make_unique_and_make_shared_live_in_psram` | Objects in PSRAM, destructors run, no leaks |
| `make_unique_frees_memory_if_the_constructor_throws` | Exception safety |
| `resident_types_allocate_from_psram` | Plain, array, aligned, polymorphic, stack, and placement cases |
| `resident_nothrow_new_returns_null_when_full` | nothrow form, and fallback resident |
| `info_tracks_the_psram_heap` | Diagnostics match the heap |
| `heap_guards_are_intact_after_everything` | No write ever crossed a block boundary |

**Do the tests catch bugs?** They were checked by mutation: sending allocations to internal RAM, or ignoring alignment, makes the suite fail.

## 2. Device tests (ESP32 and ESP32-S3, in Wokwi or on hardware)

[`tests/device`](../tests/device) is an ESP-IDF app. It enables exceptions and **comprehensive heap poisoning**, so ESP-IDF fills free memory with a pattern and surrounds every block with guard words. After each test it checks that the PSRAM free size is exactly what it was before and runs `heap_caps_check_integrity_all()`.

| Test | Proves |
|---|---|
| `psram_is_present` | PSRAM is in the heap |
| `one_mebibyte_pattern_survives_other_allocations` | A 1 MB pattern stays intact while 200 other blocks come and go: no overlap |
| `every_container_lives_in_psram` | Element addresses are in external RAM on real memory maps |
| `map_matches_std_map_under_random_operations` | 20,000 random operations on the device |
| `unordered_map_and_vector_match_std_under_random_operations` | Same, for hashing and erasing in the middle |
| `over_aligned_types_are_aligned_in_psram` | `heap_caps_aligned_alloc` on PSRAM |
| `only_policy_throws_and_recovers_cleanly` | Asking for more than all of PSRAM throws and leaks nothing |
| `exhaust_psram_then_fall_back_to_internal_ram` | Fills PSRAM to the last block, checks every block's contents, then the fallback uses internal RAM |
| `objects_live_in_psram` | `make_unique`, `make_shared`, and a polymorphic `resident` hierarchy |
| `both_cores_allocate_concurrently` | Two tasks, one per core, hammering PSRAM maps at once |
| `growth_versus_reserve_fragmentation_report` | Prints free and largest block for both growth patterns; asserts heap integrity |

Build (ESP-IDF 5.x, or the `espressif/idf` Docker image):

```bash
cd tests/device
idf.py -B build/esp32 -D SDKCONFIG=build/esp32/sdkconfig \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32" set-target esp32 build
```

The repository directory must be named `esp32-psram-stl`, because it is the component name.

Run on hardware with `idf.py -B build/esp32 flash monitor`, or in Wokwi:

```bash
export WOKWI_CLI_TOKEN=...   # https://wokwi.com/dashboard/ci
wokwi-cli --timeout 600000 --expect-text "ALL TESTS PASSED" --fail-text "TESTS FAILED" tests/device/wokwi/esp32
```

For ESP32-S3, use `esp32s3` in both places.

## 3. Builds

CI compiles every Arduino example for ESP32 and ESP32-S3 with the latest Arduino-ESP32 core, builds the ESP-IDF example with exceptions off, and runs the Arduino Library Manager linter.

## What is not tested

- **Speed.** Wokwi simulates behavior, not PSRAM timing. Benchmarks need real hardware.
- **ESP32-S2, ESP32-C5, ESP32-P4.** Expected to work; not in the matrix yet.
