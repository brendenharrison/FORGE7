#include "CrossfadeMixer.h"

#include <algorithm>
#include <cmath>

namespace forge7
{

void CrossfadeMixer::prepare(double newSampleRate, int /*maximumBlockSamples*/)
{
    sampleRate = newSampleRate;
    refreshFadeLengthSamples();
}

void CrossfadeMixer::setCrossfadeTimeMs(double milliseconds) noexcept
{
    crossfadeTimeMs = std::clamp(milliseconds, 0.5, 5000.0);
    refreshFadeLengthSamples();
}

void CrossfadeMixer::refreshFadeLengthSamples() noexcept
{
    const double samples = sampleRate * (crossfadeTimeMs * 0.001);
    const auto len = static_cast<uint64_t>(std::max(1.0, std::round(samples)));
    fadeLengthSamples.store(len, std::memory_order_relaxed);
}

void CrossfadeMixer::beginCrossfade() noexcept
{
    fadePhaseSamples.store(0, std::memory_order_relaxed);
    crossfading.store(true, std::memory_order_release);
}

void CrossfadeMixer::abort() noexcept
{
    crossfading.store(false, std::memory_order_release);
    fadePhaseSamples.store(0, std::memory_order_relaxed);
}

bool CrossfadeMixer::processCrossfadeBlock(const float* dryOut,
                                           const float* dryIn,
                                           float* output,
                                           const int numSamples) noexcept
{
    if (output == nullptr || numSamples <= 0)
        return false;

    if (! isCrossfading())
        return false;

    const uint64_t len = fadeLengthSamples.load(std::memory_order_acquire);
    uint64_t phase = fadePhaseSamples.load(std::memory_order_relaxed);

    const float* srcOut = dryOut != nullptr ? dryOut : dryIn;
    const float* srcIn = dryIn != nullptr ? dryIn : dryOut;

    for (int i = 0; i < numSamples; ++i)
    {
        const uint64_t p = phase + static_cast<uint64_t>(i);

        float sample;

        if (len == 0 || p >= len)
            sample = srcIn != nullptr ? srcIn[i] : 0.0f;
        else
        {
            const float t = static_cast<float>(static_cast<double>(p) / static_cast<double>(len));
            const float gOut = 1.0f - t;
            const float gIn = t;
            const float a = srcOut != nullptr ? srcOut[i] : 0.0f;
            const float b = srcIn != nullptr ? srcIn[i] : 0.0f;
            sample = a * gOut + b * gIn;
        }

        output[i] = sample;
    }

    phase += static_cast<uint64_t>(numSamples);
    fadePhaseSamples.store(phase, std::memory_order_relaxed);

    const bool completed = phase >= len && len > 0;

    if (completed)
    {
        abort();
        return true;
    }

    return false;
}

} // namespace forge7
