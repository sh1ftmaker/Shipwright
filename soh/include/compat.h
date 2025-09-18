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

    inline bool ends_with(const std::string& str, const std::string& suffix) {
        return str.size() >= suffix.size() &&
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    inline bool ends_with(const std::string& str, char ch) {
        return !str.empty() && str[str.size() - 1] == ch;
    }

    inline bool ends_with(const std::string& str, const char* suffix) {
        return ends_with(str, std::string(suffix));
    }
}

// std::popcount (C++20) - counts the number of 1 bits in an integer
// std::rotr (C++20) - rotate bits right
namespace std {
    template<typename T>
    inline int popcount(T value) {
        static_assert(std::is_unsigned_v<T>, "popcount requires unsigned type");
        int count = 0;
        while (value) {
            count += value & 1;
            value >>= 1;
        }
        return count;
    }

    template<typename T>
    inline T rotr(T value, int shift) {
        static_assert(std::is_unsigned_v<T>, "rotr requires unsigned type");
        const int bits = sizeof(T) * 8;
        shift %= bits;
        if (shift == 0) return value;
        return (value >> shift) | (value << (bits - shift));
    }

    template<typename T>
    inline T rotl(T value, int shift) {
        static_assert(std::is_unsigned_v<T>, "rotl requires unsigned type");
        const int bits = sizeof(T) * 8;
        shift %= bits;
        if (shift == 0) return value;
        return (value << shift) | (value >> (bits - shift));
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

// Macros for string operations
#define STRING_STARTS_WITH(str, prefix) (compat::starts_with(str, prefix))
#define STRING_ENDS_WITH(str, suffix) (compat::ends_with(str, suffix))

#else // C++20 or later

// For C++20, just use the standard features
#define STRING_STARTS_WITH(str, prefix) ((str).starts_with(prefix))
#define STRING_ENDS_WITH(str, suffix) ((str).ends_with(suffix))

#endif // __cplusplus < 202002L