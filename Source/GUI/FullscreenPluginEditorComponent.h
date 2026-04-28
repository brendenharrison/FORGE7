#pragma once

#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Controls/ParameterMappingDescriptor.h"

#include "PluginEditorCanvas.h"

namespace juce
{
class AudioProcessorEditor;
}

namespace forge7
{

struct AppContext;
class CpuMeter;

/** In-app fullscreen plugin UI for embedded pedal UX (not a separate OS window).

    Shows `AudioProcessorEditor` when available, otherwise `GenericAudioProcessorEditor`.

    Assignment Mode: pick a parameter from the list, then twist K1–K4 — `ParameterMappingManager`
    binds on first knob delta (see `prepareKnobAssignmentToNextHardwareMove`).

    Plugin UI is hosted inside `PluginEditorCanvas` so oversized VST editors scale/pan within the
    central area without overlapping FORGE chrome. */
class FullscreenPluginEditorComponent final : public juce::Component,
                                               private juce::Timer,
                                               private juce::ListBoxModel
{
public:
    FullscreenPluginEditorComponent(AppContext& context,
                                   int pluginSlotIndex,
                                   std::function<void()> onCloseRequested);
    ~FullscreenPluginEditorComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    int getPluginSlotIndex() const noexcept { return slotIndex; }

    /** Rebuild encoder modal chain (call after layout / mode change). */
    void syncEncoderFocus();

private:
    void timerCallback() override;

    int getNumRows() override;
    void paintListBoxItem(int rowNumber,
                          juce::Graphics& g,
                          int width,
                          int height,
                          bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;

    void armParameterAssignmentForSelectedRow(int rowIndex);

    void rebuildParameterListModel();
    void refreshMappingStrip();

    AppContext& appContext;
    const int slotIndex;

    std::function<void()> onClose;

    juce::Label titleLabel;
    juce::Label sceneVarLabel;
    juce::TextButton backButton { "Back" };
    juce::TextButton closeButton { "Close" };
    juce::ToggleButton assignModeToggle { "Assign" };
    std::unique_ptr<CpuMeter> cpuMeter;

    juce::TextButton viewFitHeight { "Fit H" };
    juce::TextButton viewFitWidth { "Fit W" };
    juce::TextButton viewFitAll { "Fit All" };
    juce::TextButton viewActual100 { "100%" };

    /** Owned first so destruction clears canvas → releases hosted editor before unique_ptr resets. */
    std::unique_ptr<juce::AudioProcessorEditor> embeddedEditor;
    PluginEditorCanvas pluginEditorCanvas;

    juce::Label assignHintLabel;
    juce::ListBox parameterList { {}, this };

    std::array<juce::Label, 4> knobMappingLabels {};
    std::array<juce::Label, 2> assignMappingLabels {};

    juce::Array<AutomatableParameterSummary> parameterRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FullscreenPluginEditorComponent)
};

} // namespace forge7
