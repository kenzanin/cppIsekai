/** \file imports/text.cpp
 *  \brief env.text_* -- status normalization, chapter-number extraction,
 *  date->ISO (P2, host_text.hpp -- one core, six thin marshals; design D4).
 */

#include "host_imports.hpp"

#include "plugin/host_text.hpp"

#include <bit>
#include <cstdint>
#include <string>

namespace plugin::wasm::imports {

const void* host_text_normalize_status(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_text::normalize_status(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_text_chapter_num(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);

    // Return slot: i64 (f64 bitcast). Write 0.0f when no number / volume-only.
    int64_t f64_bits = 0;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const auto v = plugin::host_text::chapter_num(*in);
        if (v) {
            const double d = *v;
            const auto bits = std::bit_cast<int64_t>(d);
            f64_bits = bits;
        }
        // No number / volume-only → 0.0f (bits=0) — parity with Go's 0.0.
    }
    *reinterpret_cast<int64_t*>(sp) = f64_bits;
    return m3Err_none;
}

const void* host_text_date_to_iso(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        if (const auto res = plugin::host_text::date_to_iso(*in)) {
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

const void* host_text_trim(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_text::trim(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_text_unescape(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_text::unescape(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_text_clean(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_text::clean(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_text_strip_html(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t in_off = arg_i32(args);
    const uint32_t in_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto in = guest_str(self, in_off, in_len)) {
        const std::string res = plugin::host_text::strip_html(*in);
        if (guest_write(self, out, cap, res)) {
            rc = static_cast<int32_t>(res.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

} // namespace plugin::wasm::imports
