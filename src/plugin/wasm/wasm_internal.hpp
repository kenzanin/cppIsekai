#pragma once

/** \file wasm_internal.hpp
 *  \brief Internal bits shared by the wasm load and invoke halves
 *  (wasm_load.cpp / wasm_rt.cpp).
 */

#include "wasm_rt.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace plugin::wasm {

/** Per-handle() fuel budget, armed at load and refreshed on every call; the
 *  wasm-side equivalent of lua's instruction-count hook (~same order). */
inline constexpr uint64_t kGasPerCall = 5'000'000;

/** \brief The module's one linear memory, window [off, off+len); nullopt when
 *  out of bounds -- guest offsets are bounds-checked here, never trusted. */
std::optional<std::span<uint8_t>> guest_mem(Instance& inst, uint32_t off, uint32_t len);

} // namespace plugin::wasm
