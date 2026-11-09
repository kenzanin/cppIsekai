#include "wasm_rt.hpp"
#include "wasm_internal.hpp"

#include "plugin/fs.hpp" // plugin::fs_read -- the env.fs_read body
#include "plugin/htmltext.hpp" // host.re2/html core (task 1.3)

#include <spdlog/spdlog.h>

#include <array>
#include <cstring>
#include <ctime>
#include <stdexcept>

namespace plugin::wasm {
namespace {

// 64 KiB wasm stack per instance; per-frame depth is the build-time
// d_m3MaxFunctionStackHeight (header note -- this wasm3 forks no
// m3_SetMaxCallStackDepth).
constexpr uint32_t kStackBytes = 64 * 1024;

// Import-domain failures carried in i32 results.
constexpr int32_t kNotFound = -1;  // key/relation/file absent, fetch failed
constexpr int32_t kTruncated = -2; // result larger than the guest's cap
constexpr int32_t kBadPattern = -3; // re2 pattern failed compilation

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

// host.re2/html: one core (plugin::htmltext), results as JSON arrays written
// into guest memory via the same guest_write convention as fs_read/storage.
const void* host_re2_call(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*, bool full) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t pat_off = arg_i32(args);
    const uint32_t pat_len = arg_i32(args);
    const uint32_t txt_off = arg_i32(args);
    const uint32_t txt_len = arg_i32(args);
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto pat = guest_str(self, pat_off, pat_len);
    const auto txt = guest_str(self, txt_off, txt_len);
    if (pat && txt) {
        try {
            const auto groups = full ? plugin::re2_match(*pat, *txt) : plugin::re2_find(*pat, *txt);
            // JSON array of groups; empty list on no-match (never an error).
            std::string json = "[";
            for (size_t i = 0; i < groups.size(); ++i) {
                if (i != 0) {
                    json += ',';
                }
                json += '"';
                for (const char c : groups[i]) {
                    if (c == '"' || c == '\\') {
                        json += '\\';
                    }
                    json += c;
                }
                json += '"';
            }
            json += ']';
            if (guest_write(self, out, cap, json)) {
                rc = static_cast<int32_t>(json.size());
            } else {
                rc = kTruncated;
            }
        } catch (const std::invalid_argument&) {
            rc = kBadPattern; // loud: bad pattern is the plugin's bug
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_re2_match(IM3Runtime rt, IM3ImportContext ctx, uint64_t* sp, void* mem) {
    return host_re2_call(rt, ctx, sp, mem, /*full=*/true);
}

const void* host_re2_find(IM3Runtime rt, IM3ImportContext ctx, uint64_t* sp, void* mem) {
    return host_re2_call(rt, ctx, sp, mem, /*full=*/false);
}

const void* host_html_call(IM3Runtime, IM3ImportContext ctx, uint64_t* sp, void*, bool want_attr) {
    auto& self = *static_cast<Instance*>(ctx->userdata);
    uint64_t* args = sp + 1;
    const uint32_t html_off = arg_i32(args);
    const uint32_t html_len = arg_i32(args);
    const uint32_t sel_off = arg_i32(args);
    const uint32_t sel_len = arg_i32(args);
    uint32_t attr_off = 0, attr_len = 0;
    if (want_attr) {
        attr_off = arg_i32(args);
        attr_len = arg_i32(args);
    }
    const uint32_t out = arg_i32(args);
    const uint32_t cap = arg_i32(args);

    int32_t rc = kNotFound;
    const auto html = guest_str(self, html_off, html_len);
    const auto sel = guest_str(self, sel_off, sel_len);
    const auto attr = want_attr ? guest_str(self, attr_off, attr_len) : std::optional<std::string>{};
    if (html && sel && (!want_attr || attr)) {
        // JSON string array; malformed HTML / no match => [] (lenient).
        const auto vals = want_attr ? plugin::html_attr(*html, *sel, *attr) : plugin::html_select(*html, *sel);
        std::string json = "[";
        for (size_t i = 0; i < vals.size(); ++i) {
            if (i != 0) {
                json += ',';
            }
            json += '"';
            for (const char c : vals[i]) {
                if (c == '"' || c == '\\') {
                    json += '\\';
                }
                json += c;
            }
            json += '"';
        }
        json += ']';
        if (guest_write(self, out, cap, json)) {
            rc = static_cast<int32_t>(json.size());
        } else {
            rc = kTruncated;
        }
    }
    *reinterpret_cast<int32_t*>(sp) = rc;
    return m3Err_none;
}

const void* host_html_select(IM3Runtime rt, IM3ImportContext ctx, uint64_t* sp, void* mem) {
    return host_html_call(rt, ctx, sp, mem, /*want_attr=*/false);
}

const void* host_html_attr(IM3Runtime rt, IM3ImportContext ctx, uint64_t* sp, void* mem) {
    return host_html_call(rt, ctx, sp, mem, /*want_attr=*/true);
}

constexpr std::array kImports{
    Import{"now", "I", host_now},
    Import{"log_info", "v(ii)", host_log_info},
    Import{"fs_read", "i(iiii)", host_fs_read},
    Import{"storage_get", "i(iiii)", host_storage_get},
    Import{"storage_set", "v(iiii)", host_storage_set},
    Import{"http_fetch", "i(iiiiii)", host_http_fetch},
    Import{"re2_match", "i(iiiiii)", host_re2_match},
    Import{"re2_find", "i(iiiiii)", host_re2_find},
    Import{"html_select", "i(iiiii)", host_html_select},
    Import{"html_attr", "i(iiiiiii)", host_html_attr},
};

} // namespace

// ---------------------------------------------------------------------------
// lifecycle
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

} // namespace plugin::wasm
