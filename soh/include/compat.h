#pragma once

// C++17/C++20 compatibility layer for Emscripten and other platforms

#include <algorithm>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>

// std::map/set::contains() is a C++20 feature
// Provide a compatibility layer for C++17
#if __cplusplus < 202002L

namespace compat {
    template<typename Container, typename Key>
    inline bool contains(const Container& container, const Key& key) {
        return container.find(key) != container.end();
    }
}

// Extension methods for standard containers
namespace std {
    template<typename K, typename V, typename C, typename A>
    inline bool contains(const map<K, V, C, A>& m, const K& key) {
        return m.find(key) != m.end();
    }

    template<typename K, typename V, typename H, typename E, typename A>
    inline bool contains(const unordered_map<K, V, H, E, A>& m, const K& key) {
        return m.find(key) != m.end();
    }

    template<typename K, typename C, typename A>
    inline bool contains(const set<K, C, A>& s, const K& key) {
        return s.find(key) != s.end();
    }

    template<typename K, typename H, typename E, typename A>
    inline bool contains(const unordered_set<K, H, E, A>& s, const K& key) {
        return s.find(key) != s.end();
    }
}

#endif // __cplusplus < 202002L