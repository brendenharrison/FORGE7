#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Controls/HardwareControlTypes.h"

namespace forge7
{

struct AppContext;

/** Development-only simulated floor hardware (K1–K4, assigns, chain, encoder, shortcuts).

    All control actions call `ControlManager::submitHardwareEvent` — never touches plugins directly.

    Threading: message thread only. */
class SimulatedControlsComponent final : public juce::Component,
                                          private juce::Slider::Listener,
                                          private juce::Timer
{
public:
    explicit SimulatedControlsComponent(AppContext& context);
    ~SimulatedControlsComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void sliderValueChanged(juce::Slider* slider) override;

    void emitKnob(HardwareControlId id, float normalized01);
    void emitEncoderDelta(int delta);
    void emitEncoderPress(HardwareControlId pressOrLong);

    AppContext& appContext;

    std::array<juce::Slider, 4> knobs {};
    std::array<juce::Label, 4> knobValueLabels {};

    juce::TextButton assign1Button { "Assign 1" };
    juce::TextButton assign2Button { "Assign 2" };

    juce::TextButton chainPrevButton { "Chain -" };
    juce::TextButton chainNextButton { "Chain +" };

    juce::Label encoderSectionLabel { {}, "Main Encoder" };
    juce::TextButton encoderLeftButton { "Rotate Left" };
    juce::TextButton encoderRightButton { "Rotate Right" };
    juce::TextButton encoderPressButton { "Press" };
    juce::TextButton encoderLongPressButton { "Long Press" };

    juce::Label shortcutsHeading { {}, "Touchscreen Test Shortcuts" };
    juce::TextButton shortcutEditMode { "Edit Mode" };
    juce::TextButton shortcutPerfMode { "Performance Mode" };
    juce::TextButton shortcutPluginBrowser { "Open Plugin Browser" };
    juce::TextButton shortcutInspector { "Open Inspector (selected)" };
    juce::TextButton shortcutSave { "Save Project" };
    juce::TextButton shortcutLoad { "Load Project" };

    juce::Label debugHeading { {}, "Status" };
    juce::Label sceneLabel;
    juce::Label variationLabel;
    juce::Label lastEventLabel;
    juce::Label knobSummaryLabel;
    juce::Label encoderFocusLabel;
    juce::Label slotLabel;
    juce::Label uiSurfaceLabel;

    HardwareControlId lastEmittedId { HardwareControlId::Knob1 };
    float lastEmittedValue { 0.0f };
    juce::String lastEmittedExtra;

    void wireButtons();
    void refreshDebugLabels();

    void emitAssignPressed(int assignIndex);
    void emitAssignReleased(int assignIndex);

    struct AssignMouseBridge final : public juce::MouseListener
    {
        SimulatedControlsComponent* owner { nullptr };
        int assignIndex { 1 };

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
    };

    AssignMouseBridge assign1Bridge;
    AssignMouseBridge assign2Bridge;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimulatedControlsComponent)
};

} // namespace forge7
