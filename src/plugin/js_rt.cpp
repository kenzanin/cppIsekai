#include "js_rt.hpp"

#include "fs.hpp"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <ctime>
#include <utility>

namespace plugin::js {
namespace {

constexpr long long kOpsPerCall = 200000; // same scale as the lua instruction budget
constexpr long long kOpsChunk = 512;

int budget_interrupt(JSContext* /*ctx*/, void* opaque) {
    auto& ops = *static_cast<long long*>(opaque);
    ops -= kOpsChunk;
    return ops > 0 ? 0 : 1; // 1 = stop the script
}

Instance& self(JSContext* ctx) {
    return *static_cast<Instance*>(JS_GetRuntimeOpaque(JS_GetRuntime(ctx)));
}

// Borrow variant: for argument cells and values freed by the caller.
std::string arg_str(JSContext* ctx, JSValueConst v) {
    std::string out;
    size_t n = 0;
    if (const char* p = JS_ToCStringLen(ctx, &n, v)) {
        out.assign(p, n);
    }
    return out;
}

std::string exception_msg(JSContext* ctx) {
    JSValue e = JS_GetException(ctx);
    const std::string out = arg_str(ctx, e); // ToString the error
    JS_FreeValue(ctx, e);
    return out.empty() ? "unknown error" : out;
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

Instance::~Instance() {
    if (ctx != nullptr) {
        JS_FreeContext(ctx);
    }
    if (rt != nullptr) {
        JS_FreeRuntime(rt);
    }
}

Loaded load(std::string id, std::string dir, std::string entry_src, wasm::Services services) {
    Loaded out;
    auto inst = std::make_unique<Instance>();
    inst->id = std::move(id);
    inst->dir = std::move(dir);
    inst->services = std::move(services);
    const auto fail = [&out](std::string msg) {
        out.error = std::move(msg);
        return std::move(out);
    };

    inst->rt = JS_NewRuntime();
    if (inst->rt == nullptr) {
        return fail("js: runtime alloc failed");
    }
    inst->ctx = JS_NewContext(inst->rt);
    if (inst->ctx == nullptr) {
        return fail("js: context alloc failed");
    }
    JS_SetRuntimeOpaque(inst->rt, inst.get());
    JS_SetInterruptHandler(inst->rt, budget_interrupt, &inst->ops_left);
    inst->ops_left = kOpsPerCall; // the entry eval charges too

    JSContext* c = inst->ctx;
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

    JSValue ret = JS_Eval(c, entry_src.data(), entry_src.size(), "<entry>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(ret)) {
        return fail("js: entry: " + exception_msg(c));
    }
    JS_FreeValue(c, ret);

    out.instance = std::move(inst);
    return out;
}

wasm::RawReply invoke(Instance& inst, const std::string& envelope_json) {
    wasm::RawReply out;
    JSContext* c = inst.ctx;
    inst.ops_left = kOpsPerCall;

    JSValue env = JS_ParseJSON(c, envelope_json.data(), envelope_json.size(), "<env>");
    if (JS_IsException(env)) {
        JS_FreeValue(c, JS_GetException(c));
        out.error = "js: bad envelope";
        return out;
    }
    JSValue g = JS_GetGlobalObject(c);
    JSValue h = JS_GetPropertyStr(c, g, "handle");
    JSValue ret = JS_UNDEFINED;
    if (!JS_IsFunction(c, h)) {
        out.error = "no handle() defined"; // parity: the lua seam's wording
    } else {
        JSValue args[] = {env};
        ret = JS_Call(c, h, JS_UNDEFINED, 1, args);
        if (JS_IsException(ret)) {
            out.error = "js: " + exception_msg(c);
        } else {
            const std::string blob = json_stringify(c, ret);
            if (blob.empty() || glz::read_json(out.reply, blob)) {
                out.error = "js: bad reply";
            } else {
                out.invoked = true;
            }
        }
    }
    JS_FreeValue(c, ret);
    JS_FreeValue(c, h);
    JS_FreeValue(c, g);
    JS_FreeValue(c, env);
    return out;
}

} // namespace plugin::js
