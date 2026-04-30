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
class SimulatedControlsComponent;
class SettingsComponent;
class ProjectSceneBrowserComponent;
class ChainChordDetector;
class TunerOverlayComponent;

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

    /** Switches rack vs performance layouts; future hook for hardware "mode" button. */
    void setEditMode(bool shouldShowRackEditor);

    void openFullscreenPluginEditor(int slotIndex);
    void closeFullscreenPluginEditor();
    void closeFullscreenPluginEditorIfShowingSlot(int slotIndex);
    bool isFullscreenPluginEditorActive() const noexcept;

    RackViewComponent* getRackView() noexcept { return rackView.get(); }

    /** After chain/scene/project-state changes: Performance HUD + Rack slots/mappings. Message thread. */
    void refreshProjectDependentViews();

    /** Simulated hardware dev panel shortcuts (message thread). */
    void openPluginBrowserFromDevTools();
    void focusPluginInspectorFromDevTools();

    juce::String describeUiSurfaceForDevTools() const;

    void openSettings();
    void closeSettings();
    bool isSettingsOpen() const noexcept { return settingsComponent != nullptr; }

    bool isProjectSceneJumpBrowserOpen() const noexcept;

    /** Chain - / + from Performance / Rack / tests: same chord window as hardware. */
    void handleChainPreviousFromUi();
    void handleChainNextFromUi();

    void toggleTunerOverlay();
    void showTunerOverlay();
    void hideTunerOverlay();
    bool isTunerOverlayVisible() const noexcept;

    /** True when a fullscreen or blocking modal should own encoder focus (not base Performance/Rack). */
    bool isModalOverlayOpen() const noexcept;

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    /** In-app fallback for simulated hardware (always visible inside the main window). */
    void toggleSimulatedControlsPanel();
    void showSimulatedControlsPanel();
    void hideSimulatedControlsPanel();
    bool isSimulatedControlsPanelVisible() const noexcept { return simulatedControlsPanelVisible; }
#endif

private:
    AppContext& appContext;
    bool editMode = false;

    EncoderNavigator encoderNavigator;
    std::unique_ptr<PerformanceViewComponent> performanceView;
    std::unique_ptr<RackViewComponent> rackView;
    std::unique_ptr<KeyboardHardwareSimulator> keyboardHardwareSimulator;
    std::unique_ptr<FullscreenPluginEditorComponent> fullscreenPluginEditor;
    std::unique_ptr<SettingsComponent> settingsComponent;
    bool settingsReturnToEditMode = false;

    std::unique_ptr<ProjectSceneBrowserComponent> projectSceneJumpBrowser;

    std::unique_ptr<ChainChordDetector> chainChordDetector;
    std::unique_ptr<TunerOverlayComponent> tunerOverlay;

    bool handleGlobalEncoderLongPress();
    void restoreBaseFocus();
    void showProjectSceneJumpBrowser();
    void closeProjectSceneJumpBrowser();

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    class SimHwDrawer final : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;
    };

    SimHwDrawer simulatedControlsDrawer;
    juce::Label simulatedControlsTitleLabel;
    juce::TextButton simulatedControlsHideButton { "Hide" };
    std::unique_ptr<SimulatedControlsComponent> simulatedControlsPanel;
    std::unique_ptr<juce::Viewport> simulatedControlsViewport;
    bool simulatedControlsPanelVisible = false;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace forge7
