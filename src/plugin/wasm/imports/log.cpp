/** \file imports/log.cpp
 *  \brief env.now / env.log_info.
 */

#include "host_imports.hpp"

#include <spdlog/spdlog.h>

#include <ctime>
#include <string>

namespace plugin::wasm::imports {

const void* host_now(IM3Runtime, IM3ImportContext, uint64_t* sp, void*) {
    *reinterpret_cast<int64_t*>(sp) = static_cast<int64_t>(std::time(nullptr));
    return m3Err_none;
}

const void* host_log_info(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    const uint32_t off = arg_i32(sp);
    const uint32_t len = arg_i32(sp);
    const auto msg = guest_str(self, off, len);
    if (!msg) {
        return m3Err_trapOutOfBoundsMemoryAccess;
    }
    // Same route as lua log.info: default logger, whose sinks feed the log page.
    spdlog::info("[plugin:{}] {}", self.id, *msg);
    return m3Err_none;
}

} // namespace plugin::wasm::imports
