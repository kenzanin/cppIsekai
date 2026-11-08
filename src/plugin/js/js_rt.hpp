#pragma once

/** \file js_rt.hpp
 *  \brief quickjs-backed plugin runtime (design D4).
 *
 * Entry = global handle(env); the envelope arrives as a plain object and the
 * reply must JSON-stringify to the shared {"ok":...} contract. One realm per
 * plugin; the budget is an interrupt handler (ops chunk) so runaway loops end
 * ok:false like wasm's gas. Globals: standard builtins plus the host tools
 * (http/storage/log/fs/now); no module loader, no os/process -- sandbox by
 * absence (ponytail: explicit prune if a plugin ever needs tighter walls).
 */
#include "plugin/wasm/wasm_rt.hpp" // RawReply/Services via the wasm aliases

#include <quickjs.h>

#include <memory>
#include <string>

namespace plugin::js {

struct Instance {
    ~Instance();

    std::string id;
    std::string dir;
    wasm::Services services;
    JSRuntime* rt = nullptr;
    JSContext* ctx = nullptr;
    long long ops_left = 0; // budget cell drained by the interrupt handler
};

struct Loaded {
    std::unique_ptr<Instance> instance; /**< the loaded realm, nullptr on failure */
    std::string error;                  /**< diagnostic when instance == nullptr */
};
/** \brief Create the realm, bind host globals, evaluate the entry script; a
 *  nullptr instance with \p error on failure (the plugin loads as absent). */
[[nodiscard]] Loaded load(std::string id, std::string dir, std::string entry_src, wasm::Services services);

/** \brief One ABI call: fresh budget, parse envelope, handle(env),
 *  JSON.stringify. */
[[nodiscard]] wasm::RawReply invoke(Instance& inst, const std::string& envelope_json);

} // namespace plugin::js
