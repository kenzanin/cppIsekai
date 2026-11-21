/** \file imports/crypto.cpp
 *  \brief env.crypto_* (P7): hex in/hex out like the codec imports; kNotFound
 *  on hex/tag/key failure -- unverified plaintext never leaves.
 */

#include "host_imports.hpp"

#include "plugin/host_crypto.hpp"

#include <cstdint>
#include <string>

namespace plugin::wasm::imports {

const void* host_crypto_sha256_hex(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_crypto::sha256_hex(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_crypto_md5_hex(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_crypto::md5_hex(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_crypto_hmac_sha256_hex(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t data_off = arg_i32(args);
    const uint32_t data_len = arg_i32(args);
    const uint32_t key_off = arg_i32(args);
    const uint32_t key_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto data = guest_str(self, data_off, data_len);
    const auto key = guest_str(self, key_off, key_len);
    if (data && key) {
        const std::string res = plugin::host_crypto::hmac_sha256_hex(*data, *key);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_crypto_aes_gcm_decrypt(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t iv_off = arg_i32(args);
    const uint32_t iv_len = arg_i32(args);
    const uint32_t ct_off = arg_i32(args);
    const uint32_t ct_len = arg_i32(args);
    const uint32_t tag_off = arg_i32(args);
    const uint32_t tag_len = arg_i32(args);
    const uint32_t key_off = arg_i32(args);
    const uint32_t key_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto iv = guest_str(self, iv_off, iv_len);
    const auto ct = guest_str(self, ct_off, ct_len);
    const auto tag = guest_str(self, tag_off, tag_len);
    const auto key = guest_str(self, key_off, key_len);
    if (iv && ct && tag && key) {
        if (const auto pt = plugin::host_crypto::aes_gcm_decrypt(*iv, *ct, *tag, *key)) {
            if (guest_write(self, out, cap, *pt)) {
                rc = static_cast<int32_t>(pt->size());
            } else {
                rc = kTruncated;
            }
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

} // namespace plugin::wasm::imports
