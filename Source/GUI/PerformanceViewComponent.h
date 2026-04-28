#pragma once

#include <array>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "AudioHealthMonitor.h"
#include "CpuMeter.h"
#include "LevelMeter.h"

namespace forge7
{

struct AppContext;

/** Live performance screen: large scene context, four hardware knob cards, chain/scene status,

    assign buttons, scene strip, and top status (BPM, CPU, meters). Dark, high-contrast, touch-friendly. */
class PerformanceViewComponent final : public juce::Component,
                                       private juce::Timer
{
public:
    explicit PerformanceViewComponent(AppContext& context);
    ~PerformanceViewComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Rebuild hardware encoder focus ring targets (scene strip, knob cards, toolbar). */
    void syncEncoderFocus();

private:
    void timerCallback() override;
    void refreshHud();

    AppContext& appContext;

    juce::TextButton tunerButton { "Tuner" };
    juce::TextButton tempoButton { "Tempo" };
    juce::TextButton setlistButton { "Setlist" };
    juce::TextButton settingsButton { "Settings" };

    juce::Label bpmStatusLabel;
    CpuMeter cpuMeter;
    LevelMeter inputLevelMeter;
    LevelMeter outputLevelMeter;

    juce::Label songTitleLabel;
    juce::Label heroSceneLabel;
    juce::Label sceneDetailLabel;
    juce::Label variationLabel;

    class KnobCard;
    std::array<std::unique_ptr<KnobCard>, 4> knobCards;

    juce::Label assign1TitleLabel;
    juce::Label assign1FunctionLabel;
    juce::Label assign2TitleLabel;
    juce::Label assign2FunctionLabel;

    juce::Label chainStatusLabel;

    static constexpr int kMaxSceneListRows = 5;
    juce::Label scenesSectionLabel;
    std::array<juce::Label, kMaxSceneListRows> sceneListLabels;

    AudioHealthMonitor audioHealthMonitor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceViewComponent)
};

} // namespace forge7
