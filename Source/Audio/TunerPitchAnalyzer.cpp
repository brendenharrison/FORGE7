#include "TunerPitchAnalyzer.h"

#include <algorithm>
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

/** Normalized autocorrelation at lag (bounded [-1,1]-ish). */
float normalizedCorrelation(const float* x, int n, int lag, double mean) noexcept
{
    if (lag <= 0 || lag >= n || x == nullptr)
        return 0.0f;

    const int len = n - lag;
    if (len < 4)
        return 0.0f;

    double dot = 0.0;
    double e0 = 0.0;
    double e1 = 0.0;

    for (int i = 0; i < len; ++i)
    {
        const double a = static_cast<double>(x[i]) - mean;
        const double b = static_cast<double>(x[i + lag]) - mean;
        dot += a * b;
        e0 += a * a;
        e1 += b * b;
    }

    const double den = std::sqrt(e0 * e1 + 1.0e-18);
    return static_cast<float>(dot / den);
}

} // namespace

TunerState TunerPitchAnalyzer::analyze(const float* samples, int numSamples, double sampleRate) noexcept
{
    TunerState out;

    if (samples == nullptr || numSamples < 256 || sampleRate < 8000.0)
        return out;

    double sum = 0.0;

    for (int i = 0; i < numSamples; ++i)
        sum += static_cast<double>(samples[i]);

    const double mean = sum / static_cast<double>(numSamples);

    const float level = rms(samples, numSamples);
    out.inputLevel = juce::jlimit(0.0f, 1.0f, level * 4.0f);

    if (level < kLevelGate)
        return out;

    const double sr = sampleRate;
    const int lagMin = juce::jmax(3, static_cast<int>(std::floor(sr / static_cast<double>(kMaxHz))));
    const int lagMax = juce::jmin(numSamples - 4, static_cast<int>(std::ceil(sr / static_cast<double>(kMinHz))));

    if (lagMax <= lagMin)
        return out;

    float globalMax = -1.0f;

    for (int lag = lagMin; lag <= lagMax; ++lag)
    {
        const float nc = normalizedCorrelation(samples, numSamples, lag, mean);
        globalMax = juce::jmax(globalMax, nc);
    }

    if (globalMax < 0.08f)
        return out;

    /** Prefer the longest period (largest lag) among lags within a hair of the peak - reduces harmonic lock. */
    int bestLag = lagMin;
    const float nearPeak = juce::jmax(0.08f, globalMax * 0.93f);

    for (int lag = lagMin; lag <= lagMax; ++lag)
    {
        const float nc = normalizedCorrelation(samples, numSamples, lag, mean);
        if (nc >= nearPeak)
            bestLag = juce::jmax(bestLag, lag);
    }

    const float bestCorr = normalizedCorrelation(samples, numSamples, bestLag, mean);

    if (bestCorr < 0.08f)
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
    out.confidence = juce::jlimit(0.0f, 1.0f, bestCorr);

    return out;
}

} // namespace forge7
