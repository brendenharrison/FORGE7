#pragma once

#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Controls/ParameterMappingDescriptor.h"

#include "PluginEditorCanvas.h"
#include "PluginEditorViewportFrame.h"

namespace juce
{
class AudioProcessorEditor;
class AudioPluginInstance;
}

namespace forge7
{

struct AppContext;
class CpuMeter;

/** In-app fullscreen plugin UI for embedded pedal UX (not a separate OS window).

    Shows `AudioProcessorEditor` when available, otherwise `GenericAudioProcessorEditor`.

    Assignment Mode: pick a parameter from the list, then twist K1-K4 - `ParameterMappingManager`
    binds on first knob delta (see `prepareKnobAssignmentToNextHardwareMove`).

    Plugin UI is hosted inside `PluginEditorCanvas` inside `PluginEditorViewportFrame` so oversized
    / native editors stay clipped to the central viewport and do not overlap FORGE chrome. */
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
    void refreshPanControlsFromCanvas();

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

    void bringChromeToFront();
    /** @return true if an editor was created and attached in this call. */
    bool tryAttachEmbeddedEditorIfNeeded();
    void logPluginEditorLayoutDiagnosticsIfChanged();
    void scheduleDeferredEditorHostReconcile();
    void performDeferredEditorHostReconcile();

    struct DeferredEditorHostReconciler;
    std::unique_ptr<DeferredEditorHostReconciler> deferredEditorHostReconciler;

    AppContext& appContext;
    const int slotIndex;

    std::function<void()> onClose;

    juce::Label titleLabel;
    juce::Label sceneVarLabel;
    juce::TextButton backButton { "Back" };
    juce::TextButton closeButton { "Close" };
    juce::ToggleButton assignModeToggle { "Assign" };
    std::unique_ptr<CpuMeter> cpuMeter;

    juce::Slider panXSlider;
    juce::Slider panYSlider;

    /** Owned first so destruction clears canvas -> releases hosted editor before unique_ptr resets. */
    std::unique_ptr<juce::AudioProcessorEditor> embeddedEditor;

    juce::AudioPluginInstance* hostedInstanceForEditor = nullptr;

    PluginEditorViewportFrame pluginViewportFrame;
    PluginEditorCanvas pluginEditorCanvas;

    ChromeBackgroundBand headerChromeBg;
    ChromeBackgroundBand footerChromeBg;

    juce::Rectangle<int> lastPluginLayoutDiagnosticBounds;

    juce::Label assignHintLabel;
    juce::ListBox parameterList;

    std::array<juce::Label, 4> knobMappingLabels {};
    std::array<juce::Label, 2> assignMappingLabels {};

    juce::Array<AutomatableParameterSummary> parameterRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FullscreenPluginEditorComponent)
};

} // namespace forge7
