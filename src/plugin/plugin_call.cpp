#include "plugin_internal.hpp"

#include <algorithm>

namespace plugin {
namespace {

// raw marshaling seam for the lua path; the wasm/js paths marshal inside
// their runtime. Kept identical to the monolith.
thread_local long long t_budget_left = 0;

void count_hook(lua_State* L, lua_Debug* /*unused*/) {
    if ((t_budget_left -= 1000) <= 0) {
        luaL_error(L, "plugin instruction budget exhausted");
    }
}

} // namespace

RawReply lua_call_entry(const Runtime& rt, const std::string& method, long instruction_budget,
                        const glz::generic& envelope) {
    RawReply out;
    lua_State* L = rt.lua.get();
    sol::state_view sv(L);

    const sol::object env = lua::json_to_sol(L, envelope);
    const sol::object h = sv["handle"];
    if (h.get_type() != sol::type::function) {
        out.error = "no handle() defined";
        return out;
    }

    t_budget_left = instruction_budget;
    lua_sethook(L, count_hook, LUA_MASKCOUNT, 1000);
    const sol::protected_function_result res = sol::protected_function(h)(env);
    lua_sethook(L, nullptr, 0, 0);
    const bool timed_out = t_budget_left <= 0;

    if (!res.valid()) {
        const sol::error e = res;
        if (timed_out) {
            spdlog::warn("[plugin:{}] {} timed out", rt.info.id, method);
            out.error = "timeout: instruction budget exhausted";
            return out;
        }
        spdlog::warn("[plugin:{}] {} failed: {}", rt.info.id, method, e.what());
        out.error = e.what();
        return out;
    }

    out.invoked = true;
    const sol::object r = res;
    // depth -1: reply members marshal where the data value always has, so the
    // wrapper level does not eat one rung of the cyclic-table depth budget.
    out.reply = lua::sol_to_json(r, -1);
    return out;
}

// The response-shape gate, shared by every backend: {"ok":bool} plus the
// ok=true "data" contract, exactly as the core pinned it. Returns the wire
// response; nullopt with `error` set when the shape is rejected (the warning
// carries plugin/method context).
std::optional<std::string> checked_reply(const glz::generic& reply, const std::string& id, const std::string& method,
                                         std::string& error) {
    const auto* obj = std::get_if<glz::generic::object_t>(&reply.data);
    if (obj == nullptr) {
        spdlog::warn("[plugin:{}] {}: malformed response (non-table)", id, method);
        error = "malformed response: not a table";
        return std::nullopt;
    }
    const glz::generic* okv = member(obj, "ok");
    if (okv == nullptr || std::get_if<bool>(&okv->data) == nullptr) {
        spdlog::warn("[plugin:{}] {}: malformed response (missing ok)", id, method);
        error = "malformed response: missing ok";
        return std::nullopt;
    }
    if (!*std::get_if<bool>(&okv->data)) {
        const glz::generic* ev = member(obj, "error");
        const bool is_str = ev != nullptr && std::get_if<std::string>(&ev->data) != nullptr;
        const std::string msg = is_str ? std::get<std::string>(ev->data) : std::string("plugin error");
        spdlog::warn("[plugin:{}] {} returned error: {}", id, method, msg);
        error = msg;
        return std::nullopt;
    }
    static const glz::generic null_v;
    const glz::generic* data = member(obj, "data");
    return R"({"ok":true,"data":)" + dump_json(data ? *data : null_v) + "}";
}

std::string Host::call(const std::string& id, const std::string& method, const std::string& params_json) {
    std::shared_ptr<Runtime> rt;
    {
        const std::scoped_lock lock(impl_->mu);
        if (const auto it = impl_->plugins.find(id); it != impl_->plugins.end()) {
            rt = it->second;
        }
    }
    if (!rt) {
        return err_json("no plugin '" + id + "'");
    }

    // Health bookkeeping: one entry per finished call (design: in-memory).
    const auto record = [&](bool ok, std::string err) {
        const std::scoped_lock lock(impl_->mu);
        auto& h = impl_->health[id];
        h.ok = ok;
        h.error = std::move(err);
        h.at = std::time(nullptr);
    };
    const auto fail = [&](std::string msg) {
        record(false, msg);
        return err_json(std::move(msg));
    };

    const std::scoped_lock call_lock(rt->mu);

    // Runtime-agnostic request envelope; each backend marshals this itself.
    glz::generic envelope;
    auto& env = envelope.data.emplace<glz::generic::object_t>();
    glz::generic api_v;
    api_v.data = static_cast<double>(kHostApi);
    env["api"] = std::move(api_v);
    glz::generic method_v;
    method_v.data = method;
    env["method"] = std::move(method_v);
    if (params_json.empty()) {
        glz::generic empty_obj;
        empty_obj.data.emplace<glz::generic::object_t>();
        env["params"] = std::move(empty_obj);
    } else {
        glz::generic params;
        if (auto ec = glz::read_json(params, params_json)) {
            return fail("bad params json");
        }
        env["params"] = std::move(params);
    }

    RawReply r;
    if (rt->lua) {
        r = lua_call_entry(*rt, method, conf_.instruction_budget, envelope);
    } else if (rt->wasm) {
        r = wasm::invoke(*rt->wasm, dump_json(envelope));
    } else if (rt->js) {
        r = js::invoke(*rt->js, dump_json(envelope));
    } else {
        return fail("runtime '" + rt->info.runtime + "' not supported");
    }
    if (!r.invoked) {
        return fail(r.error);
    }
    std::string shape_err;
    if (std::optional<std::string> res = checked_reply(r.reply, id, method, shape_err)) {
        record(true, "");
        return *res;
    }
    return fail(std::move(shape_err));
}

} // namespace plugin
