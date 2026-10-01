// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include <string_view>
#include <string>
#include <functional>
#include <xxhash.h>

namespace lacrima
{
    /**
     * @brief A 64-bit hashed string identifier using xxHash.
     * Guaranteed to be extremely fast for both compile-time and runtime hashing.
     * Replaces standard strings in hash maps, entity IDs, and asset lookups.
     */
    class StringID
    {
    public:
        // Default constructor, empty string hash
        constexpr StringID() : m_hash(0) {}

        // Construct from 64-bit raw hash
        constexpr explicit StringID(uint64_t hash) : m_hash(hash) {}

        // Construct from C-string (can be evaluated at compile time if XXH3 is constexpr capable, otherwise runtime)
        explicit StringID(const char* str)
        {
            m_hash = str ? XXH3_64bits(str, std::char_traits<char>::length(str)) : 0;
        }

        // Construct from std::string_view
        explicit StringID(std::string_view str)
        {
            m_hash = XXH3_64bits(str.data(), str.length());
        }

        // Construct from std::string
        explicit StringID(const std::string& str)
        {
            m_hash = XXH3_64bits(str.data(), str.length());
        }

        constexpr uint64_t Value() const { return m_hash; }
        constexpr explicit operator uint64_t() const { return m_hash; }
        constexpr explicit operator bool() const { return m_hash != 0; }

        constexpr bool operator==(const StringID& other) const { return m_hash == other.m_hash; }
        constexpr bool operator!=(const StringID& other) const { return m_hash != other.m_hash; }
        constexpr bool operator<(const StringID& other) const { return m_hash < other.m_hash; }

        // Helper to hash an arbitrary block of memory
        static uint64_t HashMemory(const void* data, size_t length)
        {
            return XXH3_64bits(data, length);
        }

    private:
        uint64_t m_hash;
    };
} // namespace lacrima

namespace std
{
    template <>
    struct hash<lacrima::StringID>
    {
        size_t operator()(const lacrima::StringID& id) const
        {
            return static_cast<size_t>(id.Value());
        }
    };
}

