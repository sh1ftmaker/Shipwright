#pragma once

// C++17/C++20 compatibility layer for Emscripten and other platforms

#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <string>

// std::map/set::contains() is a C++20 feature
// Provide a compatibility layer for C++17

// Helper namespace with contains functions
namespace compat {
    template<typename Container, typename Key>
    inline bool contains(const Container& container, const Key& key) {
        return container.find(key) != container.end();
    }
}

// Macro to make the usage cleaner - use MAP_CONTAINS instead of .contains()
#define MAP_CONTAINS(map, key) (compat::contains(map, key))
#define SET_CONTAINS(set, key) (compat::contains(set, key))

// For code that uses std::contains() directly, provide it in std namespace
// Note: This might not work on all compilers, so the macros above are preferred
#if __cplusplus < 202002L
namespace std {
    using compat::contains;
}
#endif