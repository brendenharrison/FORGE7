#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

class AudioEngine;

enum class MeterChannel
{
    inputAfterGain,
    outputAfterGain
};

/** Peak-style meter driven by atomic level stats published by AudioEngine (written from RT;
    painted on the message thread only). */
class LevelMeter final : public juce::Component
{
public:
    LevelMeter(AudioEngine* engine, MeterChannel channel);
    ~LevelMeter() override;

    void paint(juce::Graphics& g) override;

private:
    AudioEngine* audioEngine = nullptr;
    MeterChannel meterChannel = MeterChannel::inputAfterGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};

} // namespace forge7
