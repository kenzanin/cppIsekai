/** \file imports/re2_edit.cpp
 *  \brief env.re2_{replace,quote,find_index} -- thin marshals over
 *  plugin::htmltext; results written into guest memory via guest_write.
 */

#include "host_imports.hpp"

#include "plugin/htmltext.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace plugin::wasm::imports {

const void* host_re2_replace(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t pat_off = arg_i32(args);
    const uint32_t pat_len = arg_i32(args);
    const uint32_t rep_off = arg_i32(args);
    const uint32_t rep_len = arg_i32(args);
    const uint32_t txt_off = arg_i32(args);
    const uint32_t txt_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto pat = guest_str(self, pat_off, pat_len);
    const auto rep = guest_str(self, rep_off, rep_len);
    const auto txt = guest_str(self, txt_off, txt_len);
    if (pat && rep && txt) {
        try {
            const std::string result = plugin::re2_replace(*pat, *rep, *txt);
            if (guest_write(self, out, cap, result)) {
                rc = static_cast<int32_t>(result.size());
            } else {
                rc = kTruncated;
            }
        } catch (const std::invalid_argument&) {
            rc = kBadPattern;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_re2_quote(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t s_off = arg_i32(args);
    const uint32_t s_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto s = guest_str(self, s_off, s_len)) {
        const std::string result = plugin::re2_quote(*s);
        if (guest_write(self, out, cap, result)) {
            rc = static_cast<int32_t>(result.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_re2_find_index(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t pat_off = arg_i32(args);
    const uint32_t pat_len = arg_i32(args);
    const uint32_t txt_off = arg_i32(args);
    const uint32_t txt_len = arg_i32(args);

    int32_t rc = 0; // 0 = not found
    const auto pat = guest_str(self, pat_off, pat_len);
    const auto txt = guest_str(self, txt_off, txt_len);
    if (pat && txt) {
        try {
            rc = static_cast<int32_t>(plugin::re2_find_index(*pat, *txt));
        } catch (const std::invalid_argument&) {
            rc = kBadPattern;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

} // namespace plugin::wasm::imports
