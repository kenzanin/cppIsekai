/** \file bindings/core.cpp
 *  \brief Shared marshals (arg_str/json_stringify) plus now / log / require /
 *  fs.read / plugin.param.
 */

#include "host_bindings.hpp"

#include "plugin/fs.hpp" // plugin::fs_read -- the fs.read body

#include <spdlog/spdlog.h>

#include <ctime>
#include <stdexcept>
#include <string>

namespace plugin::js {

std::string arg_str(JSContext* ctx, JSValueConst v) {
    std::string out;
    size_t n = 0;
    if (const char* p = JS_ToCStringLen(ctx, &n, v)) {
        out.assign(p, n);
        JS_FreeCString(ctx, p); // quickjs hands back an owned copy
    }
    return out;
}

// JSON.stringify via the global object (no C-API shape bets); "" on failure.
std::string json_stringify(JSContext* ctx, JSValueConst v) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue json = JS_GetPropertyStr(ctx, g, "JSON");
    JSValue stringify = JS_GetPropertyStr(ctx, json, "stringify");
    JSValue args[] = {v};
    JSValue ret = JS_Call(ctx, stringify, JS_UNDEFINED, 1, args);
    std::string out;
    if (!JS_IsException(ret)) {
        out = arg_str(ctx, ret);
    } else {
        JS_FreeValue(ctx, JS_GetException(ctx));
    }
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, stringify);
    JS_FreeValue(ctx, json);
    JS_FreeValue(ctx, g);
    return out;
}

namespace bindings {

JSValue js_now(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* /*argv*/) {
    if (argc != 0) {
        return JS_ThrowTypeError(ctx, "now takes no arguments");
    }
    return JS_NewInt64(ctx, static_cast<int64_t>(time(nullptr)));
}

JSValue js_log(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv, int magic) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "log.info(message)");
    }
    const std::string msg = arg_str(ctx, argv[0]);
    if (magic == 0) {
        spdlog::info("[plugin:{}] {}", self(ctx).id, msg);
    } else {
        spdlog::error("[plugin:{}] {}", self(ctx).id, msg);
    }
    return JS_UNDEFINED;
}

JSValue js_plugin_param(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "plugin.param(key)");
    }
    const auto& inst = self(ctx);
    const std::string key = arg_str(ctx, argv[0]);
    const auto it = inst.services.params.find(key);
    if (it == inst.services.params.end()) {
        return JS_NULL;
    }
    return JS_NewStringLen(ctx, it->second.data(), it->second.size());
}

JSValue js_require(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "require(path)");
    }
    Instance& inst = self(ctx);
    const std::string name = arg_str(ctx, argv[0]);
    if (name.empty()) {
        return JS_ThrowTypeError(ctx, "require: empty module name");
    }
    // cache on the realm's global object: globalThis.__modules
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue cache = JS_GetPropertyStr(ctx, g, "__modules");
    if (JS_IsUndefined(cache)) {
        JS_FreeValue(ctx, cache);
        cache = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, g, "__modules", JS_DupValue(ctx, cache));
    }
    JSValue cached = JS_GetPropertyStr(ctx, cache, name.c_str());
    if (!JS_IsUndefined(cached)) {
        JS_FreeValue(ctx, g);
        JS_FreeValue(ctx, cache);
        return cached; // dup'd below via GetPropertyStr refcount semantics: JS_GetPropertyStr returns new ref
    }
    JS_FreeValue(ctx, cached);

    // resolve relative to plugin dir: "./x" / "x/y" / "a.b" -> x/y.js (try .js, /index.js)
    std::string rel = name;
    if (rel.starts_with("./")) {
        rel = rel.substr(2); // ./util -> util
    }
    for (auto& c : rel) {
        if (c == '.') {
            c = '/'; // dotted form a.b -> a/b
        }
    }
    if (!rel.empty() && rel[0] == '/') {
        rel = rel.substr(1);
    }
    // strip leading "./" pieces and reject escapes
    {
        size_t pos = 0;
        while (true) {
            const size_t slash = rel.find('/', pos);
            const std::string seg = rel.substr(pos, slash == std::string::npos ? slash : slash - pos);
            if (seg.empty() || seg == "." || seg == "..") {
                JS_FreeValue(ctx, g);
                JS_FreeValue(ctx, cache);
                return JS_ThrowTypeError(ctx, "require: bad module name");
            }
            if (slash == std::string::npos) {
                break;
            }
            pos = slash + 1;
        }
    }
    std::optional<std::string> src = plugin::fs_read(inst.dir, rel + ".js");
    std::string res_name = rel + ".js";
    if (!src) {
        src = plugin::fs_read(inst.dir, rel + "/index.js");
        res_name = rel + "/index.js";
    }
    if (!src) {
        JS_FreeValue(ctx, g);
        JS_FreeValue(ctx, cache);
        return JS_ThrowReferenceError(ctx, "require: module '%s' not found", name.c_str());
    }
    // CommonJS wrapper: (function(exports, module, require) { ... })
    const std::string wrapped = "(function(exports, module, require){\n" + *src + "\n})";
    JSValue fn = JS_Eval(ctx, wrapped.data(), wrapped.size(), res_name.c_str(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(fn)) {
        JS_FreeValue(ctx, g);
        JS_FreeValue(ctx, cache);
        return JS_Throw(ctx, JS_GetException(ctx)); // propagate compile error
    }
    JSValue module = JS_NewObject(ctx);
    JSValue exports = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, module, "exports", JS_DupValue(ctx, exports));
    // cache placeholder before running (circular requires)
    JSValue require_fn = JS_GetPropertyStr(ctx, g, "require");
    JSValue args[3] = {JS_DupValue(ctx, exports), JS_DupValue(ctx, module), require_fn};
    JS_SetPropertyStr(ctx, cache, name.c_str(), JS_DupValue(ctx, exports)); // may be replaced by module.exports below
    JSValue ret = JS_Call(ctx, fn, JS_UNDEFINED, 3, args);
    for (auto& a : args) {
        JS_FreeValue(ctx, a);
    }
    JS_FreeValue(ctx, fn);
    if (JS_IsException(ret)) {
        JS_FreeValue(ctx, exports);
        JS_FreeValue(ctx, module);
        JS_FreeValue(ctx, g);
        JS_FreeValue(ctx, cache);
        return JS_Throw(ctx, JS_GetException(ctx));
    }
    JS_FreeValue(ctx, ret);
    JSValue module_exports = JS_GetPropertyStr(ctx, module, "exports");
    JS_SetPropertyStr(ctx, cache, name.c_str(), JS_DupValue(ctx, module_exports)); // overwrite placeholder
    JS_FreeValue(ctx, exports);
    JS_FreeValue(ctx, module);
    JS_FreeValue(ctx, g);
    JS_FreeValue(ctx, cache);
    return module_exports;
}

JSValue js_fs_read(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "fs.read(path)");
    }
    const std::string rel = arg_str(ctx, argv[0]);
    const auto data = plugin::fs_read(self(ctx).dir, rel);
    if (!data) {
        return JS_NULL;
    }
    return JS_NewStringLen(ctx, data->data(), data->size());
}

} // namespace bindings

} // namespace plugin::js
