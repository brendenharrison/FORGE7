#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppContext.h"
#include "AppConfig.h"
#include "ProjectSession.h"

namespace forge7
{

class AudioEngine;
class SceneManager;
class PluginHostManager;
class ControlManager;
class ProjectSerializer;
class ParameterMappingManager;
class DevToolsWindow;
/** JUCE application entry point for FORGE 7. Responsible for process lifetime and for
    constructing the non-RT subsystems in a safe order (logger, plugin host, scenes,
    audio engine, controls, main window). Does not perform audio processing itself.

    Interacts via AppContext so UI reaches SceneManager, PluginHostManager, etc.,
    without domain code depending on JUCEApplication. DocumentWindow owns MainComponent. */
class ForgeApplication : public juce::JUCEApplication
{
public:
    ForgeApplication() = default;

    const juce::String getApplicationName() override { return "FORGE 7"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& commandLineParameters) override;
    void shutdown() override;
    void systemRequestedQuit() override;

    /** Desktop dev-only: show/raise the simulated hardware window (no-op if disabled at compile time). */
    void showSimulatedHardwareWindow();

private:
    AppContext appContext {};
    std::unique_ptr<PluginHostManager> pluginHostManager;
    std::unique_ptr<SceneManager> sceneManager;
    std::unique_ptr<ParameterMappingManager> parameterMappingManager;
    std::unique_ptr<AudioEngine> audioEngine;
    std::unique_ptr<ControlManager> controlManager;
    juce::String forgeProjectTitle { "Untitled Project" };
    std::unique_ptr<AppConfig> appConfig;
    std::unique_ptr<ProjectSession> projectSession;
    std::unique_ptr<ProjectSerializer> projectSerializer;
    /** Top-level frame; owns the root MainComponent via setContentOwned. */
    std::unique_ptr<juce::DocumentWindow> mainWindow;

    /** Desktop dev-only simulated hardware panel - see `FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW` + AppConfig. */
    std::unique_ptr<DevToolsWindow> devToolsWindow;
};

} // namespace forge7
