/** \file imports/re2_match.cpp
 *  \brief env.re2_{match,find,find_all} -- one core (plugin::htmltext),
 *  results as JSON arrays written into guest memory via the same guest_write
 *  convention as fs_read/storage.
 */

#include "host_imports.hpp"

#include "plugin/htmltext.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace plugin::wasm::imports {
namespace {

const void* re2_call(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*, bool full) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t pat_off = arg_i32(args);
    const uint32_t pat_len = arg_i32(args);
    const uint32_t txt_off = arg_i32(args);
    const uint32_t txt_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto pat = guest_str(self, pat_off, pat_len);
    const auto txt = guest_str(self, txt_off, txt_len);
    if (pat && txt) {
        try {
            const auto groups = full ? plugin::re2_match(*pat, *txt) : plugin::re2_find(*pat, *txt);
            // JSON array of groups; empty list on no-match (never an error).
            std::string json = "[";
            for (size_t i = 0; i < groups.size(); ++i) {
                if (i != 0) {
                    json += ',';
                }
                json += '"';
                for (const char c : groups[i]) {
                    if (c == '"' || c == '\\') {
                        json += '\\';
                    }
                    json += c;
                }
                json += '"';
            }
            json += ']';
            if (guest_write(self, out, cap, json)) {
                rc = static_cast<int32_t>(json.size());
            } else {
                rc = kTruncated;
            }
        } catch (const std::invalid_argument&) {
            rc = kBadPattern; // loud: bad pattern is the plugin's bug
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

} // namespace

const void* host_re2_match(IM3Runtime rt, IM3ImportContext ctx, uint64_t* sp, void* mem) {
    return re2_call(rt, ctx, sp, mem, /*full=*/true);
}

const void* host_re2_find(IM3Runtime rt, IM3ImportContext ctx, uint64_t* sp, void* mem) {
    return re2_call(rt, ctx, sp, mem, /*full=*/false);
}

const void* host_re2_find_all(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t pat_off = arg_i32(args);
    const uint32_t pat_len = arg_i32(args);
    const uint32_t txt_off = arg_i32(args);
    const uint32_t txt_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto pat = guest_str(self, pat_off, pat_len);
    const auto txt = guest_str(self, txt_off, txt_len);
    if (pat && txt) {
        try {
            const auto all = plugin::re2_find_all(*pat, *txt);
            // JSON: [[g0,g1,...], [g0,g1,...], ...] -- array of arrays of strings
            std::string json = "[";
            for (size_t i = 0; i < all.size(); ++i) {
                if (i != 0) {
                    json += ',';
                }
                json += '[';
                for (size_t j = 0; j < all[i].size(); ++j) {
                    if (j != 0) {
                        json += ',';
                    }
                    json += '"';
                    for (const char c : all[i][j]) {
                        if (c == '"' || c == '\\') {
                            json += '\\';
                        }
                        json += c;
                    }
                    json += '"';
                }
                json += ']';
            }
            json += ']';
            if (guest_write(self, out, cap, json)) {
                rc = static_cast<int32_t>(json.size());
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

} // namespace plugin::wasm::imports
