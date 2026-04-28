#pragma once

#include <atomic>
#include <cstdint>

namespace forge7
{

/** Linear-gain crossfade between two mono buffers: `output = dryOut*(1-t) + dryIn*t` over a configurable duration.

    Threading (**real-time contract**):
    - **Audio thread**: may call only `processCrossfadeBlock()`, `isCrossfading()`.
    - **Message / setup thread**: `prepare()`, `setCrossfadeTimeMs()`, `beginCrossfade()`, `abort()`.

    No heap allocation occurs on the audio path. Length and phase use atomics so the message thread
    can arm a fade (release) while the audio thread reads parameters (acquire) without mutexes.

    Future migrations (longer fades, curves): keep `fadeLengthSamples` + `fadePhaseSamples` as the
    single source of truth; avoid recomputing durations inside `processCrossfadeBlock`. */
class CrossfadeMixer
{
public:
    CrossfadeMixer() = default;

    /** Not real-time — call from `prepareToPlay` only. `maximumBlockSamples` reserved for future
        SIMD alignment / partial-block staging (unused in v1). */
    void prepare(double sampleRate, int maximumBlockSamples);

    /** Not real-time. Persists preference; refreshes `fadeLengthSamples` from current `sampleRate`. */
    void setCrossfadeTimeMs(double milliseconds) noexcept;

    double getCrossfadeTimeMs() const noexcept { return crossfadeTimeMs; }

    /** Message thread: begin a fade on the **next** audio block. Pair with host-side routing of
        outgoing/incoming processors **before** calling this. */
    void beginCrossfade() noexcept;

    /** Message thread: cancel an in-flight fade (e.g. project load). Audio thread stops mixing
        on the next read of `isCrossfading()`. */
    void abort() noexcept;

    /** Audio thread only. When false, host should process a single chain only. */
    bool isCrossfading() const noexcept { return crossfading.load(std::memory_order_acquire); }

    /** Audio thread: combine `dryOut` (fade-out branch) and `dryIn` (fade-in branch) into `output`.
        Linear ramp: `output = dryOut * (1-t) + dryIn * t` with `t = phase / fadeLength`.

        Returns true **once** when the fade completes (caller commits new audible chain routing). */
    bool processCrossfadeBlock(const float* dryOut, const float* dryIn, float* output, int numSamples) noexcept;

private:
    void refreshFadeLengthSamples() noexcept;

    double sampleRate { 48000.0 };

    /** Default seamless chain switch length — prefer updating via `setCrossfadeTimeMs` for UX. */
    double crossfadeTimeMs { 20.0 };

    std::atomic<uint64_t> fadeLengthSamples { 960 };
    std::atomic<uint64_t> fadePhaseSamples { 0 };
    std::atomic<bool> crossfading { false };

    CrossfadeMixer(const CrossfadeMixer&) = delete;
    CrossfadeMixer& operator=(const CrossfadeMixer&) = delete;
};

} // namespace forge7
