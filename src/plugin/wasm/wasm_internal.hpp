#pragma once

/** \file wasm_internal.hpp
 *  \brief Internal bits shared by the wasm load, imports and invoke TUs
 *  (wasm_load.cpp / wasm_imports.cpp / wasm_rt.cpp).
 */

#include "wasm_rt.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>

namespace plugin::wasm {

/** Per-handle() fuel budget, armed at load and refreshed on every call; the
 *  wasm-side equivalent of lua's instruction-count hook (~same order). */
inline constexpr uint64_t kGasPerCall = 5'000'000;

/** \brief The module's one linear memory, window [off, off+len); nullopt when
 *  out of bounds -- guest offsets are bounds-checked here, never trusted. */
std::optional<std::span<uint8_t>> guest_mem(Instance& inst, uint32_t off, uint32_t len);

// Import-domain failures carried in i32 results.
inline constexpr int32_t kNotFound = -1;  // key/relation/file absent, fetch failed
inline constexpr int32_t kTruncated = -2; // result larger than the guest's cap
inline constexpr int32_t kBadPattern = -3; // re2 pattern failed compilation

/** \brief Reads a guest string at [off, off+len); nullopt out of bounds. */
inline std::optional<std::string> guest_str(Instance& inst, uint32_t off, uint32_t len) {
    const auto mem = guest_mem(inst, off, len);
    if (!mem) {
        return std::nullopt;
    }
    return std::string{reinterpret_cast<const char*>(mem->data()), mem->size()};
}

/** \brief Copies `data` into the guest window [off, off+cap); guest-declared caps are
 *  bounds-checked here, not trusted. */
inline bool guest_write(Instance& inst, uint32_t off, uint32_t cap, const std::string& data) {
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

/** \brief Link every host env.* import onto `module`; a miss only means the
 *  module does not import that name. Any other complaint is fatal. */
bool link_imports(IM3Module module, Instance& inst, std::string& err);

} // namespace plugin::wasm
