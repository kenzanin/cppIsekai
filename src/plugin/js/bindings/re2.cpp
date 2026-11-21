/** \file bindings/re2.cpp
 *  \brief host.re2.* -- RE2 core (plugin/htmltext.hpp) behind thin marshals.
 */

#include "host_bindings.hpp"

#include "plugin/htmltext.hpp" // host.re2 core (task 1.3)

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace plugin::js::bindings {

// Host-side heavy primitives (design D4 one core, thin marshals). RE2 throws
// on bad patterns (loud); html helpers return [] on no-match / bad HTML.
// An empty group list is returned as []; a failed group (RE2 group skipped)
// is represented as null for positional parity with lua/wasm.

JSValue js_re2_match(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "re2.match(pattern, text)");
    }
    try {
        const std::vector<std::string> groups = re2_match(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]));
        JSValue arr = JS_NewArray(ctx);
        for (size_t i = 0; i < groups.size(); ++i) {
            JS_SetPropertyUint32(ctx, arr, i, JS_NewStringLen(ctx, groups[i].data(), groups[i].size()));
        }
        return arr;
    } catch (const std::invalid_argument& e) {
        return JS_ThrowTypeError(ctx, "%s", e.what());
    }
}

JSValue js_re2_find(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "re2.find(pattern, text)");
    }
    try {
        const std::vector<std::string> groups = re2_find(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]));
        JSValue arr = JS_NewArray(ctx);
        for (size_t i = 0; i < groups.size(); ++i) {
            JS_SetPropertyUint32(ctx, arr, i, JS_NewStringLen(ctx, groups[i].data(), groups[i].size()));
        }
        return arr;
    } catch (const std::invalid_argument& e) {
        return JS_ThrowTypeError(ctx, "%s", e.what());
    }
}

// P4: find_all, replace, quote, find_index (Lua-only gmatch has no JS analogue;
// JS callers iterate re2.find_all directly.)
JSValue js_re2_find_all(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "re2.find_all(pattern, text)");
    }
    try {
        const auto all = re2_find_all(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]));
        JSValue arr = JS_NewArray(ctx);
        for (size_t i = 0; i < all.size(); ++i) {
            JSValue row = JS_NewArray(ctx);
            for (size_t j = 0; j < all[i].size(); ++j) {
                JS_SetPropertyUint32(ctx, row, j, JS_NewStringLen(ctx, all[i][j].data(), all[i][j].size()));
            }
            JS_SetPropertyUint32(ctx, arr, i, row);
        }
        return arr;
    } catch (const std::invalid_argument& e) {
        return JS_ThrowTypeError(ctx, "%s", e.what());
    }
}

JSValue js_re2_replace(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 3) {
        return JS_ThrowTypeError(ctx, "re2.replace(pattern, repl, text)");
    }
    try {
        const std::string out = re2_replace(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]), arg_str(ctx, argv[2]));
        return JS_NewStringLen(ctx, out.data(), out.size());
    } catch (const std::invalid_argument& e) {
        return JS_ThrowTypeError(ctx, "%s", e.what());
    }
}

JSValue js_re2_quote(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "re2.quote(str)");
    }
    const std::string out = re2_quote(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

JSValue js_re2_find_index(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "re2.find_index(pattern, text)");
    }
    try {
        const size_t idx = re2_find_index(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]));
        return JS_NewInt64(ctx, static_cast<int64_t>(idx)); // 0 = not found
    } catch (const std::invalid_argument& e) {
        return JS_ThrowTypeError(ctx, "%s", e.what());
    }
}

// plugin-host-promotion D2: single-value capture; null on no match.
JSValue js_re2_capture(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "re2.capture(pattern, text)");
    }
    try {
        const auto v = re2_capture(arg_str(ctx, argv[0]), arg_str(ctx, argv[1]));
        if (!v) {
            return JS_NULL;
        }
        return JS_NewStringLen(ctx, v->data(), v->size());
    } catch (const std::invalid_argument& e) {
        return JS_ThrowTypeError(ctx, "%s", e.what());
    }
}

} // namespace plugin::js::bindings
