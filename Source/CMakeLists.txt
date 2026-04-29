# Lists all FORGE 7 translation units for the parent CMakeLists (PARENT_SCOPE).
set(FORGE7_SOURCES
    App/ForgeApplication.cpp
    App/MainComponent.cpp
    App/ProjectSession.cpp
    App/AppConfig.cpp
    App/DevToolsWindow.cpp

    Audio/AudioEngine.cpp
    Audio/CrossfadeMixer.cpp

    PluginHost/PluginHostManager.cpp
    PluginHost/PluginScanSkipStore.cpp
    PluginHost/PluginChain.cpp
    PluginHost/PluginSlot.cpp
    PluginHost/BridgedPluginManager.cpp

    Scene/SceneManager.cpp
    Scene/Scene.cpp
    Scene/ChainVariation.cpp

    Storage/ProjectSerializer.cpp
    Storage/ForgeStoragePaths.cpp

    Controls/ControlManager.cpp
    Controls/HardwareControlState.cpp
    Controls/MidiHardwareMapping.cpp
    Controls/KeyboardHardwareSimulator.cpp
    Controls/MidiControlInput.cpp
    Controls/UsbSerialHardwareBridge.cpp
    Controls/EncoderNavigator.cpp
    Controls/ParameterMappingDescriptor.cpp
    Controls/ParameterMappingManager.cpp

    GUI/RackViewComponent.cpp
    GUI/SimulatedControlsComponent.cpp
    GUI/ProjectLibraryDialogs.cpp
    GUI/UnsavedChangesModal.cpp
    GUI/NameEntryModal.cpp
    GUI/NavigationStatus.cpp
    GUI/FullscreenPluginEditorComponent.cpp
    GUI/PluginEditorCanvas.cpp
    GUI/RackSlotCard.cpp
    GUI/PerformanceViewComponent.cpp
    GUI/PluginBrowserComponent.cpp
    GUI/PluginInspectorComponent.cpp
    GUI/PluginEditorHostWindow.cpp
    GUI/SettingsComponent.cpp
    GUI/AudioHealthMonitor.cpp
    GUI/CpuMeter.cpp
    GUI/LevelMeter.cpp

    Utilities/Logger.cpp
)

if(APPLE)
    list(APPEND FORGE7_SOURCES Platform/MacAudioPermission.mm)
else()
    list(APPEND FORGE7_SOURCES Platform/MacAudioPermission.cpp)
endif()

set(FORGE7_SOURCES ${FORGE7_SOURCES} PARENT_SCOPE)
