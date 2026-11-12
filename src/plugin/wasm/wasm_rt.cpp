#include "wasm_rt.hpp"
#include "wasm_internal.hpp"

#include <cstring>
#include <utility>

namespace plugin::wasm {

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
    const auto reply_mem = guest_mem(inst, static_cast<uint32_t>(reply_ptr), static_cast<uint32_t>(reply_len));
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
