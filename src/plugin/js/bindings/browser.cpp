/** \file bindings/browser.cpp
 *  \brief host.browser.call -- app-local CDP tool RPC.
 */

#include "host_bindings.hpp"

#include "browser/chrome.hpp"

#include <spdlog/spdlog.h>

#include <string>

namespace plugin::js::bindings {

// host.browser.call (browser-host D5): JSON-string args in, tool text out;
// null with a warn on transport/RPC/isError failure -- lua pair parity.
JSValue js_browser_call(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
        return JS_ThrowTypeError(ctx, "browser.call(tool, args_json)");
    }
    std::string err;
    const std::string out = ::browser::call(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]), &err);
    if (out.empty() && !err.empty()) {
        spdlog::warn("[plugin:{}] browser.call failed: {}", self(ctx).id, err);
        return JS_NULL;
    }
    return JS_NewStringLen(ctx, out.data(), out.size());
}

} // namespace plugin::js::bindings
