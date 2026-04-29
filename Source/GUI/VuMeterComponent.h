#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

/** Compact peak meter for mono/stereo display; reads atomics on timer (message thread only).

    Smoothing and clip hold are UI-side only. Threshold matches audio tap clip detection (0.98). */
class VuMeterComponent final : public juce::Component,
                                private juce::Timer
{
public:
    /** If `readRight` is null, `readLeft` is used for both channels (mono duplicate). */
    VuMeterComponent(std::function<float()> readLeft, std::function<float()> readRight, bool verticalLayout);

    ~VuMeterComponent() override;

    void paint(juce::Graphics& g) override;

    /** Optional tiny label below/beside (ASCII), e.g. "IN", "OUT". */
    void setCaption(const juce::String& s) noexcept { caption = s; }

private:
    void timerCallback() override;

    std::function<float()> readLeftFn;
    std::function<float()> readRightFn;
    bool verticalLayout { true };

    juce::String caption;

    float displayedL { 0.0f };
    float displayedR { 0.0f };

    uint32_t clipHoldUntilMs { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VuMeterComponent)
};

} // namespace forge7
