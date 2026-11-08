#include "wasm_rt.hpp"

#include "fs.hpp" // plugin::fs_read -- the env.fs_read body

#include <spdlog/spdlog.h>

#include <array>
#include <cstring>
#include <ctime>
#include <span>
#include <utility>

namespace plugin::wasm {

namespace {

// Tunables (see the header note): 64 KiB wasm stack per instance and a gas
// budget per handle() call in the same ~instruction order as lua's count hook.
constexpr uint32_t kStackBytes = 64 * 1024;
constexpr uint64_t kGasPerCall = 5'000'000;

// Import-domain failures carried in i32 results.
constexpr int32_t kNotFound = -1;  // key/relation/file absent, fetch failed
constexpr int32_t kTruncated = -2; // result larger than the guest's cap

// ---------------------------------------------------------------------------
// guest memory. Raw imports and the host side both go through m3_GetMemory on
// the module -- the one linear memory the ABI speaks in.
// ---------------------------------------------------------------------------

std::optional<std::span<uint8_t>> guest_mem(Instance& inst, uint32_t off, uint32_t len) {
    size_t size = 0;
    uint8_t* base = m3_GetMemory(inst.module, &size, 0);
    if (base == nullptr || off > size || len > size - off) {
        return std::nullopt;
    }
    return std::span<uint8_t>{base + off, len};
}

std::optional<std::string> guest_str(Instance& inst, uint32_t off, uint32_t len) {
    const auto mem = guest_mem(inst, off, len);
    if (!mem) {
        return std::nullopt;
    }
    return std::string{reinterpret_cast<const char*>(mem->data()), mem->size()};
}

// Copies `data` into the guest window [off, off+cap); guest-declared caps are
// bounds-checked here, not trusted.
bool guest_write(Instance& inst, uint32_t off, uint32_t cap, const std::string& data) {
    if (data.size() > cap) {
        return false;
    }
    const auto mem = guest_mem(inst, off, static_cast<uint32_t>(data.size()));
    if (!mem) {
        return false;
    }
    std::memcpy(mem->data(), data.data(), data.size());
    return true;
}

// ---------------------------------------------------------------------------
// env.* imports. Slot convention per m3_core.h (m3ApiReturnType/GetArg): the
// return slot sits at _sp[0], arguments after it, one 8-byte cell each; an
// i32 occupies the low half. Hand-rolled rather than via the m3Api macros so
// the unused runtime/mem params stay nameless (no -Wextra noise).
// ---------------------------------------------------------------------------

uint32_t arg_i32(uint64_t*& sp) {
    return static_cast<uint32_t>(*sp++);
}

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

const void* host_fs_read(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1; // over the i32 return slot
    const uint32_t rel_off = arg_i32(args);
    const uint32_t rel_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto rel = guest_str(self, rel_off, rel_len)) {
        if (auto content = plugin::fs_read(self.dir, *rel)) {
            if (guest_write(self, out, cap, *content)) {
                rc = static_cast<int32_t>(content->size());
            } else {
                rc = kTruncated;
            }
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_storage_get(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t key_off = arg_i32(args);
    const uint32_t key_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    if (const auto key = guest_str(self, key_off, key_len)) {
        std::optional<std::string> v;
        if (self.services.state_get) {
            v = self.services.state_get(self.id, *key);
        }
        if (v && guest_write(self, out, cap, *v)) {
            rc = static_cast<int32_t>(v->size());
        } else if (v) {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_storage_set(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    const uint32_t key_off = arg_i32(sp); // void import: no return slot
    const uint32_t key_len = arg_i32(sp);
    const uint32_t val_off = arg_i32(sp);
    const uint32_t val_len = arg_i32(sp);

    const auto key = guest_str(self, key_off, key_len);
    const auto val = guest_str(self, val_off, val_len);
    if (!key || !val) {
        return m3Err_trapOutOfBoundsMemoryAccess;
    }
    if (self.services.state_set) {
        self.services.state_set(self.id, *key, *val);
    }
    return m3Err_none;
}

const void* host_http_fetch(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t url_off = arg_i32(args);
    const uint32_t url_len = arg_i32(args);
    const uint32_t opts_off = arg_i32(args);
    const uint32_t opts_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto url = guest_str(self, url_off, url_len);
    const auto opts = guest_str(self, opts_off, opts_len);
    if (url && opts && self.services.http_fetch) {
        if (auto reply = self.services.http_fetch(*url, *opts)) {
            if (guest_write(self, out, cap, *reply)) {
                rc = static_cast<int32_t>(reply->size());
            } else {
                rc = kTruncated;
            }
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

struct Import {
    const char* name;
    const char* sig; // m3 link notation, m3_core.h ConvertTypeCharToTypeId
    M3RawCall fn;
};

constexpr std::array kImports{
    Import{"now", "I", host_now},
    Import{"log_info", "v(ii)", host_log_info},
    Import{"fs_read", "i(iiii)", host_fs_read},
    Import{"storage_get", "i(iiii)", host_storage_get},
    Import{"storage_set", "v(iiii)", host_storage_set},
    Import{"http_fetch", "i(iiiiii)", host_http_fetch},
};

} // namespace

// ---------------------------------------------------------------------------
// lifecycle + call
// ---------------------------------------------------------------------------

Instance::~Instance() {
    // m3_LoadModule transfers the module to the runtime even on failure, so
    // freeing the runtime covers every post-load state; a module that never
    // reached a runtime (parse ok, load not attempted) is freed here.
    if (rt != nullptr) {
        m3_FreeRuntime(rt);
    } else if (module != nullptr) {
        m3_FreeModule(module);
    }
    if (env != nullptr) {
        m3_FreeEnvironment(env);
    }
}

Loaded load(std::string id, std::string dir, std::vector<std::uint8_t> wasm_bytes, Services services) {
    Loaded out;
    if (wasm_bytes.empty()) {
        out.error = "empty wasm image";
        return out;
    }

    auto inst = std::make_unique<Instance>();
    inst->id = std::move(id);
    inst->dir = std::move(dir);
    inst->services = std::move(services);
    inst->bytes = std::move(wasm_bytes);
    const auto fail = [&out](std::string msg) {
        out.error = std::move(msg);
        return std::move(out);
    };

    inst->env = m3_NewEnvironment();
    if (inst->env == nullptr) {
        return fail("environment alloc failed");
    }
    inst->rt = m3_NewRuntime(inst->env, kStackBytes, nullptr);
    if (inst->rt == nullptr) {
        return fail("runtime alloc failed");
    }
    // Arm the meter before anything compiles, so every body charges gas.
    if (const M3Result e = m3_SetResourceLimit(inst->rt, c_m3Limit_GasUnits, kGasPerCall)) {
        return fail(std::string("gas limit refused: ") + e);
    }

    IM3Module module = nullptr;
    if (const M3Result e =
            m3_ParseModule(inst->env, &module, inst->bytes.data(), static_cast<uint32_t>(inst->bytes.size()))) {
        return fail(std::string("parse: ") + (e != nullptr ? e : "malformed module"));
    }
    inst->module = module; // dtor frees it until the runtime takes ownership

    if (const M3Result e = m3_LoadModule(inst->rt, module)) {
        return fail(std::string("load: ") + e);
    }
    if (const M3Result e = m3_RunStart(module)) {
        return fail(std::string("start: ") + e);
    }

    // Link every host service; a miss only means the module does not import
    // that name. Any other complaint (bad signature, ...) is fatal here.
    for (const Import& imp : kImports) {
        const M3Result e = m3_LinkRawFunctionEx(module, "env", imp.name, imp.sig, imp.fn, inst.get());
        if (e != nullptr && std::string_view{e} != std::string_view{m3Err_functionLookupFailed}) {
            return fail(std::string("link ") + imp.name + ": " + e);
        }
    }

    if (const M3Result e = m3_CompileModule(module)) {
        return fail(std::string("compile: ") + e);
    }

    struct Export {
        const char* name;
        IM3Function* slot;
    };
    for (const Export& x : {Export{"alloc", &inst->f_alloc}, Export{"handle", &inst->f_handle},
                            Export{"last_len", &inst->f_last_len}}) {
        if (const M3Result e = m3_FindFunctionIn(x.slot, inst->module, x.name)) {
            return fail(std::string("export ") + x.name + ": " + (e != nullptr ? e : "missing"));
        }
    }

    out.instance = std::move(inst);
    return out;
}

RawReply invoke(Instance& inst, const std::string& envelope_json) {
    RawReply out;
    const auto trap = [](M3Result e) { return std::string("wasm: ") + (e != nullptr ? e : "unknown error"); };

    // Fresh fuel per call; the meter was armed at load, so bodies charge.
    m3_SetResourceLimit(inst.rt, c_m3Limit_GasUnits, kGasPerCall);

    int32_t env_ptr = 0;
    if (const M3Result e = m3_CallV(inst.f_alloc, static_cast<int32_t>(envelope_json.size()))) {
        out.error = trap(e);
        return out;
    }
    if (const M3Result e = m3_GetResultsV(inst.f_alloc, &env_ptr)) {
        out.error = trap(e);
        return out;
    }

    auto mem = guest_mem(inst, static_cast<uint32_t>(env_ptr), static_cast<uint32_t>(envelope_json.size()));
    if (!mem) {
        out.error = "wasm: alloc outside linear memory";
        return out;
    }
    std::memcpy(mem->data(), envelope_json.data(), envelope_json.size());

    int32_t reply_ptr = 0;
    if (const M3Result e = m3_CallV(inst.f_handle, env_ptr, static_cast<int32_t>(envelope_json.size()))) {
        out.error = trap(e); // gas exhaustion, bad pointer, ... all land here
        return out;
    }
    if (const M3Result e = m3_GetResultsV(inst.f_handle, &reply_ptr)) {
        out.error = trap(e);
        return out;
    }
    if (reply_ptr == 0) { // ABI: 0 = the module failed to produce a reply
        out.error = "wasm: handle failed";
        return out;
    }

    int32_t reply_len = 0;
    if (const M3Result e = m3_CallV(inst.f_last_len)) {
        out.error = trap(e);
        return out;
    }
    if (const M3Result e = m3_GetResultsV(inst.f_last_len, &reply_len)) {
        out.error = trap(e);
        return out;
    }
    if (reply_len < 0) {
        out.error = "wasm: bad reply length";
        return out;
    }
    const auto reply_mem =
        guest_mem(inst, static_cast<uint32_t>(reply_ptr), static_cast<uint32_t>(reply_len));
    if (!reply_mem) {
        out.error = "wasm: reply outside linear memory";
        return out;
    }

    const std::string blob{reinterpret_cast<const char*>(reply_mem->data()), reply_mem->size()};
    if (auto ec = glz::read_json(out.reply, blob)) {
        out.error = "wasm: bad reply";
        return out;
    }
    out.invoked = true;
    return out;
}

} // namespace plugin::wasm
