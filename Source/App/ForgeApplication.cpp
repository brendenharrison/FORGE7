#include <JuceHeader.h>

#include "ForgeApplication.h"
#include "AppConfig.h"
#include "DevToolsWindow.h"
#include "MainComponent.h"

#include "../Audio/AudioEngine.h"
#include "../Controls/ControlManager.h"
#include "../Controls/ParameterMappingManager.h"
#include "../PluginHost/PluginHostManager.h"
#include "../Scene/SceneManager.h"
#include "../Storage/ProjectSerializer.h"
#include "../Utilities/Logger.h"
#include "../Utilities/DebugSessionLog.h"

// -----------------------------------------------------------------------------
/** Simulated hardware (`DevToolsWindow`): development-only floating panel.
 *
 * Compile-time:
 * - Define FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW=0 when building embedded/Linux release
 *   artefacts that must not ship this UI (or omit DevTools sources from the target).
 *
 * Runtime (`forge7_config.json`):
 * - `showSimulatedControls` (default true from `AppConfig::seedDefaults`) opens the panel at startup
 *   when the compile-time flag above is enabled. Set to false to disable without recompiling.
 *
 * TODO: consolidate with a single FORGE7_PRODUCT_BUILD CMake option for Linux kiosk.
 */
#ifndef FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
#define FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW 1
#endif

namespace forge7
{
namespace
{
/** Thin DocumentWindow shell that forwards close to application quit. */
class ForgeMainWindow final : public juce::DocumentWindow
{
public:
    ForgeMainWindow(const juce::String& name, std::unique_ptr<juce::Component> content)
        : DocumentWindow(
              name,
              juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
              DocumentWindow::allButtons,
              true)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(content.release(), true);
        setResizable(true, true);
        centreWithSize(1024, 600);
        setAlwaysOnTop(true);
        setAlwaysOnTop(false);
        setVisible(true);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ForgeMainWindow)
};
} // namespace

void ForgeApplication::initialise(const juce::String& commandLineParameters)
{
    // #region agent log
    {
        auto* d = new juce::DynamicObject();
        d->setProperty("argsLen", commandLineParameters.length());
        debugAgentLog("H1", "ForgeApplication::initialise", "entry", juce::var(d));
    }
    // #endregion

    Logger::info("Starting FORGE 7 core services (stub). Args: " + commandLineParameters);

    appConfig = std::make_unique<AppConfig>();

    if (appConfig->loadOrCreateDefaults())
        Logger::info("FORGE7: config - " + AppConfig::getDefaultConfigFile().getFullPathName());

    pluginHostManager = std::make_unique<PluginHostManager>();
    sceneManager = std::make_unique<SceneManager>();
    parameterMappingManager = std::make_unique<ParameterMappingManager>(*sceneManager, *pluginHostManager);
    audioEngine = std::make_unique<AudioEngine>(*pluginHostManager);
    audioEngine->initialiseAudioDevice();

    controlManager = std::make_unique<ControlManager>(*parameterMappingManager);
    projectSerializer = std::make_unique<ProjectSerializer>(*sceneManager,
                                                               *parameterMappingManager,
                                                               forgeProjectTitle);

    appContext.audioEngine = audioEngine.get();
    appContext.sceneManager = sceneManager.get();
    appContext.pluginHostManager = pluginHostManager.get();
    appContext.controlManager = controlManager.get();

    if (controlManager != nullptr)
        controlManager->attachSceneNavigation(sceneManager.get(), pluginHostManager.get());
    appContext.projectSerializer = projectSerializer.get();
    appContext.parameterMappingManager = parameterMappingManager.get();
    appContext.appConfig = appConfig.get();

    mainWindow = std::make_unique<ForgeMainWindow>("FORGE 7", std::make_unique<MainComponent>(appContext));

#if JUCE_MAC
    /** Activate app before creating a second native window (helps macOS show the dev tools panel when launched via `open`). */
    juce::Process::makeForegroundProcess();
#endif

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    /** Always open during desktop dev when this macro is on. (`showSimulatedControls` in JSON may still be used
        later for embedded routing; hiding the panel via config alone proved easy to strand at false.) */
    // #region agent log
    {
        auto* dm = new juce::DynamicObject();
        dm->setProperty("FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW", FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW);
        debugAgentLog("H2", "ForgeApplication.cpp", "before_devtools_create", juce::var(dm));
    }
    // #endregion

    devToolsWindow = std::make_unique<DevToolsWindow>(appContext);

    if (mainWindow != nullptr && devToolsWindow != nullptr)
    {
        devToolsWindow->centreAroundComponent(mainWindow.get(),
                                              devToolsWindow->getWidth(),
                                              devToolsWindow->getHeight());
        devToolsWindow->toFront(true);
    }

    // #region agent log
    if (devToolsWindow != nullptr)
    {
        auto* st = new juce::DynamicObject();
        st->setProperty("isVisible", devToolsWindow->isVisible());
        st->setProperty("bounds", devToolsWindow->getBounds().toString());
        st->setProperty("isAlwaysOnTop", devToolsWindow->isAlwaysOnTop());
        st->setProperty("peer", devToolsWindow->getPeer() != nullptr);
        debugAgentLog("H3", "ForgeApplication.cpp", "after_devtools_create", juce::var(st));
        Logger::info("[DBG] devTools post-create visible=" + juce::String(devToolsWindow->isVisible() ? "yes" : "no")
                     + " peer=" + juce::String(devToolsWindow->getPeer() != nullptr ? "yes" : "no") + " bounds="
                     + devToolsWindow->getBounds().toString());
    }
    else
    {
        debugAgentLog("H3", "ForgeApplication.cpp", "devtools_null", juce::var());
    }
    // #endregion

    const bool cfgWantsSim = appConfig != nullptr && appConfig->getShowSimulatedControls();
    Logger::info("FORGE7: simulated hardware window opened (Forge7 - Simulated Hardware). "
                 "showSimulatedControls in config="
                 + juce::String(cfgWantsSim ? "true" : "false")
                 + " - compile with FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW=0 to omit the panel entirely.");
#else
    // #region agent log
    debugAgentLog("H2", "ForgeApplication.cpp", "compile_time_devtools_disabled", juce::var(0));
    // #endregion
    Logger::info("FORGE7: simulated hardware window omitted at compile time (FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW=0).");
#endif

    mainWindow->toFront(true);

#if FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
    if (devToolsWindow != nullptr)
        devToolsWindow->toFront(true);
#endif

    juce::MessageManager::callAsync([mainWin = mainWindow.get(), devWin = devToolsWindow.get()]
                                   {
                                       if (mainWin != nullptr)
                                           mainWin->toFront(true);

                                       /** Main window grabbed focus above; raise dev tools again (or use AlwaysOnTop). */
                                       if (devWin != nullptr)
                                       {
                                           devWin->toFront(true);
                                           // #region agent log
                                           {
                                               auto* st = new juce::DynamicObject();
                                               st->setProperty("devBounds", devWin->getBounds().toString());
                                               st->setProperty("devVisible", devWin->isVisible());
                                               st->setProperty("devIsFront", devWin->isAlwaysOnTop());
                                               if (mainWin != nullptr)
                                                   st->setProperty("mainBounds", mainWin->getBounds().toString());
                                               debugAgentLog("H5", "ForgeApplication.cpp", "async_after_dev_toFront",
                                                             juce::var(st));
                                           }
                                           // #endregion
                                       }

                                       if (mainWin != nullptr)
                                           if (auto* c = mainWin->getContentComponent())
                                               c->grabKeyboardFocus();
                                   });
}

void ForgeApplication::shutdown()
{
    appContext.appConfig = nullptr;
    devToolsWindow.reset();
    mainWindow.reset();
    projectSerializer.reset();
    controlManager.reset();
    audioEngine.reset();
    parameterMappingManager.reset();
    sceneManager.reset();
    pluginHostManager.reset();
    appConfig.reset();
}

void ForgeApplication::systemRequestedQuit()
{
    quit();
}

} // namespace forge7

/** Thin shell so START_JUCE_APPLICATION receives a global-scope class name (JUCE macro). */
class ForgeApplicationEntry final : public forge7::ForgeApplication {};

START_JUCE_APPLICATION(ForgeApplicationEntry)
