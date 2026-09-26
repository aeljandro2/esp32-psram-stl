// esp32-psram-stl: put the C++ standard library in the ESP32's PSRAM.
// SPDX-License-Identifier: MIT
//
// Drop-in aliases for the standard containers, backed by psram::allocator.
//
//   psram::vector<int>                 PSRAM only
//   psram::fallback::vector<int>       PSRAM first, internal RAM when full
//
// The container object itself (its few-byte header) lives wherever you
// declare it: on the stack, inside another object, or in a global. Its
// elements and nodes live in PSRAM.

#pragma once

#include <deque>
#include <forward_list>
#include <functional>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "allocator.hpp"

namespace psram {

namespace detail {

// One definition of every alias, parameterized by policy. The public names
// below pick the policy, so both families stay in sync by construction.
template <class Policy>
struct containers {
    template <class T>
    using vector = std::vector<T, allocator<T, Policy>>;

    template <class T>
    using deque = std::deque<T, allocator<T, Policy>>;

    template <class T>
    using list = std::list<T, allocator<T, Policy>>;

    template <class T>
    using forward_list = std::forward_list<T, allocator<T, Policy>>;

    template <class K, class V, class Compare = std::less<K>>
    using map = std::map<K, V, Compare, allocator<std::pair<const K, V>, Policy>>;

    template <class K, class V, class Compare = std::less<K>>
    using multimap = std::multimap<K, V, Compare, allocator<std::pair<const K, V>, Policy>>;

    template <class K, class Compare = std::less<K>>
    using set = std::set<K, Compare, allocator<K, Policy>>;

    template <class K, class Compare = std::less<K>>
    using multiset = std::multiset<K, Compare, allocator<K, Policy>>;

    template <class K, class V, class Hash = std::hash<K>, class Equal = std::equal_to<K>>
    using unordered_map = std::unordered_map<K, V, Hash, Equal, allocator<std::pair<const K, V>, Policy>>;

    template <class K, class V, class Hash = std::hash<K>, class Equal = std::equal_to<K>>
    using unordered_multimap =
        std::unordered_multimap<K, V, Hash, Equal, allocator<std::pair<const K, V>, Policy>>;

    template <class K, class Hash = std::hash<K>, class Equal = std::equal_to<K>>
    using unordered_set = std::unordered_set<K, Hash, Equal, allocator<K, Policy>>;

    template <class K, class Hash = std::hash<K>, class Equal = std::equal_to<K>>
    using unordered_multiset = std::unordered_multiset<K, Hash, Equal, allocator<K, Policy>>;

    template <class CharT, class Traits = std::char_traits<CharT>>
    using basic_string = std::basic_string<CharT, Traits, allocator<CharT, Policy>>;
};

}  // namespace detail

// ---- PSRAM only (psram::) -------------------------------------------------
#define PSRAM_STL_DECLARE_CONTAINERS(POLICY)                                                             \
    template <class T>                                                                                   \
    using vector = typename ::psram::detail::containers<POLICY>::template vector<T>;                     \
    template <class T>                                                                                   \
    using deque = typename ::psram::detail::containers<POLICY>::template deque<T>;                       \
    template <class T>                                                                                   \
    using list = typename ::psram::detail::containers<POLICY>::template list<T>;                         \
    template <class T>                                                                                   \
    using forward_list = typename ::psram::detail::containers<POLICY>::template forward_list<T>;         \
    template <class K, class V, class Compare = std::less<K>>                                            \
    using map = typename ::psram::detail::containers<POLICY>::template map<K, V, Compare>;               \
    template <class K, class V, class Compare = std::less<K>>                                            \
    using multimap = typename ::psram::detail::containers<POLICY>::template multimap<K, V, Compare>;     \
    template <class K, class Compare = std::less<K>>                                                     \
    using set = typename ::psram::detail::containers<POLICY>::template set<K, Compare>;                  \
    template <class K, class Compare = std::less<K>>                                                     \
    using multiset = typename ::psram::detail::containers<POLICY>::template multiset<K, Compare>;        \
    template <class K, class V, class Hash = std::hash<K>, class Equal = std::equal_to<K>>               \
    using unordered_map =                                                                                \
        typename ::psram::detail::containers<POLICY>::template unordered_map<K, V, Hash, Equal>;         \
    template <class K, class V, class Hash = std::hash<K>, class Equal = std::equal_to<K>>               \
    using unordered_multimap =                                                                           \
        typename ::psram::detail::containers<POLICY>::template unordered_multimap<K, V, Hash, Equal>;    \
    template <class K, class Hash = std::hash<K>, class Equal = std::equal_to<K>>                        \
    using unordered_set = typename ::psram::detail::containers<POLICY>::template unordered_set<K, Hash, Equal>; \
    template <class K, class Hash = std::hash<K>, class Equal = std::equal_to<K>>                        \
    using unordered_multiset =                                                                           \
        typename ::psram::detail::containers<POLICY>::template unordered_multiset<K, Hash, Equal>;       \
    template <class CharT, class Traits = std::char_traits<CharT>>                                       \
    using basic_string = typename ::psram::detail::containers<POLICY>::template basic_string<CharT, Traits>; \
    using string = basic_string<char>;

PSRAM_STL_DECLARE_CONTAINERS(::psram::policy::only)

// ---- PSRAM first, internal RAM as fallback (psram::fallback::) -------------
namespace fallback {
PSRAM_STL_DECLARE_CONTAINERS(::psram::policy::prefer)
}  // namespace fallback

#undef PSRAM_STL_DECLARE_CONTAINERS

}  // namespace psram
