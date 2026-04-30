#pragma once

#include <cmath>

#include <juce_core/juce_core.h>

namespace forge7
{

/** Latest tuner analysis for UI (message thread only). */
struct TunerState
{
    bool signalPresent { false };
    float frequencyHz { 0.0f };
    juce::String noteName { "-" };
    int octave { 0 };
    float centsOffset { 0.0f };
    float confidence { 0.0f };
    float inputLevel { 0.0f };
};

/** Lightweight pitch estimate from mono samples (message thread). Not RT-safe work size - caller copies snapshot first. */
class TunerPitchAnalyzer
{
public:
    /** Guitar-focused range ~40 Hz-1000 Hz; returns state with note/octave/cents vs equal temperament. */
    static TunerState analyze(const float* samples, int numSamples, double sampleRate) noexcept;

private:
    static constexpr float kMinHz = 40.0f;
    static constexpr float kMaxHz = 1000.0f;
    static constexpr float kLevelGate = 0.0008f;
};

} // namespace forge7
