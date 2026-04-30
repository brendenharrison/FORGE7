#pragma once

#include <array>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "AudioHealthMonitor.h"
#include "CpuMeter.h"
#include "VuMeterComponent.h"

namespace forge7
{

struct AppContext;

/** Live performance screen: shows Project / Scene / Chain hierarchy, K1-K4, Button 1-2,

    Chain - / Chain + navigation, BPM/CPU - tuned for ~7\" embedded pedal use. */
class PerformanceViewComponent final : public juce::Component,
                                       private juce::Timer
{
public:
    explicit PerformanceViewComponent(AppContext& context);
    ~PerformanceViewComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Rebuild hardware encoder focus ring targets. */
    void syncEncoderFocus(bool resetToFirst = false);

    /** Full HUD refresh (project/scene/chain + knobs); safe from `MainComponent`. */
    void refreshHud();

private:
    void timerCallback() override;

    AppContext& appContext;

    juce::TextButton rackEditButton { "RACK" };
    juce::TextButton scenePrevButton { "Scene -" };
    juce::TextButton sceneNextButton { "Scene +" };
    juce::Label sceneCountLabel;
    juce::TextButton chainPrevButton { "Chain -" };
    juce::TextButton chainNextButton { "Chain +" };
    juce::TextButton settingsButton { "Settings" };

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    juce::TextButton simHwButton { "Sim HW" };
    juce::Label simHwHintLabel;
#endif

    juce::Label bpmStatusLabel;
    CpuMeter cpuMeter;

    juce::Label projectNameLabel;
    juce::Label projectDirtyLabel;
    juce::Label heroSceneLabel;
    juce::Label chainHeaderLabel;
    juce::Label chainCountLabel;

    class KnobCard;
    std::array<std::unique_ptr<KnobCard>, 4> knobCards;

    std::unique_ptr<juce::Component> assignButton1Led;
    std::unique_ptr<juce::Component> assignButton2Led;

    juce::Label assign1TitleLabel;
    juce::Label assign1FunctionLabel;
    juce::Label assign2TitleLabel;
    juce::Label assign2FunctionLabel;

    AudioHealthMonitor audioHealthMonitor;

    /** Main input / output peaks only (no per-plugin meters in Performance mode). */
    std::unique_ptr<VuMeterComponent> perfInputVuMeter;
    std::unique_ptr<VuMeterComponent> perfOutputVuMeter;

    /** When scene|chain navigation changes, knob mapping debug logs once (not timer-spammed). */
    juce::String lastAssignablesHudLogKey;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceViewComponent)
};

} // namespace forge7
