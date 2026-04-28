#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

class AudioEngine;

/** Displays approximate CPU usage derived from measurements taken outside the RT callback
    (e.g. time delta around process dispatch). Reads atomics fed by AudioEngine / host on
    a timer - never queries plugins directly from paint(). */
class CpuMeter final : public juce::Component,
                       private juce::Timer
{
public:
    explicit CpuMeter(AudioEngine* engine);
    ~CpuMeter() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    AudioEngine* audioEngine = nullptr;
    juce::String cpuLabel { "CPU -%" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CpuMeter)
};

} // namespace forge7
