#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <algorithm>
#include "dgb/Spoof.h"

namespace TagDigest
{
    constexpr std::uint32_t DigestConst(const char* str)
    {
        std::uint32_t h = 5381u;
        while (*str)
            h = ((h << 5) + h) + static_cast<std::uint32_t>(static_cast<unsigned char>(*str++));
        return h;
    }

    inline std::uint32_t Digest(const char* str)
    {
        SPOOF_FUNC;
        std::uint32_t h = 5381u;
        while (str && *str)
            h = ((h << 5) + h) + static_cast<std::uint32_t>(static_cast<unsigned char>(*str++));
        return h;
    }

    inline std::uint32_t Digest(const std::string& s)
    {
        SPOOF_FUNC;
        return Digest(s.c_str());
    }

    inline std::uint32_t DigestPrefix(const char* str, std::size_t len)
    {
        SPOOF_FUNC;
        std::uint32_t h = 5381u;
        for (std::size_t i = 0; i < len && str[i]; ++i)
            h = ((h << 5) + h) + static_cast<std::uint32_t>(static_cast<unsigned char>(str[i]));
        return h;
    }

    inline std::uint32_t DigestPrefix(const std::string& s, std::size_t maxLen)
    {
        SPOOF_FUNC;
        std::size_t len = (std::min)(maxLen, s.size());
        return DigestPrefix(s.c_str(), len);
    }

#define TAG_DIGEST(str) TagDigest::DigestConst(str)
}
