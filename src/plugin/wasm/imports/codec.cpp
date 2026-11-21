/** \file imports/codec.cpp
 *  \brief env.codec_* (P6): base64/hex, text-in/text-out like the text
 *  imports; decode kNotFound on invalid, the house optional convention.
 */

#include "host_imports.hpp"

#include "plugin/host_codecs.hpp"

#include <cstdint>
#include <string>

namespace plugin::wasm::imports {

const void* host_codec_base64_encode(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_codec::base64_encode(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_codec_base64_decode(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        if (const auto res = plugin::host_codec::base64_decode(*in)) {
            if (guest_write(self, out, cap, *res)) {
                rc = static_cast<int32_t>(res->size());
            } else {
                rc = kTruncated;
            }
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_codec_hex_encode(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_codec::hex_encode(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_codec_hex_decode(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        if (const auto res = plugin::host_codec::hex_decode(*in)) {
            if (guest_write(self, out, cap, *res)) {
                rc = static_cast<int32_t>(res->size());
            } else {
                rc = kTruncated;
            }
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

} // namespace plugin::wasm::imports
