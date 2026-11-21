/** \file imports/html.cpp
 *  \brief env.html_select / html_attr / html_xpath -- JSON string arrays.
 */

#include "host_imports.hpp"

#include "plugin/host_xpath.hpp"
#include "plugin/htmltext.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace plugin::wasm::imports {
namespace {

const void* html_call(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*, bool want_attr) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t html_off = arg_i32(args);
    const uint32_t html_len = arg_i32(args);
    const uint32_t sel_off = arg_i32(args);
    const uint32_t sel_len = arg_i32(args);
    uint32_t attr_off = 0, attr_len = 0;
    if (want_attr) {
        attr_off = arg_i32(args);
        attr_len = arg_i32(args);
    }
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto html = guest_str(self, html_off, html_len);
    const auto sel = guest_str(self, sel_off, sel_len);
    const auto attr = want_attr ? guest_str(self, attr_off, attr_len) : std::optional<std::string>{};
    if (html && sel && (!want_attr || attr)) {
        // JSON string array; malformed HTML / no match => [] (lenient).
        const auto vals = want_attr ? plugin::html_attr(*html, *sel, *attr) : plugin::html_select(*html, *sel);
        std::string json = "[";
        for (size_t i = 0; i < vals.size(); ++i) {
            if (i != 0) {
                json += ',';
            }
            json += '"';
            for (const char c : vals[i]) {
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
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

} // namespace

const void* host_html_select(IM3Runtime rt, IM3ImportContext ctx, uint64_t* sp, void* mem) {
    return html_call(rt, ctx, sp, mem, /*want_attr=*/false);
}

const void* host_html_attr(IM3Runtime rt, IM3ImportContext ctx, uint64_t* sp, void* mem) {
    return html_call(rt, ctx, sp, mem, /*want_attr=*/true);
}

// P8/P9: host.html.xpath(expr, html) -- values as a JSON string array (house
// list convention); kBadPattern on xpath error, kNotFound on guest faults.
const void* host_html_xpath(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t expr_off = arg_i32(args);
    const uint32_t expr_len = arg_i32(args);
    const uint32_t html_off = arg_i32(args);
    const uint32_t html_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto expr = guest_str(self, expr_off, expr_len);
    const auto doc = guest_str(self, html_off, html_len);
    if (expr && doc) {
        const auto vals = plugin::host_xpath::xpath(*doc, *expr);
        if (!vals) {
            rc = kBadPattern; // loud: bad expr is the plugin's bug
        } else {
            std::string json = "[";
            for (size_t i = 0; i < vals->size(); ++i) {
                if (i != 0) {
                    json += ',';
                }
                json += '"';
                for (const char c : (*vals)[i]) {
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
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

} // namespace plugin::wasm::imports
