#pragma once

/** \file wasm_rt.hpp
 *  \brief wasm3-backed plugin runtime (design D3).
 *
 * The guest module exports alloc/handle/last_len -- all i32 linear-memory
 * offsets -- and may import the env.* host services below; both directions
 * move raw bytes by guest offset:
 *   ptr = alloc(len); [write envelope at ptr]; out = handle(ptr, len);
 *   n = last_len(); reply = memory[out, out+n)
 * The module must define a linear memory (the one imports and exports
 * address). This seam owns the whole wasm side: load/link/compile, the
 * per-call fuel budget and the trap -> ok:false mapping. Whatever handle
 * leaves behind must parse as JSON; everything past that is the shared shape
 * check (plugin.cpp).
 *
 * Limits: this vendored wasm3 forks no m3_SetMaxCallStackDepth -- the runtime
 * stack is fixed at creation and per-frame depth by the build-time
 * d_m3MaxFunctionStackHeight. It carries a gas meter (m3_SetResourceLimit,
 * armed before compilation) used as the per-call fuel budget, so runaway
 * loops/recursion trap out of gas like lua's count hook. Tunables live in
 * wasm_internal.hpp / wasm_load.cpp; ponytail: promote to plugin::Config when
 * a plugin needs other values.
 */

#include <glaze/glaze.hpp>

#include "plugin/runtime_common.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <wasm3.h>

namespace plugin::wasm {

// Shared runtime types (RawReply/Services and the state/fetch signatures) live
// in plugin::runtime_common.hpp; aliased here so the wasm seam keeps its
// spelling. The lua seam consumes the same via plugin::lua, the core directly.
using plugin::HttpFetch;
using plugin::RawReply;
using plugin::Services;

/** \brief One loaded module: runtime + parsed image + resolved imports. The
 *  wasm bytes must outlive the module (m3 keeps pointers into them), hence
 *  the member. */
struct Instance {
    ~Instance();

    std::string id;
    std::string dir;
    Services services;
    std::vector<std::uint8_t> bytes;

    IM3Environment env = nullptr;
    IM3Runtime rt = nullptr;
    IM3Module module = nullptr;
    IM3Function f_alloc = nullptr;    // i32(size) -> i32 ptr
    IM3Function f_handle = nullptr;   // i32(ptr), i32(len) -> i32 out (0 = no reply)
    IM3Function f_last_len = nullptr; // () -> i32
};

struct Loaded {
    std::unique_ptr<Instance> instance; /**< the loaded module, nullptr on failure */
    std::string error;                  /**< diagnostic when instance == nullptr */
};
/** \brief Parse, load, link the env.* imports and compile; a nullptr instance
 *  with \p error on any failure (the plugin then loads as absent). */
[[nodiscard]] Loaded load(std::string id, std::string dir, std::vector<std::uint8_t> wasm_bytes, Services services);

/** \brief One ABI call: fresh fuel, alloc, envelope write, handle, last_len,
 *  read. Any trap or a non-JSON reply => !invoked with a "wasm: ..." error. */
[[nodiscard]] RawReply invoke(Instance& inst, const std::string& envelope_json);

} // namespace plugin::wasm
