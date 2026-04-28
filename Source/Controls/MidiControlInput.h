#pragma once

#include <memory>

#include <juce_audio_devices/juce_audio_devices.h>

#include "MidiHardwareMapping.h"

namespace forge7
{

class ControlManager;

/** MIDI → `HardwareControlEvent` using `DevelopmentMidiMapping` (editable defaults).

    **Threading**
    - `handleIncomingMidiMessage` runs on the JUCE MIDI thread — **never** touch plugin parameters here.
      Only call `ControlManager::submitHardwareEvent`, which marshals work to the message thread.
    - `openDevice…` / `setDevelopmentMapping` — call from the **message thread** (GUI / startup).

    **Device selection**
    - `getAvailableInputDevices()` — enumerate inputs for UI.
    - `openDeviceAtIndex` / `openDeviceWithIdentifier` — select active port; closes the previous input. */
class MidiControlInput final : public juce::MidiInputCallback
{
public:
    explicit MidiControlInput(ControlManager& owner);
    ~MidiControlInput() override;

    static juce::Array<juce::MidiDeviceInfo> getAvailableInputDevices();

    /** Convenience: opens first enumerated device (no-op if list empty). */
    void attachToAvailableInput();

    bool openDeviceAtIndex(int deviceIndex);
    bool openDeviceWithIdentifier(const juce::String& deviceIdentifier);

    void closeCurrentDevice();

    /** Identifier string from `MidiDeviceInfo`, or empty if closed. */
    juce::String getCurrentDeviceIdentifier() const;

    /** Index into `getAvailableInputDevices()` or -1 if closed / unknown. */
    int getCurrentDeviceIndex() const;

    DevelopmentMidiMapping getDevelopmentMapping() const;
    void setDevelopmentMapping(const DevelopmentMidiMapping& mapping);

    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

private:
    void stopAndReleaseDevice() noexcept;

    static float ccValueToNormalized(int value7) noexcept;
    static float relativeCcToDelta(int value7) noexcept;

    ControlManager& controlManager;
    std::unique_ptr<juce::MidiInput> midiInput;

    mutable juce::CriticalSection mappingLock;
    DevelopmentMidiMapping developmentMapping {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiControlInput)
};

} // namespace forge7
