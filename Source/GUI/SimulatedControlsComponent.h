#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Controls/HardwareControlTypes.h"

namespace forge7
{

struct AppContext;

/** Development-only simulated floor hardware (K1-K4, assigns, chain, encoder, shortcuts).

    All control actions call `ControlManager::submitHardwareEvent` - never touches plugins directly.

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

    void emitKnobAbsoluteForDevTools(HardwareControlId id, float normalized01);

    /** `deltaNormalized` moves the mapped plugin parameter in normalized 0...1 space (typ. +/-0.01). */
    void emitKnobRelativeDelta(const HardwareControlId id, float pluginNormalizedDelta);

    void refreshAssignableKnobDisplaysFromPluginState();

    void emitEncoderDelta(int delta);
    void emitEncoderPress(HardwareControlId pressOrLong);

    AppContext& appContext;

    std::array<juce::Slider, 4> knobs {};
    std::array<juce::Label, 4> knobRelLabels {};
    std::array<juce::Label, 4> knobValueLabels {};
    std::array<juce::TextButton, 4> knobDownButtons {};
    std::array<juce::TextButton, 4> knobUpButtons {};

    juce::TextButton assign1Button { "Button 1" };
    juce::TextButton assign2Button { "Button 2" };

    juce::TextButton chainPrevButton { "Chain -" };
    juce::TextButton chainNextButton { "Chain +" };
    juce::TextButton tunerToggleButton { "Toggle Tuner" };

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
    juce::TextButton shortcutExportProject { "Export Project..." };
    juce::TextButton shortcutImportProject { "Import Project..." };
    juce::Label shortcutLibraryStatusLabel;

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
