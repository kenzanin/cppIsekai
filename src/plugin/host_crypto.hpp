/** \brief host.crypto.* cores (P7): SHA-256 / MD5 / HMAC-SHA256 via
 *  hash-library, AES-GCM decryption built on tiny-aes-c (block cipher only —
 *  CTR keystream + GHASH live here). Hex strings in/out; nil on any failure
 *  or tag mismatch (plaintext only returns verified).
 *
 *  GCM per NIST SP 800-38D: GHASH over AAD||pad||CT||pad||len-block with the
 *  x^128+x^7+x^2+x+1 polynomial (0xE1 leading byte); 96-bit IVs used directly
 *  as J0, longer IVs derived via GHASH. Tag is always 16 bytes.
 */
#pragma once

#include "plugin/host_codecs.hpp" // hex_decode + the optional convention

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

// Vendored pin (third_party/tiny-aes, same upstream commit as the xmake
// package): the prebuilt package lib is 128-only, so the tiny-aes256 target
// compiles this copy with AES256; no __cplusplus guard upstream. The define
// is repeated here because target defines do not propagate to consumers --
// keep it identical to the tiny-aes256 add_defines or the ctx ABI diverges.
#define AES256 1
extern "C" {
#include "aes.h"
}

#include <md5.h>
#include <sha256.h>
#include <hmac.h>

namespace plugin::host_crypto {

/** \brief Lowercase-hex SHA-256 (hash-library getHash contract). */
inline std::string sha256_hex(std::string_view data) {
    SHA256 h;
    h.add(data.data(), data.size());
    return h.getHash();
}

/** \brief Lowercase-hex MD5. */
inline std::string md5_hex(std::string_view data) {
    MD5 h;
    h.add(data.data(), data.size());
    return h.getHash();
}

/** \brief Lowercase-hex HMAC-SHA256(data, key) — spec arg order; the
 *  hash-library free template takes the key last. */
inline std::string hmac_sha256_hex(std::string_view data, std::string_view key) {
    return hmac<SHA256>(data.data(), data.size(), key.data(), key.size());
}

namespace detail {

// GF(2^128) multiply per NIST SP 800-38D: MSB-first bits of `a`, `v` evolves by
// right-shift with the 0xE1<<?? reduction entering at the top byte. The
// mirrored left-shift form gives wrong tags against published vectors --
// validated against GCM-spec TC4 (76fc6ece...) before landing here.
inline void gmul(const uint8_t a[16], const uint8_t b[16], uint8_t out[16]) {
    uint8_t z[16] = {};
    uint8_t v[16];
    std::memcpy(v, b, 16);
    for (int i = 0; i < 16; ++i) {
        for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
            if (a[i] & mask) {
                for (int k = 0; k < 16; ++k) {
                    z[k] ^= v[k];
                }
            }
            const auto carry = static_cast<bool>(v[15] & 1);
            for (int k = 15; k > 0; --k) {
                v[k] = static_cast<uint8_t>((v[k] >> 1) | (v[k - 1] << 7));
            }
            v[0] = static_cast<uint8_t>((v[0] >> 1) ^ (carry ? 0xE1 : 0));
        }
    }
    std::memcpy(out, z, 16);
}

// GHASH: y = y xor block; y *= H, over zero-padded data then the 16-byte
// (64-bit bitlen A || 64-bit bitlen C) trailer appended by the caller.
inline void ghash(const uint8_t h[16], const uint8_t* data, size_t len, uint8_t y[16]) {
    for (size_t off = 0; off < len; off += 16) {
        uint8_t block[16] = {};
        const size_t n = (len - off < 16) ? len - off : 16;
        std::memcpy(block, data + off, n);
        for (int k = 0; k < 16; ++k) {
            y[k] ^= block[k];
        }
        uint8_t tmp[16];
        gmul(y, h, tmp);
        std::memcpy(y, tmp, 16);
    }
}

inline void inc32(uint8_t ctr[16]) {
    for (int i = 15; i >= 12; --i) {
        if (++ctr[i] != 0) {
            break;
        }
    }
}

// XOR data with the CTR keystream starting at counter block j0.
inline void ctr_xor(AES_ctx& ctx, const uint8_t j0[16], const std::string& in, std::string& out) {
    uint8_t ctr[16];
    std::memcpy(ctr, j0, 16);
    out.clear();
    out.reserve(in.size());
    for (size_t off = 0; off < in.size(); off += 16) {
        inc32(ctr);
        uint8_t ks[16];
        std::memcpy(ks, ctr, 16);
        AES_ECB_encrypt(&ctx, ks);
        const size_t n = (in.size() - off < 16) ? in.size() - off : 16;
        for (size_t k = 0; k < n; ++k) {
            out.push_back(static_cast<char>(in[off + k] ^ ks[k]));
        }
    }
}

} // namespace detail

/** \brief AES-GCM decrypt, hex in (iv, ciphertext, 16-byte tag, key), raw
 *  plaintext out; nil on tag mismatch / malformed input. Key 16/24/32 bytes
 *  as compiled into the pinned tiny-aes; tag length must be 16. */
inline std::optional<std::string> aes_gcm_decrypt(std::string_view iv_hex, std::string_view ct_hex,
                                                  std::string_view tag_hex, std::string_view key_hex) {
    const auto iv = host_codec::hex_decode(std::string(iv_hex));
    const auto ct = host_codec::hex_decode(std::string(ct_hex));
    const auto tag = host_codec::hex_decode(std::string(tag_hex));
    const auto key = host_codec::hex_decode(std::string(key_hex));
    // 256-bit keys only: the pinned tiny-aes is compiled AES256 (single-keylen
    // library); a 16/24-byte key would read past the buffer inside init.
    if (!iv || !ct || !tag || !key || tag->size() != 16 || key->size() != 32) {
        return std::nullopt;
    }

    AES_ctx ctx;
    AES_init_ctx(&ctx, reinterpret_cast<const uint8_t*>(key->data()));

    // H = Ek(0^128).
    uint8_t h[16] = {};
    AES_ECB_encrypt(&ctx, h);

    // J0: 96-bit IV used verbatim with a fixed 1; otherwise GHASH-derived.
    uint8_t j0[16];
    if (iv->size() == 12) {
        std::memcpy(j0, iv->data(), 12);
        j0[12] = 0;
        j0[13] = 0;
        j0[14] = 0;
        j0[15] = 1;
    } else {
        std::memset(j0, 0, 16);
        std::string buf(reinterpret_cast<const char*>(iv->data()), iv->size());
        buf.resize((iv->size() + 15) / 16 * 16 + 16, '\0'); // pad + len block slot
        const uint64_t bits = static_cast<uint64_t>(iv->size()) * 8;
        for (int k = 0; k < 8; ++k) {
            // trailer block = 0^64 || len: same shape as the tag's lenblk
            buf[buf.size() - 8 + k] = static_cast<char>((bits >> (56 - 8 * k)) & 0xFF);
        }
        detail::ghash(h, reinterpret_cast<const uint8_t*>(buf.data()), buf.size(), j0);
    }

    // Tag over AAD(empty)||CT: S = GHASH(CT||pad||len), T = Ek(J0) xor S.
    const size_t ct_len = ct->size();
    std::string gbuf;
    gbuf.resize((ct_len + 15) / 16 * 16 + 16, '\0');
    std::copy(ct->begin(), ct->end(), gbuf.begin());
    const uint64_t bits = static_cast<uint64_t>(ct_len) * 8;
    uint8_t lenblk[16] = {}; // len(A) = 0 (no aad), len(C) big-endian bits
    for (int k = 0; k < 8; ++k) {
        lenblk[15 - k] = static_cast<uint8_t>((bits >> (8 * k)) & 0xFF);
    }
    std::copy(lenblk, lenblk + 16, gbuf.end() - 16);
    uint8_t s[16] = {};
    detail::ghash(h, reinterpret_cast<const uint8_t*>(gbuf.data()), gbuf.size(), s);

    uint8_t expect[16];
    std::memcpy(expect, j0, 16);
    AES_ECB_encrypt(&ctx, expect);
    for (int k = 0; k < 16; ++k) {
        expect[k] ^= s[k];
        // uint8 compare: char sign-extends >=0x80 tag bytes on the != path
        if (expect[k] != static_cast<uint8_t>((*tag)[k])) {
            return std::nullopt; // unverified plaintext never leaves
        }
    }

    std::string pt;
    detail::ctr_xor(ctx, j0, *ct, pt);
    return pt;
}

} // namespace plugin::host_crypto
