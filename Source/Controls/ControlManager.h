#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "HardwareControlState.h"
#include "HardwareControlTypes.h"

namespace forge7
{

class MidiControlInput;
class ParameterMappingManager;
class SceneManager;
class PluginHostManager;

/** Central hub: all physical inputs normalize into `HardwareControlEvent` and enter here.

    Transports (MIDI, USB serial stub, keyboard dev harness, future GPIO) call **`submitHardwareEvent`**
    only — **never** bypass this type for feature code so GUI and audio routing stay agnostic.

    Threading:
    - **submitHardwareEvent** may be called from MIDI callbacks, serial threads, or message thread.
    - State updates are lock-free (`HardwareControlState` atomics) + light routing under `controlLock`.
    - **Listeners** run on the **JUCE message thread** (async hop from non-message threads).

    AudioEngine does not use this class. */
class ControlManager
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;

        /** Knob movement after normalization (id is always Knob1…Knob4). */
        virtual void parameterControlChanged(HardwareControlId id, float normalizedValue) {}

        /** Assign buttons (pressed or released). */
        virtual void assignableButtonChanged(HardwareControlId assignButtonId, bool isPressed) {}

        virtual void chainVariationPreviousPressed() {}
        virtual void chainVariationNextPressed() {}

        /** Encoder: rotation delta, short press, long press — unified stream for navigation UX. */
        virtual void encoderNavigationEvent(const HardwareControlEvent& event) {}
    };

    explicit ControlManager(ParameterMappingManager& mappingManager);
    ~ControlManager();

    ParameterMappingManager& getParameterMappingManager() noexcept { return parameterMappingManager; }

    HardwareControlState& getHardwareState() noexcept { return hardwareState; }
    const HardwareControlState& getHardwareState() const noexcept { return hardwareState; }

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    /** Entry point for every transport — **the** normalization boundary. */
    void submitHardwareEvent(HardwareControlEvent event);

    /** Optional built-in routing: chain prev/next invoke scene variation stepping with crossfade.
        Set from app startup; omit for headless/tests. */
    void attachSceneNavigation(SceneManager* scenes, PluginHostManager* host) noexcept;

    MidiControlInput* getMidiControlInputForSetup() noexcept { return midiInput.get(); }

private:
    void applyEventToState(const HardwareControlEvent& e);
    void routeThroughMappingStub(const HardwareControlEvent& e);
    void invokeSceneNavigationIfAttached(const HardwareControlEvent& e);
    void notifyListeners(const HardwareControlEvent& e);

    ParameterMappingManager& parameterMappingManager;
    HardwareControlState hardwareState {};

    SceneManager* sceneManager = nullptr;
    PluginHostManager* pluginHostManager = nullptr;

    std::unique_ptr<MidiControlInput> midiInput;

    juce::ListenerList<Listener> listeners;

    mutable juce::CriticalSection controlLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlManager)
};

} // namespace forge7
