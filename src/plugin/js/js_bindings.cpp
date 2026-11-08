#include "js_rt.hpp"
#include "js_internal.hpp"

#include "plugin/fs.hpp" // plugin::fs_read -- the fs.read body

#include <spdlog/spdlog.h>

#include <ctime>

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

} // namespace

void bind_host_globals(JSContext* c) {
    JSValue g = JS_GetGlobalObject(c);
    const auto bind = [&](JSValue obj, const char* name, JSValue fn) {
        JS_SetPropertyStr(c, obj, name, fn); // consumes the reference
    };

    bind(g, "now", JS_NewCFunction(c, js_now, "now", 0));

    JSValue log = JS_NewObject(c);
    bind(log, "info", JS_NewCFunctionMagic(c, js_log, "info", 1, JS_CFUNC_generic_magic, 0));
    bind(log, "error", JS_NewCFunctionMagic(c, js_log, "error", 1, JS_CFUNC_generic_magic, 1));
    bind(g, "log", log);

    JSValue http = JS_NewObject(c);
    bind(http, "fetch", JS_NewCFunction(c, js_http_fetch, "fetch", 2));
    bind(g, "http", http);

    JSValue fs = JS_NewObject(c);
    bind(fs, "read", JS_NewCFunction(c, js_fs_read, "read", 1));
    bind(g, "fs", fs);

    JSValue storage = JS_NewObject(c);
    bind(storage, "get", JS_NewCFunction(c, js_storage_get, "get", 1));
    bind(storage, "set", JS_NewCFunction(c, js_storage_set, "set", 2));
    bind(g, "storage", storage);

    JS_FreeValue(c, g);
}

} // namespace plugin::js
