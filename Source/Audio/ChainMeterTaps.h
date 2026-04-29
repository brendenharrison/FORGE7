#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace forge7
{

constexpr int kChainMeterSlots = 8;

/** Lock-free peak taps written from the audio callback only (relaxed atomics).
    GUI reads on the message thread for VuMeterComponent. Mono guitar path: duplicate L/R in UI.
    Slot count must match `kPluginChainMaxSlots`. */
struct ChainMeterTaps
{
    /** Signal entering the plugin chain (after input gain), block peak [0, 1]. */
    std::atomic<float> preChainPeak { 0.0f };

    /** Peak after processing slot `i` (pass-through if slot empty/bypassed). */
    std::array<std::atomic<float>, static_cast<size_t>(kChainMeterSlots)> postSlotPeak {};

    /** Mono signal after output gain, immediately before copying to device outputs. */
    std::atomic<float> postOutputGainPeak { 0.0f };

    void resetPeaksToZero() noexcept
    {
        preChainPeak.store(0.0f, std::memory_order_relaxed);
        postOutputGainPeak.store(0.0f, std::memory_order_relaxed);

        for (auto& p : postSlotPeak)
            p.store(0.0f, std::memory_order_relaxed);
    }
};

} // namespace forge7
