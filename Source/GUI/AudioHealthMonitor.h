#pragma once

#include <cstdint>

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

class AudioEngine;

/** Message-thread UI: compares consecutive audio callback counters from `AudioEngine`

    to surface gross dropouts / stalls (best-effort; not a profiler). Does not allocate on the audio thread. */
class AudioHealthMonitor final : public juce::Component,
                                 private juce::Timer
{
public:
    explicit AudioHealthMonitor(AudioEngine* engine);
    ~AudioHealthMonitor() override;

    void resized() override;

private:
    void timerCallback() override;

    AudioEngine* audioEngine = nullptr;
    juce::Label statusLabel;
    uint64_t previousCallbackTotal { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioHealthMonitor)
};

} // namespace forge7
