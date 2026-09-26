// On-device test suite for esp32-psram-stl (ESP-IDF, runs on hardware or in Wokwi).
//
// Every test also checks, afterwards, that the PSRAM free size is exactly what
// it was before (no leaks) and that every heap passes an integrity walk. The
// build enables comprehensive heap poisoning, so the walk also verifies the
// guard bytes around every block: an out-of-bounds write anywhere fails it.
//
// Output ends with "ALL TESTS PASSED" or "TESTS FAILED".

#include <PsramStl.h>

#include <cinttypes>
#include <cstdio>
#include <map>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace {

int g_passed = 0;
int g_failed = 0;
int g_errors_in_test = 0;

#define EXPECT(cond)                                                                   \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            ++g_errors_in_test;                                                        \
            std::printf("    expectation failed at line %d: %s\n", __LINE__, #cond);   \
        }                                                                              \
    } while (0)

void run(const char* name, void (*body)()) {
    g_errors_in_test = 0;
    const std::size_t free_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    body();
    const std::size_t free_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (free_after != free_before) {
        ++g_errors_in_test;
        std::printf("    PSRAM leak: %u bytes free before, %u after\n", static_cast<unsigned>(free_before),
                    static_cast<unsigned>(free_after));
    }
    if (!heap_caps_check_integrity_all(true)) {
        ++g_errors_in_test;
        std::printf("    heap integrity check failed\n");
    }
    const bool ok = g_errors_in_test == 0;
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    (ok ? g_passed : g_failed)++;
}

template <class Container>
bool every_element_in_psram(const Container& c) {
    for (const auto& e : c) {
        if (!psram::is_psram(&e)) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------

void psram_is_present() {
    EXPECT(psram::available());
    const psram::heap_info h = psram::info();
    EXPECT(h.total >= 2u * 1024 * 1024);
    psram::report();
}

void one_mebibyte_pattern_survives_other_allocations() {
    psram::vector<std::uint32_t> big(256 * 1024);
    EXPECT(psram::is_psram(big.data()));
    for (std::size_t i = 0; i < big.size(); ++i) big[i] = static_cast<std::uint32_t>(i * 2654435761u);

    // Allocate and free many other blocks around it: none may overlap the pattern.
    {
        psram::vector<psram::vector<std::uint8_t>> others;
        for (int i = 0; i < 200; ++i) others.emplace_back(512 + i * 7, static_cast<std::uint8_t>(i));
        for (int i = 0; i < 200; ++i) {
            for (std::uint8_t b : others[i]) {
                if (b != static_cast<std::uint8_t>(i)) {
                    EXPECT(false);
                    break;
                }
            }
        }
    }
    std::size_t bad = 0;
    for (std::size_t i = 0; i < big.size(); ++i) bad += big[i] != static_cast<std::uint32_t>(i * 2654435761u);
    EXPECT(bad == 0);
}

void every_container_lives_in_psram() {
    psram::deque<int> dq;
    psram::list<int> li;
    psram::forward_list<int> fl;
    psram::map<int, int> mp;
    psram::multimap<int, int> mm;
    psram::set<int> st;
    psram::unordered_map<int, int> um;
    psram::unordered_set<int> us;
    psram::string text(300, 'p');
    for (int i = 0; i < 1000; ++i) {
        dq.push_back(i);
        li.push_back(i);
        fl.push_front(i);
        mp[i] = i;
        mm.emplace(i % 11, i);
        st.insert(i);
        um[i] = i;
        us.insert(i);
    }
    EXPECT(every_element_in_psram(dq));
    EXPECT(every_element_in_psram(li));
    EXPECT(every_element_in_psram(fl));
    EXPECT(every_element_in_psram(mp));
    EXPECT(every_element_in_psram(mm));
    EXPECT(every_element_in_psram(st));
    EXPECT(every_element_in_psram(um));
    EXPECT(every_element_in_psram(us));
    EXPECT(psram::is_psram(text.data()));
}

void map_matches_std_map_under_random_operations() {
    std::mt19937 rng(2026);
    psram::map<int, int> ours;
    std::map<int, int> ref;
    for (int step = 0; step < 20000; ++step) {
        const int key = static_cast<int>(rng() % 1500);
        if (rng() % 3 == 0) {
            ours.erase(key);
            ref.erase(key);
        } else {
            ours[key] = step;
            ref[key] = step;
        }
    }
    EXPECT(ours.size() == ref.size());
    EXPECT(std::equal(ours.begin(), ours.end(), ref.begin(), ref.end()));
}

void unordered_map_and_vector_match_std_under_random_operations() {
    std::mt19937 rng(42);
    psram::unordered_map<std::uint32_t, std::uint32_t> ours;
    std::unordered_map<std::uint32_t, std::uint32_t> ref;
    psram::vector<std::uint32_t> v;
    std::vector<std::uint32_t> rv;
    for (std::uint32_t step = 0; step < 20000; ++step) {
        const std::uint32_t key = rng() % 2000;
        ours[key] ^= step;
        ref[key] ^= step;
        if (rng() % 4 == 0 && !rv.empty()) {
            const std::size_t at = rng() % rv.size();
            v.erase(v.begin() + at);
            rv.erase(rv.begin() + at);
        } else {
            v.push_back(step);
            rv.push_back(step);
        }
    }
    EXPECT(ours.size() == ref.size());
    for (const auto& kv : ref) {
        auto it = ours.find(kv.first);
        EXPECT(it != ours.end() && it->second == kv.second);
    }
    EXPECT(std::equal(v.begin(), v.end(), rv.begin(), rv.end()));
}

void over_aligned_types_are_aligned_in_psram() {
    struct alignas(64) line {
        std::uint8_t bytes[64];
    };
    psram::vector<line> lines(100);
    EXPECT(reinterpret_cast<std::uintptr_t>(lines.data()) % 64 == 0);
    EXPECT(psram::is_psram(lines.data()));
    for (auto& l : lines) l.bytes[63] = 0x7E;
    EXPECT(lines[99].bytes[63] == 0x7E);
}

void only_policy_throws_and_recovers_cleanly() {
    const std::size_t free_before = psram::info().free;
    bool caught = false;
    try {
        psram::vector<std::uint8_t> v;
        v.reserve(psram::info().total + 1);
    } catch (const std::bad_alloc&) {
        caught = true;
    }
    EXPECT(caught);
    EXPECT(psram::info().free == free_before);
}

void exhaust_psram_then_fall_back_to_internal_ram() {
    std::vector<psram::vector<std::uint8_t>> blocks;  // outer vector in internal RAM on purpose
    blocks.reserve(512);
    std::size_t sizes[] = {64 * 1024, 4 * 1024, 256};
    for (std::size_t size : sizes) {
        for (;;) {
            try {
                blocks.emplace_back(size, static_cast<std::uint8_t>(blocks.size()));
            } catch (const std::bad_alloc&) {
                break;
            }
            if (blocks.size() == blocks.capacity()) break;
        }
    }
    std::printf("    filled PSRAM with %u blocks, %u bytes left\n", static_cast<unsigned>(blocks.size()),
                static_cast<unsigned>(psram::info().free));

    // Every block still holds its own byte: no two allocations overlap.
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const std::uint8_t expected = static_cast<std::uint8_t>(i);
        for (std::uint8_t b : blocks[i]) {
            if (b != expected) {
                EXPECT(false);
                break;
            }
        }
    }

    if (psram::info().largest_block < 4000) {
        psram::fallback::vector<int> soft(1000, 5);
        EXPECT(!psram::is_psram(soft.data()));
        EXPECT(soft[999] == 5);
    }
    blocks.clear();
    blocks.shrink_to_fit();
}

struct counted {
    static int alive;
    int value;
    explicit counted(int v) : value(v) { ++alive; }
    ~counted() { --alive; }
};
int counted::alive = 0;

struct shape : psram::resident {
    virtual ~shape() = default;
    virtual int sides() const = 0;
};
struct hexagon : shape {
    int sides() const override { return 6; }
    float cache[32] = {};
};

void objects_live_in_psram() {
    {
        auto u = psram::make_unique<counted>(1);
        auto s = psram::make_shared<counted>(2);
        auto s2 = s;
        EXPECT(psram::is_psram(u.get()));
        EXPECT(psram::is_psram(s.get()));
        EXPECT(counted::alive == 2);
        std::unique_ptr<shape> hex(new hexagon());
        EXPECT(psram::is_psram(hex.get()));
        EXPECT(hex->sides() == 6);
    }
    EXPECT(counted::alive == 0);
}

// ---- both cores at once ---------------------------------------------------

struct worker_args {
    std::uint32_t seed;
    bool ok;
    SemaphoreHandle_t done;
};

// Runs in its own function so every container is destroyed before the task ends.
bool hammer_a_map(std::uint32_t seed) {
    std::mt19937 rng(seed);
    psram::map<int, psram::vector<int>> ours;
    std::map<int, std::vector<int>> ref;
    for (int step = 0; step < 6000; ++step) {
        const int key = static_cast<int>(rng() % 400);
        const int value = static_cast<int>(rng());
        if (rng() % 5 == 0) {
            ours.erase(key);
            ref.erase(key);
        } else {
            ours[key].push_back(value);
            ref[key].push_back(value);
        }
        if (step % 500 == 0) vTaskDelay(1);  // let the other core's task interleave
    }
    bool ok = ours.size() == ref.size();
    auto a = ours.begin();
    auto b = ref.begin();
    for (; ok && a != ours.end(); ++a, ++b) {
        ok = a->first == b->first && std::equal(a->second.begin(), a->second.end(), b->second.begin(), b->second.end());
    }
    return ok;
}

void worker(void* p) {
    auto* args = static_cast<worker_args*>(p);
    args->ok = hammer_a_map(args->seed);
    xSemaphoreGive(args->done);
    vTaskDelete(nullptr);
}

void both_cores_allocate_concurrently() {
    worker_args a{1, false, xSemaphoreCreateBinary()};
    worker_args b{2, false, xSemaphoreCreateBinary()};
    const BaseType_t last_core = portNUM_PROCESSORS - 1;
    xTaskCreatePinnedToCore(worker, "psram-a", 8192, &a, 5, nullptr, 0);
    xTaskCreatePinnedToCore(worker, "psram-b", 8192, &b, 5, nullptr, last_core);
    EXPECT(xSemaphoreTake(a.done, pdMS_TO_TICKS(120000)) == pdTRUE);
    EXPECT(xSemaphoreTake(b.done, pdMS_TO_TICKS(120000)) == pdTRUE);
    EXPECT(a.ok);
    EXPECT(b.ok);
    vSemaphoreDelete(a.done);
    vSemaphoreDelete(b.done);
    vTaskDelay(pdMS_TO_TICKS(50));  // let the idle task reclaim the worker stacks
}

// ---- fragmentation: informational, integrity is what we assert ----------

void growth_versus_reserve_fragmentation_report() {
    auto measure = [](bool reserve) {
        psram::vector<psram::vector<int>> many;
        many.reserve(300);
        for (int i = 0; i < 300; ++i) {
            many.emplace_back();
            if (reserve) many.back().reserve(100);
        }
        for (int round = 0; round < 100; ++round) {
            for (auto& v : many) v.push_back(round);
        }
        const psram::heap_info h = psram::info();
        std::printf("    %-12s free %u, largest block %u\n", reserve ? "reserve()" : "push_back()",
                    static_cast<unsigned>(h.free), static_cast<unsigned>(h.largest_block));
    };
    measure(false);
    measure(true);
    EXPECT(psram::check_integrity(true));
}

}  // namespace

extern "C" void app_main() {
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    std::printf("\nesp32-psram-stl %s device tests on %s (%d cores)\n", PSRAM_STL_VERSION, CONFIG_IDF_TARGET,
                chip.cores);

    run("psram_is_present", psram_is_present);
    run("one_mebibyte_pattern_survives_other_allocations", one_mebibyte_pattern_survives_other_allocations);
    run("every_container_lives_in_psram", every_container_lives_in_psram);
    run("map_matches_std_map_under_random_operations", map_matches_std_map_under_random_operations);
    run("unordered_map_and_vector_match_std_under_random_operations",
        unordered_map_and_vector_match_std_under_random_operations);
    run("over_aligned_types_are_aligned_in_psram", over_aligned_types_are_aligned_in_psram);
    run("only_policy_throws_and_recovers_cleanly", only_policy_throws_and_recovers_cleanly);
    run("exhaust_psram_then_fall_back_to_internal_ram", exhaust_psram_then_fall_back_to_internal_ram);
    run("objects_live_in_psram", objects_live_in_psram);
    run("both_cores_allocate_concurrently", both_cores_allocate_concurrently);
    run("growth_versus_reserve_fragmentation_report", growth_versus_reserve_fragmentation_report);

    std::printf("\nDEVICE TESTS: %d passed, %d failed\n", g_passed, g_failed);
    std::printf(g_failed == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED\n");
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
