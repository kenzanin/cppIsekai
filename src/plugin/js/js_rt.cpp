#include "js_rt.hpp"
#include "js_internal.hpp"

#include <spdlog/spdlog.h>

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

std::string exception_msg(JSContext* ctx) {
    JSValue e = JS_GetException(ctx);
    const std::string out = arg_str(ctx, e); // ToString the error
    JS_FreeValue(ctx, e);
    return out.empty() ? "unknown error" : out;
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

    bind_host_globals(inst->ctx);

    JSValue ret = JS_Eval(inst->ctx, entry_src.data(), entry_src.size(), "<entry>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(ret)) {
        return fail("js: entry: " + exception_msg(inst->ctx));
    }
    JS_FreeValue(inst->ctx, ret);

    out.instance = std::move(inst);
    return out;
}

wasm::RawReply invoke(Instance& inst, const std::string& envelope_json) {
    wasm::RawReply out;
    JSContext* c = inst.ctx;
    JS_UpdateStackTop(inst.rt); // pool-thread stack base differs from the loader's
    inst.ops_left = kOpsPerCall;

    JSValue env = JS_ParseJSON(c, envelope_json.data(), envelope_json.size(), "<env>");
    if (JS_IsException(env)) {
        const std::string why = exception_msg(c);
        spdlog::warn("[js:dbg] env={} err={}", envelope_json, why);
        out.error = "js: bad envelope: " + why;
        JS_FreeValue(c, JS_GetException(c));
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
