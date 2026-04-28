#pragma once

namespace forge7
{

// Forward declarations for the application service graph. ForgeApplication owns the
// concrete instances; MainComponent and other UI receive a non-owning AppContext.

class AudioEngine;
class SceneManager;
class PluginHostManager;
class ControlManager;
class ProjectSerializer;
class ParameterMappingManager;
class AppConfig;
class EncoderNavigator;
class MainComponent;

/** Non-owning bundle of core subsystems. Created once in ForgeApplication and passed
    to MainComponent so the GUI can dispatch editing, performance mode, and file I/O
    without duplicating ownership. Audio and control paths remain distinct: only code
    invoked from AudioEngine's callback may touch real-time plugin processing. */
struct AppContext
{
    AudioEngine* audioEngine = nullptr;
    SceneManager* sceneManager = nullptr;
    PluginHostManager* pluginHostManager = nullptr;
    ControlManager* controlManager = nullptr;
    ProjectSerializer* projectSerializer = nullptr;
    ParameterMappingManager* parameterMappingManager = nullptr;
    AppConfig* appConfig = nullptr;

    /** Full-window encoder focus overlay; optional until MainComponent constructs. */
    EncoderNavigator* encoderNavigator = nullptr;

    /** Root shell — set by `MainComponent` for dev tools / simulated hardware UI. Message thread only. */
    MainComponent* mainComponent = nullptr;
};

} // namespace forge7
