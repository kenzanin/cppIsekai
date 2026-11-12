#pragma once
/** \file url_codec.hpp
 *  \brief Query-component percent codec shared by the lua/js/wasm bindings
 *  (auto-update js-wasm-host-parity 1.1). encode = RFC 3986 unreserved
 *  pass-through, everything else %XX (uppercase, UTF-8 bytes). decode accepts
 *  strict %XX and leaves everything else verbatim — `+` is NOT decoded to
 *  space: this is queryEncode semantics (goIsekai parity), not form encoding.
 *  Header-only: one implementation, three marshals. */
#include <cctype>
#include <string>

namespace plugin {

inline std::string url_encode(const std::string& s) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() + 8);
    for (const char c : s) {
        const auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) != 0 || c == '-' || c == '.' || c == '_' || c == '~') {
            out += c;
        } else {
            out += '%';
            out += kHex[uc >> 4];
            out += kHex[uc & 0x0F];
        }
    }
    return out;
}

inline int url_hex(const char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

inline std::string url_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 <= s.size() - 1) {
            const int hi = url_hex(s[i + 1]);
            const int lo = url_hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out += s[i];
    }
    return out;
}

} // namespace plugin
