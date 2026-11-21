/** \file bindings/html.cpp
 *  \brief host.html.* -- lexbor css select/attr plus xpath.
 */

#include "host_bindings.hpp"

#include "plugin/host_xpath.hpp"
#include "plugin/htmltext.hpp" // html select/attr core (task 1.3)

#include <cstdint>
#include <string>
#include <vector>

namespace plugin::js::bindings {

JSValue js_html_select(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "html.select(html, css)");
    }
    const std::vector<std::string> texts = html_select(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]));
    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < texts.size(); ++i) {
        JS_SetPropertyUint32(ctx, arr, i, JS_NewStringLen(ctx, texts[i].data(), texts[i].size()));
    }
    return arr;
}

JSValue js_html_attr(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 3) {
        return JS_ThrowTypeError(ctx, "html.attr(html, css, attr)");
    }
    const std::vector<std::string> vals =
        html_attr(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]), arg_str(ctx, argv[2]));
    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < vals.size(); ++i) {
        JS_SetPropertyUint32(ctx, arr, i, JS_NewStringLen(ctx, vals[i].data(), vals[i].size()));
    }
    return arr;
}

// P8/P9: host.html.xpath(expr, html) -- array of string values (attr value /
// text), null on xpath or parse error.
JSValue js_html_xpath(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
        return JS_ThrowTypeError(ctx, "html.xpath(expr, html)");
    }
    const auto vals = plugin::host_xpath::xpath(arg_str(ctx, argv[1]), arg_str(ctx, argv[0]));
    if (!vals) {
        return JS_NULL;
    }
    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < vals->size(); ++i) {
        JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i),
                             JS_NewStringLen(ctx, (*vals)[i].data(), (*vals)[i].size()));
    }
    return arr;
}

} // namespace plugin::js::bindings
