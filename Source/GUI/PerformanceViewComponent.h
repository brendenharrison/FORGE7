#pragma once

#include <array>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "AudioHealthMonitor.h"
#include "CpuMeter.h"

namespace forge7
{

struct AppContext;

/** Live performance screen: scene + chain variation, K1-K4, assigns, variation navigation,

    BPM/CPU - tuned for ~7\" embedded pedal use (minimal chrome). */
class PerformanceViewComponent final : public juce::Component,
                                       private juce::Timer
{
public:
    explicit PerformanceViewComponent(AppContext& context);
    ~PerformanceViewComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Rebuild hardware encoder focus ring targets. */
    void syncEncoderFocus();

private:
    void timerCallback() override;
    void refreshHud();

    AppContext& appContext;

    juce::TextButton rackEditButton { "RACK" };
    juce::TextButton chainPrevButton { "Var -" };
    juce::TextButton chainNextButton { "Var +" };
    juce::TextButton settingsButton { "Settings" };

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    juce::TextButton simHwButton { "Sim HW" };
    juce::Label simHwHintLabel;
#endif

    juce::Label bpmStatusLabel;
    CpuMeter cpuMeter;

    juce::Label heroSceneLabel;
    juce::Label variationLabel;
    juce::Label chainVarIndexLabel;

    class KnobCard;
    std::array<std::unique_ptr<KnobCard>, 4> knobCards;

    juce::Label assign1TitleLabel;
    juce::Label assign1FunctionLabel;
    juce::Label assign2TitleLabel;
    juce::Label assign2FunctionLabel;

    AudioHealthMonitor audioHealthMonitor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceViewComponent)
};

} // namespace forge7
