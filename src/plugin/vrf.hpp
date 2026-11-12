/** \brief Mangafire-style VRF request signing — pure transform.
 *
 * The host ships ONLY the transform; site XOR tables are declared by the
 * plugin (see plugins_dev/lua/mangafire). Ported byte-for-byte from the
 * goIsekai reference implementation; test vectors live in tests/test_vrf.cpp.
 */
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace plugin {

/** \brief One XOR-table stage of the VRF signer. `key`/`tbl` are decoded bytes. */
struct VrfStage {
    uint8_t iv = 0;
    std::string key;
    std::string tbl;
};

// Strict RFC 4648 base64. The vendored glaze `read_base64` stops silently at
// the first invalid char, which cannot produce the validation error contract
// the host tool needs, so decode is local and strict here.
inline std::optional<std::string> vrf_b64_decode(const std::string& s) {
    int8_t table[256];
    for (int i = 0; i < 256; ++i) table[i] = -1;
    const char* alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; ++i) table[static_cast<uint8_t>(alpha[i])] = static_cast<int8_t>(i);

    // validate (padding only trailing, max two)
    size_t end = s.size();
    size_t pad = 0;
    while (end > 0 && s[end - 1] == '=') { --end; ++pad; }
    if (pad > 2) return std::nullopt;
    const size_t len = end; // payload length excluding '='
    if (len % 4 == 1) return std::nullopt; // impossible base64 quantum
    std::string out;
    out.reserve(len * 3 / 4 + 1);
    uint32_t val = 0;
    int bits = 0;
    for (size_t i = 0; i < len; ++i) {
        const int d = table[static_cast<uint8_t>(s[i])];
        if (d < 0) return std::nullopt;
        val = (val << 6) | static_cast<uint32_t>(d);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((val >> bits) & 0xFF));
        }
    }
    return out;
}

inline std::string vrf_b64url_encode(const std::string& in) {
    const char* alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    for (size_t i = 0; i < in.size(); i += 3) {
        const uint32_t a = static_cast<uint8_t>(in[i]);
        const uint32_t b = (i + 1 < in.size()) ? static_cast<uint8_t>(in[i + 1]) : 0;
        const uint32_t c = (i + 2 < in.size()) ? static_cast<uint8_t>(in[i + 2]) : 0;
        const uint32_t n = (a << 16) | (b << 8) | c;
        out.push_back(alpha[(n >> 18) & 0x3F]);
        out.push_back(alpha[(n >> 12) & 0x3F]);
        if (i + 1 < in.size()) out.push_back(alpha[(n >> 6) & 0x3F]);
        if (i + 2 < in.size()) out.push_back(alpha[n & 0x3F]);
    }
    return out;
}

/** \brief VRF-sign `api_path` per the mangafire contract. Empty on bad input; `err` carries the reason. */
inline std::optional<std::string> vrf_sign(const std::string& api_path,
                                          const std::map<std::string, std::string>& params,
                                          const std::vector<VrfStage>& stages,
                                          std::string& err) {
    if (api_path.empty()) {
        err = "vrf_sign: api_path is empty";
        return std::nullopt;
    }
    if (stages.empty()) {
        err = "vrf_sign: stages are empty";
        return std::nullopt;
    }
    std::vector<VrfStage> decoded;
    decoded.reserve(stages.size());
    for (size_t i = 0; i < stages.size(); ++i) {
        const auto& st = stages[i];
        auto key = vrf_b64_decode(st.key);
        if (!key || key->empty()) {
            err = "vrf_sign: stage " + std::to_string(i + 1) + ": key: invalid base64";
            return std::nullopt;
        }
        auto tbl = vrf_b64_decode(st.tbl);
        if (!tbl || tbl->size() < 256) {
            err = "vrf_sign: stage " + std::to_string(i + 1) + ": tbl: invalid base64 (need 256 bytes)";
            return std::nullopt;
        }
        decoded.push_back({st.iv, std::move(*key), std::move(*tbl)});
    }

    std::string sign = api_path;
    if (sign.starts_with("/api")) sign.erase(0, 4); // Go TrimPrefix semantics: literal prefix, not path segment
    if (!params.empty()) {
        sign += "?";
        bool first = true;
        for (const auto& [k, v] : params) {
            if (!first) sign += "&";
            first = false;
            sign += k;
            sign += "=";
            sign += v;
        }
    }

    std::string data = std::move(sign);
    for (const auto& st : decoded) {
        std::string out;
        out.resize(data.size());
        uint8_t prev = st.iv;
        const size_t klen = st.key.size();
        const auto& tbl = st.tbl;
        for (size_t i = 0; i < data.size(); ++i) {
            const uint8_t x = static_cast<uint8_t>(data[i]) ^ static_cast<uint8_t>(st.key[i % klen]) ^ prev;
            prev = static_cast<uint8_t>(tbl[x]);
            out[i] = static_cast<char>(prev);
        }
        data = std::move(out);
    }
    return vrf_b64url_encode(data);
}

} // namespace plugin
