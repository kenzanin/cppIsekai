/** \brief host.codecs.* cores (P6): std-only base64/hex, one implementation,
 *  thin marshals per runtime (design D4). Decode is strict-optional —
 *  nil/null/kNotFound on invalid input, the house contract. */
#pragma once

#include "plugin/vrf.hpp" // vrf_b64_decode -- strict RFC 4648, reused verbatim

#include <optional>
#include <string>

namespace plugin::host_codec {

/** \brief Standard-alphabet base64 with padding (Go StdEncoding). */
inline std::string base64_encode(const std::string& in) {
    static constexpr char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        const uint32_t n = (static_cast<uint32_t>(static_cast<uint8_t>(in[i])) << 16) |
                           (static_cast<uint32_t>(static_cast<uint8_t>(in[i + 1])) << 8) |
                           static_cast<uint32_t>(static_cast<uint8_t>(in[i + 2]));
        out.push_back(alpha[(n >> 18) & 0x3F]);
        out.push_back(alpha[(n >> 12) & 0x3F]);
        out.push_back(alpha[(n >> 6) & 0x3F]);
        out.push_back(alpha[n & 0x3F]);
    }
    const size_t rem = in.size() - i;
    if (rem == 1) {
        const uint32_t n = static_cast<uint32_t>(static_cast<uint8_t>(in[i])) << 16;
        out.push_back(alpha[(n >> 18) & 0x3F]);
        out.push_back(alpha[(n >> 12) & 0x3F]);
        out += "==";
    } else if (rem == 2) {
        const uint32_t n = (static_cast<uint32_t>(static_cast<uint8_t>(in[i])) << 16) |
                           (static_cast<uint32_t>(static_cast<uint8_t>(in[i + 1])) << 8);
        out.push_back(alpha[(n >> 18) & 0x3F]);
        out.push_back(alpha[(n >> 12) & 0x3F]);
        out.push_back(alpha[(n >> 6) & 0x3F]);
        out += '=';
    }
    return out;
}

/** \brief Strict standard base64 decode; nullopt on any invalid input.
 *  Delegates to the vrf core — one implementation, no second table. */
inline std::optional<std::string> base64_decode(const std::string& s) {
    return vrf_b64_decode(s);
}

/** \brief Lowercase hex (Go hex.EncodeToString). */
inline std::string hex_encode(const std::string& in) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(in.size() * 2);
    for (const char c : in) {
        out.push_back(hex[(static_cast<uint8_t>(c) >> 4) & 0xF]);
        out.push_back(hex[static_cast<uint8_t>(c) & 0xF]);
    }
    return out;
}

/** \brief Hex decode, upper or lower case; nullopt on odd length / bad digit. */
inline std::optional<std::string> hex_decode(const std::string& s) {
    if (s.size() % 2 != 0) {
        return std::nullopt;
    }
    static constexpr auto nib = [](const char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    std::string out;
    out.reserve(s.size() / 2);
    for (size_t i = 0; i < s.size(); i += 2) {
        const int hi = nib(s[i]);
        const int lo = nib(s[i + 1]);
        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return out;
}

} // namespace plugin::host_codec
