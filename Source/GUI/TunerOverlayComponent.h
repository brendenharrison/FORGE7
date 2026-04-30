#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Audio/TunerPitchAnalyzer.h"

namespace forge7
{

struct AppContext;

/** Fullscreen monophonic guitar tuner: raw pre-FX input, large type, optional output mute from settings. */
class TunerOverlayComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    TunerOverlayComponent(AppContext& context, std::function<void()> closeHandler);
    ~TunerOverlayComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void visibilityChanged() override;

private:
    void timerCallback() override;

    AppContext& appContext;
    std::function<void()> onRequestClose;

    juce::TextButton closeButton { "Close" };
    juce::Label titleLabel;
    juce::Label noteNameLabel;
    juce::Label octaveLabel;
    juce::Label centsLabel;
    juce::Label statusLabel;
    juce::Label footnoteLabel;

    TunerState lastState;

    std::vector<float> analysisScratch;

    juce::Rectangle<int> needleMeterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerOverlayComponent)
};

} // namespace forge7
