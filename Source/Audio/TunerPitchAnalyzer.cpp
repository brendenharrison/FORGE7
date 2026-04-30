#include "TunerPitchAnalyzer.h"

#include <cmath>

namespace forge7
{
namespace
{
float rms(const float* x, int n) noexcept
{
    if (x == nullptr || n <= 0)
        return 0.0f;

    double acc = 0.0;

    for (int i = 0; i < n; ++i)
        acc += static_cast<double>(x[i]) * static_cast<double>(x[i]);

    return static_cast<float>(std::sqrt(acc / static_cast<double>(n)));
}

const char* noteLetter(int midiNoteNumber) noexcept
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int idx = (midiNoteNumber % 12 + 12) % 12;
    return names[idx];
}

} // namespace

TunerState TunerPitchAnalyzer::analyze(const float* samples, int numSamples, double sampleRate) noexcept
{
    TunerState out;

    if (samples == nullptr || numSamples < 256 || sampleRate < 8000.0)
        return out;

    const float level = rms(samples, numSamples);
    out.inputLevel = juce::jlimit(0.0f, 1.0f, level * 4.0f);

    if (level < kLevelGate)
        return out;

    const double sr = sampleRate;
    const int lagMin = juce::jmax(3, static_cast<int>(std::floor(sr / static_cast<double>(kMaxHz))));
    const int lagMax = juce::jmin(numSamples - 2, static_cast<int>(std::ceil(sr / static_cast<double>(kMinHz))));

    if (lagMax <= lagMin)
        return out;

    double bestCorr = 0.0;
    int bestLag = lagMin;

    for (int lag = lagMin; lag <= lagMax; ++lag)
    {
        double sum = 0.0;
        const int limit = numSamples - lag;

        for (int i = 0; i < limit; ++i)
            sum += static_cast<double>(samples[i]) * static_cast<double>(samples[i + lag]);

        if (sum > bestCorr)
        {
            bestCorr = sum;
            bestLag = lag;
        }
    }

    if (bestCorr <= 0.0)
        return out;

    const float hz = static_cast<float>(sr / static_cast<double>(bestLag));

    if (!std::isfinite(hz) || hz < kMinHz || hz > kMaxHz)
        return out;

    const float midiFloat = 69.0f + 12.0f * std::log2(hz / 440.0f);
    const int midiRounded = static_cast<int>(std::lround(static_cast<double>(midiFloat)));
    const float nearestHz = 440.0f * std::pow(2.0f, (static_cast<float>(midiRounded) - 69.0f) / 12.0f);
    const float cents = 1200.0f * std::log2(hz / nearestHz);

    out.signalPresent = true;
    out.frequencyHz = hz;
    out.noteName = noteLetter(midiRounded);
    out.octave = (midiRounded / 12) - 1;
    out.centsOffset = cents;
    out.confidence = juce::jlimit(0.0f, 1.0f, static_cast<float>(bestCorr / (static_cast<double>(numSamples) * level * level + 1.0e-9)));

    return out;
}

} // namespace forge7
