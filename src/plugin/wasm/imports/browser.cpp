/** \file imports/browser.cpp
 *  \brief env.browser_call (browser-host D5) -- app-local CDP tools.
 */

#include "host_imports.hpp"

#include "browser/chrome.hpp"

#include <cstdint>
#include <string>

namespace plugin::wasm::imports {

// (tool_off,tool_len,args_off,args_len,out,cap) -> len; kNotFound covers
// tool/RPC/transport failure, kTruncated on cap.
const void* host_browser_call(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t tool_off = arg_i32(args);
    const uint32_t tool_len = arg_i32(args);
    const uint32_t args_off = arg_i32(args);
    const uint32_t args_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto tool = guest_str(self, tool_off, tool_len);
    const auto call_args = guest_str(self, args_off, args_len);
    if (tool && call_args) {
        std::string err;
        std::string res = ::browser::call(*tool, *call_args, &err);
        if (!res.empty() || err.empty()) {
            if (guest_write(self, out, cap, res)) {
                rc = static_cast<int32_t>(res.size());
            } else {
                rc = kTruncated;
            }
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

} // namespace plugin::wasm::imports
