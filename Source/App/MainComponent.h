#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Controls/EncoderNavigator.h"

namespace forge7
{

struct AppContext;
class PerformanceViewComponent;
class RackViewComponent;
class KeyboardHardwareSimulator;
class FullscreenPluginEditorComponent;

/** Root GUI shell for the standalone app. Switches between Performance Mode (hands-free,
    touch-friendly) and Edit Mode (rack builder). Owns EncoderNavigator for the large
    encoder-driven focus model and delegates project/scene edits to ProjectSerializer /
    SceneManager via AppContext.

    Threading: all methods run on the message thread only; never calls plugin processBlock. */
class MainComponent final : public juce::Component
{
public:
    explicit MainComponent(AppContext& context);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Switches rack vs performance layouts; future hook for hardware “mode” button. */
    void setEditMode(bool shouldShowRackEditor);

    void openFullscreenPluginEditor(int slotIndex);
    void closeFullscreenPluginEditor();
    void closeFullscreenPluginEditorIfShowingSlot(int slotIndex);
    bool isFullscreenPluginEditorActive() const noexcept;

    RackViewComponent* getRackView() noexcept { return rackView.get(); }

    /** Simulated hardware dev panel shortcuts (message thread). */
    void openPluginBrowserFromDevTools();
    void focusPluginInspectorFromDevTools();

    juce::String describeUiSurfaceForDevTools() const;

private:
    AppContext& appContext;
    bool editMode = false;

    EncoderNavigator encoderNavigator;
    std::unique_ptr<PerformanceViewComponent> performanceView;
    std::unique_ptr<RackViewComponent> rackView;
    std::unique_ptr<KeyboardHardwareSimulator> keyboardHardwareSimulator;
    std::unique_ptr<FullscreenPluginEditorComponent> fullscreenPluginEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace forge7
