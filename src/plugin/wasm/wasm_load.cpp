#include "wasm_rt.hpp"
#include "wasm_internal.hpp"

#include <stdexcept>
#include <string_view>

namespace plugin::wasm {

constexpr uint32_t kStackBytes = 64 * 1024;


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
    {
        std::string err;
        if (!link_imports(module, *inst, err)) {
            return fail(std::move(err));
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
