// Host test suite for esp32-psram-stl.
//
// Runs against a simulated ESP32 heap (fake_heap.cpp) under AddressSanitizer
// and UndefinedBehaviorSanitizer. Proves, for every container and helper:
// where each byte comes from, that nothing leaks, that out-of-memory paths
// behave as documented, and that behavior matches the plain std:: containers.

#include <PsramStl.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <deque>
#include <list>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "harness.hpp"

using fakeheap::region;

namespace {

/// Records the heap at construction; answers what happened since.
struct heap_scope {
    std::size_t psram_live = fakeheap::stats(region::psram).live_blocks;
    std::size_t internal_live = fakeheap::stats(region::internal).live_blocks;
    std::size_t psram_allocs = fakeheap::stats(region::psram).allocations;
    std::size_t internal_allocs = fakeheap::stats(region::internal).allocations;
    std::size_t aligned = fakeheap::stats(region::psram).aligned_allocations;

    std::size_t new_psram_allocations() const { return fakeheap::stats(region::psram).allocations - psram_allocs; }
    std::size_t new_internal_allocations() const {
        return fakeheap::stats(region::internal).allocations - internal_allocs;
    }
    std::size_t new_aligned_allocations() const {
        return fakeheap::stats(region::psram).aligned_allocations - aligned;
    }
    bool leak_free() const {
        return fakeheap::stats(region::psram).live_blocks == psram_live &&
               fakeheap::stats(region::internal).live_blocks == internal_live;
    }
};

region owner(const void* p) {
    region r = region::internal;
    CHECK(fakeheap::owner_of(p, &r));
    return r;
}

template <class Container>
bool every_element_in_psram(const Container& c) {
    for (const auto& e : c) {
        if (!psram::is_psram(&e)) {
            return false;
        }
    }
    return true;
}

std::size_t reported_oom_bytes = 0;
void record_oom(std::size_t bytes) { reported_oom_bytes = bytes; }

struct tracked {
    static int alive;
    int value;
    explicit tracked(int v) : value(v) { ++alive; }
    tracked(const tracked& o) : value(o.value) { ++alive; }
    ~tracked() { --alive; }
};
int tracked::alive = 0;

}  // namespace

// ---------------------------------------------------------------------------
// The allocator contract
// ---------------------------------------------------------------------------
TEST(allocator_meets_the_standard_contract) {
    using A = psram::allocator<int>;
    using traits = std::allocator_traits<A>;
    static_assert(traits::is_always_equal::value);
    static_assert(std::is_same_v<traits::rebind_alloc<double>, psram::allocator<double>>);
    static_assert(std::is_same_v<traits::rebind_alloc<double>::policy_type, psram::policy::only>);
    static_assert(std::is_same_v<psram::vector<int>, std::vector<int, psram::allocator<int>>>);
    static_assert(std::is_same_v<psram::fallback::vector<int>,
                                 std::vector<int, psram::allocator<int, psram::policy::prefer>>>);
    static_assert(std::is_same_v<psram::string,
                                 std::basic_string<char, std::char_traits<char>, psram::allocator<char>>>);
    static_assert(sizeof(psram::unique_ptr<int>) == sizeof(int*));
    static_assert(std::is_empty_v<psram::resident>);
    static_assert(std::is_empty_v<psram::allocator<int>>);

    A a;
    psram::allocator<double> b(a);
    CHECK(a == b);
    CHECK(!(a != b));
    int* p = a.allocate(16);
    CHECK(psram::is_psram(p));
    a.deallocate(p, 16);
}

TEST(size_overflow_is_rejected) {
    psram::allocator<std::uint64_t> a;
    CHECK_THROWS(a.allocate(SIZE_MAX / 4), std::bad_array_new_length);
}

// ---------------------------------------------------------------------------
// Containers: every byte in PSRAM, nothing leaks
// ---------------------------------------------------------------------------
TEST(vector_elements_live_in_psram) {
    heap_scope s;
    {
        psram::vector<int> v;
        for (int i = 0; i < 10000; ++i) {
            v.push_back(i);
        }
        CHECK(psram::is_psram(v.data()));
        long long sum = 0;
        for (int x : v) sum += x;
        CHECK_EQ(sum, 49995000LL);
    }
    CHECK(s.leak_free());
    CHECK(s.new_psram_allocations() > 0);
    CHECK_EQ(s.new_internal_allocations(), 0u);
}

TEST(every_standard_container_lives_in_psram) {
    heap_scope s;
    {
        psram::deque<int> dq;
        psram::list<int> li;
        psram::forward_list<int> fl;
        psram::map<int, int> mp;
        psram::multimap<int, int> mm;
        psram::set<int> st;
        psram::multiset<int> ms;
        psram::unordered_map<int, int> um;
        psram::unordered_multimap<int, int> umm;
        psram::unordered_set<int> us;
        psram::unordered_multiset<int> ums;
        for (int i = 0; i < 500; ++i) {
            dq.push_back(i);
            li.push_back(i);
            fl.push_front(i);
            mp[i] = i;
            mm.emplace(i % 7, i);
            st.insert(i);
            ms.insert(i % 5);
            um[i] = i;
            umm.emplace(i % 3, i);
            us.insert(i);
            ums.insert(i % 9);
        }
        CHECK(every_element_in_psram(dq));
        CHECK(every_element_in_psram(li));
        CHECK(every_element_in_psram(fl));
        CHECK(every_element_in_psram(mp));
        CHECK(every_element_in_psram(mm));
        CHECK(every_element_in_psram(st));
        CHECK(every_element_in_psram(ms));
        CHECK(every_element_in_psram(um));
        CHECK(every_element_in_psram(umm));
        CHECK(every_element_in_psram(us));
        CHECK(every_element_in_psram(ums));
        CHECK_EQ(mp.size(), 500u);
        CHECK_EQ(ms.size(), 500u);
    }
    CHECK(s.leak_free());
    CHECK_EQ(s.new_internal_allocations(), 0u);
}

TEST(strings_use_psram_beyond_the_small_string_buffer) {
    heap_scope s;
    {
        psram::string small = "hi";
        CHECK_EQ(s.new_psram_allocations(), 0u);  // fits the inline buffer
        psram::string large(200, 'x');
        CHECK(psram::is_psram(large.data()));
        large += small;
        CHECK_EQ(large.size(), 202u);
        CHECK(large.compare(200, 2, "hi") == 0);
    }
    CHECK(s.leak_free());
}

TEST(nested_containers_stay_in_psram) {
    heap_scope s;
    {
        psram::map<psram::string, psram::vector<int>> index;
        for (int i = 0; i < 200; ++i) {
            psram::string key = "sensor-with-a-long-name-" + psram::string(std::to_string(i).c_str());
            index[key].assign(64, i);
        }
        for (const auto& [key, values] : index) {
            CHECK(psram::is_psram(key.data()));
            CHECK(psram::is_psram(values.data()));
        }
    }
    CHECK(s.leak_free());
    CHECK_EQ(s.new_internal_allocations(), 0u);
}

TEST(swap_move_and_splice_keep_memory_in_psram) {
    heap_scope s;
    {
        psram::vector<int> a(1000, 1);
        const int* buffer = a.data();
        psram::vector<int> b(std::move(a));
        CHECK(b.data() == buffer);  // moved, not copied
        psram::map<int, int> m1{{1, 1}, {2, 2}};
        psram::map<int, int> m2{{3, 3}};
        m1.swap(m2);
        CHECK_EQ(m1.size(), 1u);
        CHECK_EQ(m2.size(), 2u);
        psram::list<int> l1{1, 2, 3};
        psram::list<int> l2{4, 5};
        l1.splice(l1.end(), l2);
        CHECK_EQ(l1.size(), 5u);
        CHECK(every_element_in_psram(l1));
        psram::vector<int> c;
        c = b;  // copy assignment
        CHECK(psram::is_psram(c.data()));
    }
    CHECK(s.leak_free());
}

// ---------------------------------------------------------------------------
// Differential testing: same random operations, same results as std::
// ---------------------------------------------------------------------------
TEST(map_behaves_exactly_like_std_map) {
    heap_scope s;
    {
        std::mt19937 rng(1234);
        psram::map<int, int> ours;
        std::map<int, int> ref;
        for (int step = 0; step < 50000; ++step) {
            const int key = static_cast<int>(rng() % 2000);
            switch (rng() % 4) {
                case 0: ours[key] = step; ref[key] = step; break;
                case 1: ours.erase(key); ref.erase(key); break;
                case 2: ours.emplace(key, -step); ref.emplace(key, -step); break;
                default: CHECK_EQ(ours.count(key), ref.count(key)); break;
            }
            if (step % 5000 == 0) {
                CHECK(std::equal(ours.begin(), ours.end(), ref.begin(), ref.end()));
            }
        }
        CHECK(std::equal(ours.begin(), ours.end(), ref.begin(), ref.end()));
    }
    CHECK(s.leak_free());
}

TEST(unordered_map_behaves_exactly_like_std_unordered_map) {
    heap_scope s;
    {
        std::mt19937 rng(99);
        psram::unordered_map<std::uint32_t, std::uint32_t> ours;
        std::unordered_map<std::uint32_t, std::uint32_t> ref;
        for (std::uint32_t step = 0; step < 50000; ++step) {
            const auto key = static_cast<std::uint32_t>(rng() % 3000);
            if (rng() % 3 == 0) {
                ours.erase(key);
                ref.erase(key);
            } else {
                ours[key] += step;
                ref[key] += step;
            }
        }
        CHECK_EQ(ours.size(), ref.size());
        for (const auto& [k, v] : ref) {
            auto it = ours.find(k);
            CHECK(it != ours.end() && it->second == v);
        }
        ours.rehash(0);
        ours.reserve(10000);
        CHECK(every_element_in_psram(ours));
    }
    CHECK(s.leak_free());
}

TEST(sequences_behave_exactly_like_std_sequences) {
    heap_scope s;
    {
        std::mt19937 rng(7);
        psram::vector<int> v;
        std::vector<int> rv;
        psram::deque<int> d;
        std::deque<int> rd;
        psram::list<int> l;
        std::list<int> rl;
        for (int step = 0; step < 20000; ++step) {
            const int value = static_cast<int>(rng());
            const auto op = static_cast<unsigned>(rng() % 5);
            if (op < 3 || rv.empty()) {
                const std::size_t at = rv.empty() ? 0 : rng() % (rv.size() + 1);
                v.insert(v.begin() + static_cast<long>(at), value);
                rv.insert(rv.begin() + static_cast<long>(at), value);
                d.push_front(value);
                rd.push_front(value);
                l.push_back(value);
                rl.push_back(value);
            } else {
                const std::size_t at = rng() % rv.size();
                v.erase(v.begin() + static_cast<long>(at));
                rv.erase(rv.begin() + static_cast<long>(at));
                d.pop_back();
                rd.pop_back();
                l.pop_front();
                rl.pop_front();
            }
        }
        CHECK(std::equal(v.begin(), v.end(), rv.begin(), rv.end()));
        CHECK(std::equal(d.begin(), d.end(), rd.begin(), rd.end()));
        CHECK(std::equal(l.begin(), l.end(), rl.begin(), rl.end()));
        v.shrink_to_fit();
        CHECK(v.empty() || psram::is_psram(v.data()));
    }
    CHECK(s.leak_free());
}

// ---------------------------------------------------------------------------
// Alignment
// ---------------------------------------------------------------------------
TEST(over_aligned_types_get_aligned_memory) {
    struct alignas(64) block64 {
        float values[16];
    };
    struct alignas(16) block16 {
        std::uint32_t words[4];
    };
    heap_scope s;
    {
        psram::vector<int> plain(100);
        CHECK_EQ(s.new_aligned_allocations(), 0u);  // 4-byte types take the plain path

        psram::vector<block64> a(33);
        psram::vector<block16> b(17);
        CHECK(reinterpret_cast<std::uintptr_t>(a.data()) % 64 == 0);
        CHECK(reinterpret_cast<std::uintptr_t>(b.data()) % 16 == 0);
        CHECK(s.new_aligned_allocations() >= 2u);
        CHECK(psram::is_psram(a.data()));
    }
    CHECK(s.leak_free());
}

// ---------------------------------------------------------------------------
// Out of memory
// ---------------------------------------------------------------------------
TEST(only_policy_raises_bad_alloc_when_psram_is_full) {
    fakeheap::set_capacity(region::psram, 1024);
    reported_oom_bytes = 0;
    psram::oom_handler previous = psram::set_oom_handler(record_oom);
    heap_scope s;
    {
        psram::vector<int> v;
        CHECK_THROWS(v.reserve(10000), std::bad_alloc);
        CHECK_EQ(reported_oom_bytes, 40000u);
        CHECK(v.empty());
    }
    CHECK(s.leak_free());
    CHECK_EQ(s.new_internal_allocations(), 0u);  // never touched internal RAM
    psram::set_oom_handler(previous);
}

TEST(fallback_policy_prefers_psram) {
    heap_scope s;
    {
        psram::fallback::vector<int> v(1000);
        CHECK(psram::is_psram(v.data()));
    }
    CHECK_EQ(s.new_internal_allocations(), 0u);
    CHECK(s.leak_free());
}

TEST(fallback_policy_uses_internal_ram_when_psram_is_full) {
    fakeheap::set_capacity(region::psram, 256);
    heap_scope s;
    {
        psram::fallback::vector<int> v(1000);
        CHECK(!psram::is_psram(v.data()));
        CHECK(owner(v.data()) == region::internal);
        psram::fallback::map<int, int> m;
        for (int i = 0; i < 100; ++i) m[i] = i;
        CHECK_EQ(m.size(), 100u);
    }
    CHECK(s.new_internal_allocations() > 0u);
    CHECK(s.leak_free());
}

TEST(no_psram_at_all) {
    fakeheap::set_capacity(region::psram, 0);
    CHECK(!psram::available());
    CHECK(!psram::begin());
    psram::vector<int> strict_v;
    CHECK_THROWS(strict_v.push_back(1), std::bad_alloc);
    psram::fallback::vector<int> soft_v;
    soft_v.push_back(1);
    CHECK_EQ(soft_v.size(), 1u);
}

// ---------------------------------------------------------------------------
// Objects
// ---------------------------------------------------------------------------
TEST(make_unique_and_make_shared_live_in_psram) {
    heap_scope s;
    {
        auto u = psram::make_unique<tracked>(7);
        CHECK(psram::is_psram(u.get()));
        CHECK_EQ(u->value, 7);
        auto shared = psram::make_shared<tracked>(8);
        CHECK(psram::is_psram(shared.get()));
        auto copy = shared;
        CHECK_EQ(shared.use_count(), 2L);
        CHECK_EQ(tracked::alive, 2);
        auto soft = psram::fallback::make_unique<tracked>(9);
        CHECK(psram::is_psram(soft.get()));
    }
    CHECK_EQ(tracked::alive, 0);
    CHECK(s.leak_free());
}

TEST(make_unique_frees_memory_if_the_constructor_throws) {
    struct explodes {
        explodes() { throw std::runtime_error("boom"); }
    };
    heap_scope s;
    CHECK_THROWS(psram::make_unique<explodes>(), std::runtime_error);
    CHECK(s.leak_free());
}

namespace {
struct widget : psram::resident {
    int payload[64] = {};
};
struct alignas(64) aligned_widget : psram::resident {
    char payload[128] = {};
};
struct animal : psram::resident {
    virtual ~animal() = default;
    virtual int legs() const = 0;
};
struct dog : animal {
    int legs() const override { return 4; }
    double weight = 12.5;
};
struct fallback_widget : psram::fallback::resident {
    int payload[8] = {};
};
}  // namespace

TEST(resident_types_allocate_from_psram) {
    heap_scope s;
    {
        widget* w = new widget();
        CHECK(psram::is_psram(w));
        delete w;

        widget* many = new widget[4];
        CHECK(psram::is_psram(many));
        delete[] many;

        auto* big = new aligned_widget();
        CHECK(reinterpret_cast<std::uintptr_t>(big) % 64 == 0);
        CHECK(psram::is_psram(big));
        delete big;

        std::unique_ptr<animal> pet(new dog());  // polymorphic delete frees the right address
        CHECK(psram::is_psram(pet.get()));
        CHECK_EQ(pet->legs(), 4);

        widget on_stack;  // not heap: lives where it is declared
        CHECK(!psram::is_psram(&on_stack));

        alignas(widget) unsigned char buffer[sizeof(widget)];
        widget* placed = new (buffer) widget();
        CHECK(!psram::is_psram(placed));
        placed->~widget();
    }
    CHECK(s.leak_free());
}

TEST(resident_nothrow_new_returns_null_when_full) {
    fakeheap::set_capacity(region::psram, 16);
    heap_scope s;
    widget* none = new (std::nothrow) widget();
    CHECK(none == nullptr);
    CHECK_THROWS(new widget(), std::bad_alloc);

    auto* soft = new fallback_widget();  // falls back to internal RAM
    CHECK(soft != nullptr);
    CHECK(!psram::is_psram(soft));
    delete soft;
    CHECK(s.leak_free());
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------
TEST(info_tracks_the_psram_heap) {
    const psram::heap_info before = psram::info();
    CHECK_EQ(before.total, 4u * 1024 * 1024);
    CHECK(psram::available());
    {
        psram::vector<std::uint8_t> v(4000);
        const psram::heap_info during = psram::info();
        CHECK_EQ(before.free - during.free, 4000u);
        CHECK(during.minimum_free <= during.free);
        CHECK(during.largest_block <= during.free);
    }
    CHECK_EQ(psram::info().free, before.free);
}

TEST(heap_guards_are_intact_after_everything) {
    CHECK(psram::check_integrity(true));
    CHECK(heap_caps_check_integrity(MALLOC_CAP_INTERNAL, true));
}

// ---------------------------------------------------------------------------
int main() {
    int failed = 0;
    for (const auto& t : harness::registry()) {
        harness::failures_in_current_test() = 0;
        fakeheap::reset_capacities();
        psram::set_oom_handler(nullptr);
        t.body();
        const bool ok = harness::failures_in_current_test() == 0;
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", t.name);
        failed += ok ? 0 : 1;
    }
    const std::size_t total = harness::registry().size();
    std::printf("\n%zu tests, %zu passed, %d failed\n", total, total - failed, failed);
    return failed == 0 ? 0 : 1;
}
