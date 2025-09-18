#pragma once

// C++17/C++20 compatibility layer for Emscripten and other platforms

#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <string>
#include <algorithm>
#include <vector>

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

// C++20 compatibility features for C++17
#if __cplusplus < 202002L

// std::string::starts_with (C++20)
namespace compat {
    inline bool starts_with(const std::string& str, const std::string& prefix) {
        return str.size() >= prefix.size() &&
               str.compare(0, prefix.size(), prefix) == 0;
    }

    inline bool starts_with(const std::string& str, char ch) {
        return !str.empty() && str[0] == ch;
    }

    inline bool starts_with(const std::string& str, const char* prefix) {
        return starts_with(str, std::string(prefix));
    }
}

// std::erase (C++20) - removes all occurrences of a value from a container
namespace std {
    template<typename CharT, typename Traits, typename Allocator, typename U>
    inline typename basic_string<CharT, Traits, Allocator>::size_type
    erase(basic_string<CharT, Traits, Allocator>& str, const U& value) {
        auto it = remove(str.begin(), str.end(), value);
        auto count = distance(it, str.end());
        str.erase(it, str.end());
        return count;
    }

    template<typename T, typename Allocator, typename U>
    inline typename vector<T, Allocator>::size_type
    erase(vector<T, Allocator>& vec, const U& value) {
        auto it = remove(vec.begin(), vec.end(), value);
        auto count = distance(it, vec.end());
        vec.erase(it, vec.end());
        return count;
    }

    // For code that uses std::contains() directly
    using compat::contains;
}

// Macro for starts_with
#define STRING_STARTS_WITH(str, prefix) (compat::starts_with(str, prefix))

#else // C++20 or later

// For C++20, just use the standard features
#define STRING_STARTS_WITH(str, prefix) ((str).starts_with(prefix))

#endif // __cplusplus < 202002L