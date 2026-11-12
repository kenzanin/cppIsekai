#include "js_internal.hpp"
#include "js_rt.hpp"

#include "plugin/fs.hpp"       // plugin::fs_read -- the fs.read body
#include "plugin/htmltext.hpp" // host.re2/html core (task 1.3)
#include "plugin/url_codec.hpp" // plugin::url_{encode,decode} -- host.url_* body

#include <spdlog/spdlog.h>

#include <ctime>
#include <stdexcept>
#include <vector>

namespace plugin::js {

std::string arg_str(JSContext* ctx, JSValueConst v) {
    std::string out;
    size_t n = 0;
    if (const char* p = JS_ToCStringLen(ctx, &n, v)) {
        out.assign(p, n);
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

namespace {

Instance& self(JSContext* ctx) {
    return *static_cast<Instance*>(JS_GetRuntimeOpaque(JS_GetRuntime(ctx)));
}

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

JSValue js_http_fetch(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "http.fetch(url, opts)");
    }
    Instance& inst = self(ctx);
    if (!inst.services.http_fetch) {
        return JS_ThrowTypeError(ctx, "http unavailable");
    }
    const std::string url = arg_str(ctx, argv[0]);
    std::string opts = "{}";
    if (argc > 1 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        opts = json_stringify(ctx, argv[1]);
    }
    const auto res = inst.services.http_fetch(url, opts);
    if (!res) {
        return JS_NULL; // parity: lua nil on failure
    }
    JSValue parsed = JS_ParseJSON(ctx, res->data(), res->size(), "<fetch>");
    if (JS_IsException(parsed)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        return JS_NULL;
    }
    return parsed; // {status, body, headers} -- the lua-shaped reply
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
    if (rel.rfind("./", 0) == 0) rel = rel.substr(2);  // ./util -> util
    for (auto& c : rel) if (c == '.') c = '/';          // dotted form a.b -> a/b
    if (!rel.empty() && rel[0] == '/') rel = rel.substr(1);
    // strip leading "./" pieces and reject escapes
    {
        size_t pos = 0;
        while (true) {
            const size_t slash = rel.find('/', pos);
            const std::string seg = rel.substr(pos, slash == std::string::npos ? slash : slash - pos);
            if (seg.empty() || seg == "." || seg == "..") {
                JS_FreeValue(ctx, g); JS_FreeValue(ctx, cache);
                return JS_ThrowTypeError(ctx, "require: bad module name");
            }
            if (slash == std::string::npos) break;
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
        JS_FreeValue(ctx, g); JS_FreeValue(ctx, cache);
        return JS_ThrowReferenceError(ctx, "require: module '%s' not found", name.c_str());
    }
    // CommonJS wrapper: (function(exports, module, require) { ... })
    const std::string wrapped = "(function(exports, module, require){\n" + *src + "\n})";
    JSValue fn = JS_Eval(ctx, wrapped.data(), wrapped.size(), res_name.c_str(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(fn)) {
        JS_FreeValue(ctx, g); JS_FreeValue(ctx, cache);
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
    for (auto& a : args) JS_FreeValue(ctx, a);
    JS_FreeValue(ctx, fn);
    if (JS_IsException(ret)) {
        JS_FreeValue(ctx, exports); JS_FreeValue(ctx, module); JS_FreeValue(ctx, g); JS_FreeValue(ctx, cache);
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

JSValue js_storage_get(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "storage.get(key)");
    }
    Instance& inst = self(ctx);
    if (!inst.services.state_get) {
        return JS_NULL;
    }
    const auto v = inst.services.state_get(inst.id, arg_str(ctx, argv[0]));
    if (!v) {
        return JS_NULL;
    }
    return JS_NewStringLen(ctx, v->data(), v->size());
}

JSValue js_storage_set(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "storage.set(key, value)");
    }
    Instance& inst = self(ctx);
    if (inst.services.state_set) {
        inst.services.state_set(inst.id, arg_str(ctx, argv[0]), arg_str(ctx, argv[1]));
    }
    return JS_UNDEFINED;
}

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
        return JS_ThrowTypeError(ctx, e.what());
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
        return JS_ThrowTypeError(ctx, e.what());
    }
}

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

// host.url_*: one shared codec core (plugin/url_codec.hpp), thin marshal.
// Strings only, exactly one arg; anything else is a TypeError (host-surface
// parity decision, design of js-wasm-host-parity).
JSValue js_url_encode(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "url_encode(string)");
    }
    const std::string out = plugin::url_encode(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

JSValue js_url_decode(JSContext* ctx, JSValueConst /*this*/, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "url_decode(string)");
    }
    const std::string out = plugin::url_decode(arg_str(ctx, argv[0]));
    return JS_NewStringLen(ctx, out.data(), out.size());
}

} // namespace

void bind_host_globals(JSContext* c) {
    JSValue g = JS_GetGlobalObject(c);
    const auto bind = [&](JSValue obj, const char* name, JSValue fn) {
        JS_SetPropertyStr(c, obj, name, fn); // consumes the reference
    };

    // One namespaced surface: everything the host provides lives under
    // `host.` (parity with the lua/wasm tables; no bare globals).
    JSValue host = JS_NewObject(c);

    bind(host, "now", JS_NewCFunction(c, js_now, "now", 0));

    JSValue log = JS_NewObject(c);
    bind(log, "info", JS_NewCFunctionMagic(c, js_log, "info", 1, JS_CFUNC_generic_magic, 0));
    bind(log, "error", JS_NewCFunctionMagic(c, js_log, "error", 1, JS_CFUNC_generic_magic, 1));
    bind(host, "log", log);

    JSValue http = JS_NewObject(c);
    bind(http, "fetch", JS_NewCFunction(c, js_http_fetch, "fetch", 2));
    bind(host, "http", http);

    JSValue fs = JS_NewObject(c);
    bind(fs, "read", JS_NewCFunction(c, js_fs_read, "read", 1));
    bind(host, "fs", fs);

    // require(): CommonJS-style, confined to the plugin dir; modules cached
    // on globalThis.__modules so repeat requires are free.
    bind(g, "require", JS_NewCFunction(c, js_require, "require", 1));

    JSValue storage = JS_NewObject(c);
    bind(storage, "get", JS_NewCFunction(c, js_storage_get, "get", 1));
    bind(storage, "set", JS_NewCFunction(c, js_storage_set, "set", 2));
    bind(host, "storage", storage);

    // Host-side heavy primitives (design D4 one core, thin marshals): RE2
    // (linear-time, no ReDoS) and lexbor DOM selectors. Bad patterns throw
    // (loud); no-match / malformed HTML return [].
    JSValue re2 = JS_NewObject(c);
    bind(re2, "find", JS_NewCFunction(c, js_re2_find, "find", 2));
    bind(re2, "match", JS_NewCFunction(c, js_re2_match, "match", 2));
    bind(host, "re2", re2);

    JSValue html = JS_NewObject(c);
    bind(html, "select", JS_NewCFunction(c, js_html_select, "select", 2));
    bind(html, "attr", JS_NewCFunction(c, js_html_attr, "attr", 3));
    bind(host, "html", html);

    // Query-component codec, same core lua's host.url_* delegate to.
    bind(host, "url_encode", JS_NewCFunction(c, js_url_encode, "url_encode", 1));
    bind(host, "url_decode", JS_NewCFunction(c, js_url_decode, "url_decode", 1));

    bind(g, "host", host); // consumes the host reference tree

    JS_FreeValue(c, g);
}

} // namespace plugin::js
